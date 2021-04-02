#include "BlendDemo/BlendApp.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd) {
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try	{
		BlendApp theApp(hInstance);
		if (!theApp.Initialize())
			return 0;

		return theApp.Run();
	}
	catch (DxException& e) {
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
		return 0;
	}
}

bool BlendApp::Initialize() {
	if (!D3DApp::Initialize())
		return false;

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	// Get the increment size of a descriptor in this heap type.  This is hardware specific, 
	// so we have to query this information.
	mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	mWaves = std::make_unique<Waves>(128, 128, 1.0f, 0.03f, 4.0f, 0.2f);

	LoadTextures();
	BuildRootSignature();
	BuildDescriptorHeaps();
	BuildShadersAndInputLayout();
	BuildLandGeometry();
	BuildWavesGeometry();
	BuildBoxGeometry();
#if defined(Ex8)
	BuildQuadGeometry();
#endif
	BuildMaterials();
	BuildRenderItems();
	BuildFrameResources();
	BuildPSOs();

	// Execute the initialization commands.
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	FlushCommandQueue();

	return true;
}

BlendApp::BlendApp(HINSTANCE hInstance) 
	: D3DApp(hInstance) {}

BlendApp::~BlendApp() {
	if (md3dDevice != nullptr)
		FlushCommandQueue();
}

void BlendApp::OnResize() {
	D3DApp::OnResize();

	XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f*MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
	XMStoreFloat4x4(&mProj, P);
}

void BlendApp::Update(const GameTimer& gt) {
	OnKeyboardInput(gt);
	UpdateCamera(gt);

	// Cycle through the circular frame resource array.
	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	// Has the GPU finished processing the commands of the current frame resource?
	// If not, wait until the GPU has completed commands up to this fence point.
	if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	AnimateMaterials(gt);
	UpdateObjectCBs(gt);
	UpdateMaterialCBs(gt);
	UpdateMainPassCB(gt);
	UpdateWaves(gt);
}

void BlendApp::Draw(const GameTimer& gt) {
	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

	// Reuse the memory associated with command recording.
	// We can only reset when the associated command lists have finished execution on the GPU.
	ThrowIfFailed(cmdListAlloc->Reset());

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
#if defined(Ex2)
	ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["transparent"].Get()));
#elif defined(Ex8)
	ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["depth"].Get()));
#elif defined(Ex9)
	ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["addition"].Get()));
#else
	ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));
#endif

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	// Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// Clear the back buffer and depth buffer.
	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), (float*)&mMainPassCB.FogColor, 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Specify the buffers we are going to render to.
	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());
	
	ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	auto passCB = mCurrFrameResource->PassCB->Resource();
	mCommandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

#if defined(Ex2)
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Transparent]);
#else
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);
#endif

#if defined(Ex2)
	mCommandList->SetPipelineState(mPSOs["opaque"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);
#else

#if !(defined(Ex8) || defined(Ex9))
	mCommandList->SetPipelineState(mPSOs["alphaTested"].Get());
#endif
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::AlphaTested]);

#if !(defined(Ex8) || defined(Ex9))
	mCommandList->SetPipelineState(mPSOs["transparent"].Get());
#endif
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Transparent]);
#endif

#if defined(Ex7)
	mCommandList->SetPipelineState(mPSOs["Bolt"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Exercise]);
#endif

#if defined(Ex8)
	UINT passCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstants));
	mCommandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress() + 1 * passCBByteSize);

	mCommandList->SetPipelineState(mPSOs["drawStencil"].Get());

	mCommandList->OMSetStencilRef(1);
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::DepthBlue]);

	mCommandList->OMSetStencilRef(2);
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::DepthGreen]);
	
	mCommandList->OMSetStencilRef(3);
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::DepthRed]);
#endif

	// Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	// Done recording commands.
	ThrowIfFailed(mCommandList->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Swap the back and front buffers
	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	// Advance the fence value to mark commands up to this fence point.
	mCurrFrameResource->Fence = static_cast<UINT>(++mCurrentFence);

	// Add an instruction to the command queue to set a new fence point. 
	// Because we are on the GPU timeline, the new fence point won't be 
	// set until the GPU finishes processing all the commands prior to this Signal().
	mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}
	 
void BlendApp::OnMouseDown(WPARAM btnState, int x, int y) {
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mhMainWnd);
}

void BlendApp::OnMouseUp(WPARAM btnState, int x, int y) {
	ReleaseCapture();
}

void BlendApp::OnMouseMove(WPARAM btnState, int x, int y) {
	if ((btnState & MK_LBUTTON) != 0) {
		// Make each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

		// Update angles based on input to orbit camera around box.
		mTheta += dx;
		mPhi += dy;

		// Restrict the angle mPhi.
		mPhi = MathHelper::Clamp(mPhi, 0.1f, MathHelper::Pi - 0.1f);
	}
	else if ((btnState & MK_RBUTTON) != 0) {
		// Make each pixel correspond to 0.2 unit in the scene.
		float dx = 0.2f * static_cast<float>(x - mLastMousePos.x);
		float dy = 0.2f * static_cast<float>(y - mLastMousePos.y);

		// Update the camera radius based on input.
		mRadius += dx - dy;

		// Restrict the radius.
		mRadius = MathHelper::Clamp(mRadius, 5.0f, 150.0f);
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void BlendApp::OnKeyboardInput(const GameTimer& gt) {}

void BlendApp::UpdateCamera(const GameTimer& gt) {
	// Convert Spherical to Cartesian coordinates.
	mEyePos.x = mRadius * sinf(mPhi) * cosf(mTheta);
	mEyePos.z = mRadius * sinf(mPhi) * sinf(mTheta);
	mEyePos.y = mRadius * cosf(mPhi);

	// Build the view matrix.
	XMVECTOR pos = XMVectorSet(mEyePos.x, mEyePos.y, mEyePos.z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&mView, view);
}

void BlendApp::AnimateMaterials(const GameTimer& gt) {
	// Scroll the water material texture coordinates.
	auto waterMat = mMaterials["water"].get();

	float& tu = waterMat->MatTransform(3, 0);
	float& tv = waterMat->MatTransform(3, 1);

	tu += 0.1f * gt.DeltaTime();
	tv += 0.02f * gt.DeltaTime();

	if (tu >= 1.0f)
		tu -= 1.0f;

	if (tv >= 1.0f)
		tv -= 1.0f;

	waterMat->MatTransform(3, 0) = tu;
	waterMat->MatTransform(3, 1) = tv;

	// Material has changed, so need to update cbuffer.
	waterMat->NumFramesDirty = gNumFrameResources;
}

void BlendApp::UpdateObjectCBs(const GameTimer& gt) {
	auto currObjectCB = mCurrFrameResource->ObjectCB.get();
	for (auto& e : mAllRitems) {
		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		if (e->NumFramesDirty > 0) {
			XMMATRIX world = XMLoadFloat4x4(&e->World);
			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));

			currObjectCB->CopyData(e->ObjCBIndex, objConstants);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;
		}
	}
}

void BlendApp::UpdateMaterialCBs(const GameTimer& gt) {
	auto currMaterialCB = mCurrFrameResource->MaterialCB.get();
	for (auto& e : mMaterials) {
		// Only update the cbuffer data if the constants have changed.  If the cbuffer
		// data changes, it needs to be updated for each FrameResource.
		Material* mat = e.second.get();
		if (mat->NumFramesDirty > 0) {
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialConstants matConstants;
			matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
			matConstants.FresnelR0 = mat->FresnelR0;
			matConstants.Roughness = mat->Roughness;
			XMStoreFloat4x4(&matConstants.MatTransform, XMMatrixTranspose(matTransform));

			currMaterialCB->CopyData(mat->MatCBIndex, matConstants);

			// Next FrameResource need to be updated too.
			mat->NumFramesDirty--;
		}
	}
}

void BlendApp::UpdateMainPassCB(const GameTimer& gt) {
	XMMATRIX view = XMLoadFloat4x4(&mView);
	XMMATRIX proj = XMLoadFloat4x4(&mProj);

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	mMainPassCB.EyePosW = mEyePos;
	mMainPassCB.RenderTargetSize = XMFLOAT2(static_cast<float>(mClientWidth), static_cast<float>(mClientHeight));
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.NearZ = 1.0f;
	mMainPassCB.FarZ = 1000.0f;
	mMainPassCB.TotalTime = gt.TotalTime();
	mMainPassCB.DeltaTime = gt.DeltaTime();
	mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
	mMainPassCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[0].Strength = { 0.9f, 0.9f, 0.8f };
	mMainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[1].Strength = { 0.3f, 0.3f, 0.3f };
	mMainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	mMainPassCB.Lights[2].Strength = { 0.15f, 0.15f, 0.15f };

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);

#if defined(Ex8)
	PassConstants spritePassCB = mMainPassCB;

	XMMATRIX spriteView = XMMatrixIdentity();
	XMMATRIX invSpriteView = XMMatrixIdentity();
	XMMATRIX spriteProj = XMMatrixOrthographicLH(static_cast<float>(mClientWidth), static_cast<float>(mClientHeight), 1.0f, 1000.0f);
	XMMATRIX invSpriteProj = XMMatrixInverse(&XMMatrixDeterminant(spriteProj), spriteProj);
	XMMATRIX spriteViewProj = XMMatrixMultiply(spriteView, spriteProj);
	XMMATRIX invSpriteViewProj = XMMatrixInverse(&XMMatrixDeterminant(spriteViewProj), spriteViewProj);
	XMStoreFloat4x4(&spritePassCB.View, XMMatrixTranspose(spriteView));
	XMStoreFloat4x4(&spritePassCB.InvView, XMMatrixTranspose(invSpriteView));
	XMStoreFloat4x4(&spritePassCB.Proj, XMMatrixTranspose(spriteProj));
	XMStoreFloat4x4(&spritePassCB.InvProj, XMMatrixTranspose(invSpriteProj));
	XMStoreFloat4x4(&spritePassCB.ViewProj, XMMatrixTranspose(spriteViewProj));
	XMStoreFloat4x4(&spritePassCB.InvViewProj, XMMatrixTranspose(invSpriteViewProj));

	currPassCB->CopyData(1, spritePassCB);
#endif
}

void BlendApp::UpdateWaves(const GameTimer& gt) {
	// Every quarter second, generate a random wave.
	static float t_base = 0.0f;
	if ((mTimer.TotalTime() - t_base) >= 0.25f) {
		t_base += 0.25f;

		int i = MathHelper::Rand(4, mWaves->RowCount() - 5);
		int j = MathHelper::Rand(4, mWaves->ColumnCount() - 5);

		float r = MathHelper::RandF(0.2f, 0.5f);

		mWaves->Disturb(i, j, r);
	}

	// Update the wave simulation.
	mWaves->Update(gt.DeltaTime());

	// Update the wave vertex buffer with the new solution.
	auto currWavesVB = mCurrFrameResource->WavesVB.get();
	for (int i = 0; i < mWaves->VertexCount(); i++) {
		Vertex v;

		v.Pos = mWaves->Position(i);
		v.Normal = mWaves->Normal(i);

		// Derive tex-coords from position by 
		// mapping [-w/2,w/2] --> [0,1]
		v.TexC.x = 0.5f + v.Pos.x / mWaves->Width();
		v.TexC.y = 0.5f - v.Pos.z / mWaves->Depth();

		currWavesVB->CopyData(i, v);
	}

	// Set the dynamic VB of the wave renderitem to the current frame VB.
	mWavesRitem->Geo->VertexBufferGPU = currWavesVB->Resource();
}

void BlendApp::LoadTextures() {
	auto grassTex = std::make_unique<Texture>();
	grassTex->Name = "grassTex";
	grassTex->Filename = L"./../../../Textures/grass.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), grassTex->Filename.c_str(),
		grassTex->Resource, grassTex->UploadHeap));

	auto waterTex = std::make_unique<Texture>();
	waterTex->Name = "waterTex";
	waterTex->Filename = L"./../../../Textures/water1.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), waterTex->Filename.c_str(),
		waterTex->Resource, waterTex->UploadHeap));

	auto fenceTex = std::make_unique<Texture>();
	fenceTex->Name = "fenceTex";
#if defined(Ex7)
	fenceTex->Filename = L"./../../../Textures/Bolt001.dds";
#else
	fenceTex->Filename = L"./../../../Textures/WireFence.dds";
#endif
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), fenceTex->Filename.c_str(),
		fenceTex->Resource, fenceTex->UploadHeap));

#if defined(Ex8)
	auto azaTex = std::make_unique<Texture>();
	azaTex->Name = "azaTex";
	azaTex->Filename = L"./../../../Textures/aza.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), azaTex->Filename.c_str(),
		azaTex->Resource, azaTex->UploadHeap));
#elif defined(Ex9)
	auto azaTex = std::make_unique<Texture>();
	azaTex->Name = "azaTex";
	azaTex->Filename = L"./../../../Textures/white1x1.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), azaTex->Filename.c_str(),
		azaTex->Resource, azaTex->UploadHeap));
#endif

	mTextures[grassTex->Name] = std::move(grassTex);
	mTextures[waterTex->Name] = std::move(waterTex);
	mTextures[fenceTex->Name] = std::move(fenceTex);
#if defined(Ex8) || defined(Ex9)
	mTextures[azaTex->Name] = std::move(azaTex);
#endif
}

void BlendApp::BuildRootSignature() {
	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[4];

	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[1].InitAsConstantBufferView(0);
	slotRootParameter[2].InitAsConstantBufferView(1);
	slotRootParameter[3].InitAsConstantBufferView(2);

	auto staticSamplers = GetStaticSamplers();

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter,
		(UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
		::OutputDebugStringA(static_cast<char*>(errorBlob->GetBufferPointer()));
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void BlendApp::BuildDescriptorHeaps() {
	//
	// Create the SRV heap.
	//
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
#if defined(Ex8) || defined(Ex9)
	srvHeapDesc.NumDescriptors = 4;
#else
	srvHeapDesc.NumDescriptors = 3;
#endif
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

	//
	// Fill out the heap with actual descriptors.
	//
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	auto grassTex = mTextures["grassTex"]->Resource;
	auto waterTex = mTextures["waterTex"]->Resource;
	auto fenceTex = mTextures["fenceTex"]->Resource;
#if defined(Ex8) || defined(Ex9)
	auto azaTex = mTextures["azaTex"]->Resource;
#endif

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = grassTex->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = -1;
	md3dDevice->CreateShaderResourceView(grassTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = waterTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(waterTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = fenceTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(fenceTex.Get(), &srvDesc, hDescriptor);

#if defined(Ex8) || defined(Ex9)
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = azaTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = azaTex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(azaTex.Get(), &srvDesc, hDescriptor);
#endif
}

void BlendApp::BuildShadersAndInputLayout() {
	const D3D_SHADER_MACRO defines[] = {
		"FOG", "1",
		NULL, NULL
	};

	const D3D_SHADER_MACRO alphaTestDefines[] = {
		"FOG", "1",
#if defined(Ex4)
		"DISCARD", "1",
#else
		"ALPHA_TEST", "1",
#endif
		NULL, NULL
	};

	mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "VS", "vs_5_0");
#if defined(Ex8) || defined(Ex9)
	mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "PS", "ps_5_0");
	mShaders["alphaTestedPS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "PS", "ps_5_0");
#else
	mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", defines, "PS", "ps_5_0");
	mShaders["alphaTestedPS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", alphaTestDefines, "PS", "ps_5_0");
#endif

	mInputLayout = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
}

void BlendApp::BuildLandGeometry() {
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(160.0f, 160.0f, 50, 50);

	//
	// Extract the vertex elements we are interested and apply the height function to
	// each vertex.  In addition, color the vertices based on their height so we have
	// sandy looking beaches, grassy low hills, and snow mountain peaks.
	//

	std::vector<Vertex> vertices(grid.Vertices.size());
	for (size_t i = 0; i < grid.Vertices.size(); i++) {
		auto& p = grid.Vertices[i].Position;
		vertices[i].Pos = p;
		vertices[i].Pos.y = GetHillsHeight(p.x, p.z);
		vertices[i].Normal = GetHillsNormal(p.x, p.z);
		vertices[i].TexC = grid.Vertices[i].TexC;
	}

	const UINT vbByteSize = static_cast<UINT>(vertices.size()) * sizeof(Vertex);

	std::vector<std::uint16_t> indices = grid.GetIndices16();
	const UINT ibByteSize = static_cast<UINT>(indices.size()) * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "landGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = static_cast<UINT>(indices.size());
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["grid"] = submesh;

	mGeometries["landGeo"] = std::move(geo);
}

void BlendApp::BuildWavesGeometry() {
	std::vector<std::uint16_t> indices(3 * mWaves->TriangleCount()); // 3 indices per face
	assert(mWaves->VertexCount() < 0x0000ffff);

	// Iterate over each quad.
	int m = mWaves->RowCount();
	int n = mWaves->ColumnCount();
	int k = 0;
	for (int i = 0; i < m - 1; i++)	{
		for (int j = 0; j < n - 1; j++) {
			indices[k] = i * n + j;
			indices[k + 1] = i * n + j + 1;
			indices[k + 2] = (i + 1)*n + j;

			indices[k + 3] = (i + 1)*n + j;
			indices[k + 4] = i * n + j + 1;
			indices[k + 5] = (i + 1)*n + j + 1;

			k += 6; // next quad
		}
	}

	UINT vbByteSize = mWaves->VertexCount() * sizeof(Vertex);
	UINT ibByteSize = static_cast<UINT>(indices.size()) * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "waterGeo";

	// Set dynamically.
	geo->VertexBufferCPU = nullptr;
	geo->VertexBufferGPU = nullptr;

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["grid"] = submesh;

	mGeometries["waterGeo"] = std::move(geo);
}

void BlendApp::BuildBoxGeometry() {
	GeometryGenerator geoGen;
#if defined(Ex7)
	GeometryGenerator::MeshData box = geoGen.CreateCylinder(3.0f, 3.0f, 6.0f, 20, 20);
#else
	GeometryGenerator::MeshData box = geoGen.CreateBox(8.0f, 8.0f, 8.0f, 3);
#endif

	std::vector<Vertex> vertices(box.Vertices.size());
	for (size_t i = 0; i < box.Vertices.size(); i++) {
		auto& p = box.Vertices[i].Position;
		vertices[i].Pos = p;
		vertices[i].Normal = box.Vertices[i].Normal;
		vertices[i].TexC = box.Vertices[i].TexC;
	}

	const UINT vbByteSize = static_cast<UINT>(vertices.size()) * sizeof(Vertex);

	std::vector<std::uint16_t> indices = box.GetIndices16();
	const UINT ibByteSize = static_cast<UINT>(indices.size()) * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "boxGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = static_cast<UINT>(indices.size());
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["box"] = submesh;

	mGeometries["boxGeo"] = std::move(geo);
}

void BlendApp::BuildQuadGeometry() {
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData quad = geoGen.CreateQuad(0.0f, 0.0f, 
		static_cast<float>(mClientWidth), static_cast<float>(mClientHeight), 1.0f);

	std::vector<Vertex> vertices(quad.Vertices.size());
	for (size_t i = 0; i < quad.Vertices.size(); i++) {
		auto& p = quad.Vertices[i].Position;
		vertices[i].Pos = p;
		vertices[i].Normal = quad.Vertices[i].Normal;
		vertices[i].TexC = quad.Vertices[i].TexC;
	}

	const UINT vbByteSize = static_cast<UINT>(vertices.size()) * sizeof(Vertex);

	std::vector<std::uint16_t> indices = quad.GetIndices16();
	const UINT ibByteSize = static_cast<UINT>(indices.size()) * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "quadGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = static_cast<UINT>(indices.size());
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["quad"] = submesh;

	mGeometries[geo->Name] = std::move(geo);
}

void BlendApp::BuildPSOs() {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

#if defined(Ex8)
	CD3DX12_BLEND_DESC blendState(D3D12_DEFAULT);
	blendState.RenderTarget[0].RenderTargetWriteMask = 0;
#endif

	//
	// PSO for opaque objects.
	//
	ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaquePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	opaquePsoDesc.pRootSignature = mRootSignature.Get();
	opaquePsoDesc.VS = {
		reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()),
		mShaders["standardVS"]->GetBufferSize()
	};
	opaquePsoDesc.PS = {
		reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
		mShaders["opaquePS"]->GetBufferSize()
	};
	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
#if defined(Ex8)
	opaquePsoDesc.BlendState = blendState;
#else
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
#endif
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
	opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	opaquePsoDesc.DSVFormat = mDepthStencilFormat;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));

	//
	// PSO for transparent objects
	//

	D3D12_GRAPHICS_PIPELINE_STATE_DESC transparentPsoDesc = opaquePsoDesc;

	D3D12_RENDER_TARGET_BLEND_DESC transparencyBlendDesc;
	transparencyBlendDesc.BlendEnable = true;
	transparencyBlendDesc.LogicOpEnable = false;
#if defined(Ex1_Add) || defined(Ex1_Sub)
	transparencyBlendDesc.SrcBlend = D3D12_BLEND_ONE;
#elif defined(Ex1_Mul)
	transparencyBlendDesc.SrcBlend = D3D12_BLEND_ZERO;
#else
	transparencyBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
#endif
#if defined(Ex1_Add) || defined(Ex1_Sub)
	transparencyBlendDesc.DestBlend = D3D12_BLEND_ONE;
#elif defined(Ex1_Mul)
	transparencyBlendDesc.DestBlend = D3D12_BLEND_SRC_COLOR;
	transparencyBlendDesc.DestBlend = D3D12_BLEND_ONE;
#else
	transparencyBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
#endif
#if defined(Ex1_Sub)
	transparencyBlendDesc.BlendOp = D3D12_BLEND_OP_SUBTRACT;
#else
	transparencyBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
#endif
	transparencyBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	transparencyBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
	transparencyBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
#if defined(Ex5)
	transparencyBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_BLUE | D3D12_COLOR_WRITE_ENABLE_ALPHA;
#elif defined(Ex8)
	transparencyBlendDesc.RenderTargetWriteMask = 0;
#else
	transparencyBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
#endif

	transparentPsoDesc.BlendState.RenderTarget[0] = transparencyBlendDesc;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&transparentPsoDesc, IID_PPV_ARGS(&mPSOs["transparent"])));

	//
	// PSO for alpha tested objects
	//

	D3D12_GRAPHICS_PIPELINE_STATE_DESC alphaTestedPsoDesc = opaquePsoDesc;
	alphaTestedPsoDesc.PS =	{
		reinterpret_cast<BYTE*>(mShaders["alphaTestedPS"]->GetBufferPointer()),
		mShaders["alphaTestedPS"]->GetBufferSize()
	};
	alphaTestedPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&alphaTestedPsoDesc, IID_PPV_ARGS(&mPSOs["alphaTested"])));

#if defined(Ex7)
	D3D12_GRAPHICS_PIPELINE_STATE_DESC exercisePsoDesc = opaquePsoDesc;

	D3D12_DEPTH_STENCIL_DESC exerciseDSS;
	exerciseDSS.DepthEnable = false;
	exerciseDSS.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	exerciseDSS.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	exerciseDSS.StencilEnable = false;

	D3D12_RENDER_TARGET_BLEND_DESC exerciseBlendDesc;
	exerciseBlendDesc.BlendEnable = true;
	exerciseBlendDesc.LogicOpEnable = false;
	exerciseBlendDesc.SrcBlend = D3D12_BLEND_SRC_COLOR;
	exerciseBlendDesc.DestBlend = D3D12_BLEND_ONE;
	exerciseBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	exerciseBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	exerciseBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
	exerciseBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	exerciseBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
	exerciseBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	exercisePsoDesc.DepthStencilState = exerciseDSS;
	exercisePsoDesc.BlendState.RenderTarget[0] = exerciseBlendDesc;
	exercisePsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&exercisePsoDesc, IID_PPV_ARGS(&mPSOs["Exercise"])));
#elif defined(Ex8)
	CD3DX12_BLEND_DESC depthCounterBlendState(D3D12_DEFAULT);
	depthCounterBlendState.RenderTarget[0].RenderTargetWriteMask = 0;

	D3D12_DEPTH_STENCIL_DESC depthCounterDDS;
	depthCounterDDS.DepthEnable = true;
	depthCounterDDS.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthCounterDDS.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	depthCounterDDS.StencilEnable = true;
	depthCounterDDS.StencilReadMask = 0xff;
	depthCounterDDS.StencilWriteMask = 0xff;

	depthCounterDDS.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthCounterDDS.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthCounterDDS.FrontFace.StencilPassOp = D3D12_STENCIL_OP_INCR;
	depthCounterDDS.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	depthCounterDDS.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthCounterDDS.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthCounterDDS.BackFace.StencilPassOp = D3D12_STENCIL_OP_INCR;
	depthCounterDDS.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC depthCounterPsoDesc = opaquePsoDesc;
	depthCounterPsoDesc.BlendState = depthCounterBlendState;
	depthCounterPsoDesc.DepthStencilState = depthCounterDDS;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&depthCounterPsoDesc, IID_PPV_ARGS(&mPSOs["depth"])));

	D3D12_DEPTH_STENCIL_DESC drawStencilDDS;
	drawStencilDDS.DepthEnable = true;
	drawStencilDDS.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	drawStencilDDS.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	drawStencilDDS.StencilEnable = true;
	drawStencilDDS.StencilReadMask = 0xff;
	drawStencilDDS.StencilWriteMask = 0xff;

	drawStencilDDS.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	drawStencilDDS.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	drawStencilDDS.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	drawStencilDDS.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;

	drawStencilDDS.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	drawStencilDDS.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	drawStencilDDS.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	drawStencilDDS.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC drawStencilPsoDesc = opaquePsoDesc;
	drawStencilPsoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	drawStencilPsoDesc.DepthStencilState = drawStencilDDS;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&drawStencilPsoDesc, IID_PPV_ARGS(&mPSOs["drawStencil"])));
#elif defined(Ex9)
	D3D12_RENDER_TARGET_BLEND_DESC additionBlendDesc;
	additionBlendDesc.BlendEnable = true;
	additionBlendDesc.LogicOpEnable = false;
	additionBlendDesc.SrcBlend = D3D12_BLEND_ONE;
	additionBlendDesc.DestBlend = D3D12_BLEND_ONE;
	additionBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	additionBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	additionBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
	additionBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	additionBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
	additionBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	D3D12_DEPTH_STENCIL_DESC additionDDS;
	additionDDS.DepthEnable = false;
	additionDDS.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	additionDDS.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	additionDDS.StencilEnable = false;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC additionPsoDesc = opaquePsoDesc;
	additionPsoDesc.BlendState.RenderTarget[0] = additionBlendDesc;
	additionPsoDesc.DepthStencilState = additionDDS;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&additionPsoDesc, IID_PPV_ARGS(&mPSOs["addition"])));
#endif
}

void BlendApp::BuildFrameResources() {
#if defined(Ex8)
	for (int i = 0; i < gNumFrameResources; i++) {
		mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
			2, static_cast<UINT>(mAllRitems.size()), static_cast<UINT>(mMaterials.size()), mWaves->VertexCount()));
	}
#else
	for (int i = 0; i < gNumFrameResources; i++) {
		mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
			1, static_cast<UINT>(mAllRitems.size()), static_cast<UINT>(mMaterials.size()), mWaves->VertexCount()));
	}
#endif
}

void BlendApp::BuildMaterials() {
	auto grass = std::make_unique<Material>();
	grass->Name = "grass";
	grass->MatCBIndex = 0;
	grass->DiffuseSrvHeapIndex = 0;
	grass->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	grass->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	grass->Roughness = 0.125f;

	// This is not a good water material definition, but we do not have all the rendering
	// tools we need (transparency, environment reflection), so we fake it for now.
	auto water = std::make_unique<Material>();
	water->Name = "water";
	water->MatCBIndex = 1;
	water->DiffuseSrvHeapIndex = 1;
	water->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.5f);
	water->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	water->Roughness = 0.0f;

	auto wirefence = std::make_unique<Material>();
	wirefence->Name = "wirefence";
	wirefence->MatCBIndex = 2;
	wirefence->DiffuseSrvHeapIndex = 2;
	wirefence->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
#if defined(Ex7)
	wirefence->FresnelR0 = XMFLOAT3(0.0f, 0.0f, 0.0f);
	wirefence->Roughness = 0.9f;
#else
	wirefence->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	wirefence->Roughness = 0.25f;
#endif

#if defined(Ex8)
	auto blueMat = std::make_unique<Material>();
	blueMat->Name = "blueMat";
	blueMat->MatCBIndex = 3;
	blueMat->DiffuseSrvHeapIndex = 3;
	blueMat->DiffuseAlbedo = XMFLOAT4(0.1f, 0.1f, 1.0f, 1.0f);
	
	auto greenMat = std::make_unique<Material>();
	greenMat->Name = "greenMat";
	greenMat->MatCBIndex = 4;
	greenMat->DiffuseSrvHeapIndex = 3;
	greenMat->DiffuseAlbedo = XMFLOAT4(0.1f, 1.0f, 0.1f, 1.0f);

	auto redMat = std::make_unique<Material>();
	redMat->Name = "redMat";
	redMat->MatCBIndex = 5;
	redMat->DiffuseSrvHeapIndex = 3;
	redMat->DiffuseAlbedo = XMFLOAT4(1.0f, 0.1f, 0.1f, 1.0f);
#elif defined(Ex9)
	auto addMat = std::make_unique<Material>();
	addMat->Name = "addMat";
	addMat->MatCBIndex = 3;
	addMat->DiffuseSrvHeapIndex = 3;
	addMat->DiffuseAlbedo = XMFLOAT4(0.05f, 0.05f, 0.05f, 1.0f);
#endif

	mMaterials[grass->Name] = std::move(grass);
	mMaterials[water->Name] = std::move(water);
	mMaterials[wirefence->Name] = std::move(wirefence);
#if defined(Ex8)
	mMaterials[blueMat->Name] = std::move(blueMat);
	mMaterials[greenMat->Name] = std::move(greenMat);
	mMaterials[redMat->Name] = std::move(redMat);
#elif defined(Ex9)
	mMaterials[addMat->Name] = std::move(addMat);
#endif
}

void BlendApp::BuildRenderItems() {
	auto wavesRitem = std::make_unique<RenderItem>();
	wavesRitem->World = MathHelper::Identity4x4();
	XMStoreFloat4x4(&wavesRitem->TexTransform, XMMatrixScaling(5.0f, 5.0f, 1.0f));
	wavesRitem->ObjCBIndex = 0;
#if defined(Ex9)
	wavesRitem->Mat = mMaterials["addMat"].get();
#else
	wavesRitem->Mat = mMaterials["water"].get();
#endif
	wavesRitem->Geo = mGeometries["waterGeo"].get();
	wavesRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	wavesRitem->IndexCount = wavesRitem->Geo->DrawArgs["grid"].IndexCount;
	wavesRitem->StartIndexLocation = wavesRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	wavesRitem->BaseVertexLocation = wavesRitem->Geo->DrawArgs["grid"].BaseVertexLocation;

	mWavesRitem = wavesRitem.get();

	mRitemLayer[static_cast<int>(RenderLayer::Transparent)].push_back(wavesRitem.get());

	auto gridRitem = std::make_unique<RenderItem>();
	gridRitem->World = MathHelper::Identity4x4();
	XMStoreFloat4x4(&gridRitem->TexTransform, XMMatrixScaling(5.0f, 5.0f, 1.0f));
	gridRitem->ObjCBIndex = 1;
#if defined(Ex9)
	gridRitem->Mat = mMaterials["addMat"].get();
#else
	gridRitem->Mat = mMaterials["grass"].get();
#endif
	gridRitem->Geo = mGeometries["landGeo"].get();
	gridRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
	gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;

	mRitemLayer[static_cast<int>(RenderLayer::Opaque)].push_back(gridRitem.get());

	auto boxRitem = std::make_unique<RenderItem>();
#if defined(Ex7)
	XMStoreFloat4x4(&boxRitem->World, XMMatrixTranslation(0.0f, 3.0f, 0.0f));
#else
	XMStoreFloat4x4(&boxRitem->World, XMMatrixTranslation(3.0f, 2.0f, -9.0f));
#endif
	boxRitem->ObjCBIndex = 2;
#if defined(Ex9)
	boxRitem->Mat = mMaterials["addMat"].get();
#else
	boxRitem->Mat = mMaterials["wirefence"].get();
#endif
	boxRitem->Geo = mGeometries["boxGeo"].get();
	boxRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
	boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;

#if defined(Ex7)
	mRitemLayer[static_cast<int>(RenderLayer::Bolt)].push_back(boxRitem.get());
#else
	mRitemLayer[static_cast<int>(RenderLayer::AlphaTested)].push_back(boxRitem.get());
#endif

#if defined(Ex8)
	auto blueQuadRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&blueQuadRitem->World, XMMatrixTranslation(
		static_cast<float>(-mClientWidth / 2), static_cast<float>(mClientHeight / 2), 0.0f));
	blueQuadRitem->ObjCBIndex = 3;
	blueQuadRitem->Mat = mMaterials["blueMat"].get();
	blueQuadRitem->Geo = mGeometries["quadGeo"].get();
	blueQuadRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	blueQuadRitem->IndexCount = blueQuadRitem->Geo->DrawArgs["quad"].IndexCount;
	blueQuadRitem->StartIndexLocation = blueQuadRitem->Geo->DrawArgs["quad"].StartIndexLocation;
	blueQuadRitem->BaseVertexLocation = blueQuadRitem->Geo->DrawArgs["quad"].BaseVertexLocation;
	mRitemLayer[static_cast<int>(RenderLayer::DepthBlue)].push_back(blueQuadRitem.get());

	auto greenQuadRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&greenQuadRitem->World, XMMatrixTranslation(
		static_cast<float>(-mClientWidth / 2), static_cast<float>(mClientHeight / 2), 0.0f));
	greenQuadRitem->ObjCBIndex = 4;
	greenQuadRitem->Mat = mMaterials["greenMat"].get();
	greenQuadRitem->Geo = mGeometries["quadGeo"].get();
	greenQuadRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	greenQuadRitem->IndexCount = greenQuadRitem->Geo->DrawArgs["quad"].IndexCount;
	greenQuadRitem->StartIndexLocation = greenQuadRitem->Geo->DrawArgs["quad"].StartIndexLocation;
	greenQuadRitem->BaseVertexLocation = greenQuadRitem->Geo->DrawArgs["quad"].BaseVertexLocation;
	mRitemLayer[static_cast<int>(RenderLayer::DepthGreen)].push_back(greenQuadRitem.get());

	auto redQuadRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&redQuadRitem->World, XMMatrixTranslation(
		static_cast<float>(-mClientWidth / 2), static_cast<float>(mClientHeight / 2), 0.0f));
	redQuadRitem->ObjCBIndex = 5;
	redQuadRitem->Mat = mMaterials["redMat"].get();
	redQuadRitem->Geo = mGeometries["quadGeo"].get();
	redQuadRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	redQuadRitem->IndexCount = redQuadRitem->Geo->DrawArgs["quad"].IndexCount;
	redQuadRitem->StartIndexLocation = redQuadRitem->Geo->DrawArgs["quad"].StartIndexLocation;
	redQuadRitem->BaseVertexLocation = redQuadRitem->Geo->DrawArgs["quad"].BaseVertexLocation;
	mRitemLayer[static_cast<int>(RenderLayer::DepthRed)].push_back(redQuadRitem.get());
#endif

	mAllRitems.push_back(std::move(wavesRitem));
	mAllRitems.push_back(std::move(gridRitem));
	mAllRitems.push_back(std::move(boxRitem));
#if defined(Ex8)
	mAllRitems.push_back(std::move(blueQuadRitem));
	mAllRitems.push_back(std::move(greenQuadRitem));
	mAllRitems.push_back(std::move(redQuadRitem));
#endif
}

void BlendApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems) {
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));

	auto objectCB = mCurrFrameResource->ObjectCB->Resource();
	auto matCB = mCurrFrameResource->MaterialCB->Resource();

	// For each render item...
	for (size_t i = 0; i < ritems.size(); i++) {
		auto ri = ritems[i];

		cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
		cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		tex.Offset(ri->Mat->DiffuseSrvHeapIndex, mCbvSrvDescriptorSize);

		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;
		D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + ri->Mat->MatCBIndex * matCBByteSize;

		cmdList->SetGraphicsRootDescriptorTable(0, tex);
		cmdList->SetGraphicsRootConstantBufferView(1, objCBAddress);
		cmdList->SetGraphicsRootConstantBufferView(3, matCBAddress);

		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> BlendApp::GetStaticSamplers() {
	// Applications usually only need a handful of samplers.  So just define them all up front
	// and keep them available as part of the root signature.  

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	return {
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp };
}

float BlendApp::GetHillsHeight(float x, float z) const {
	return 0.3f * (z * sinf(0.1f * x) + x * cosf(0.1f * z));
}

DirectX::XMFLOAT3 BlendApp::GetHillsNormal(float x, float z) const {
	// n = (-df/dx, 1, -df/dz)
	XMFLOAT3 n(
		-0.03f * z * cosf(0.1f * x) - 0.3f * cosf(0.1f * z),
		1.0f,
		-0.3f * sinf(0.1f * x) + 0.03f * x * sinf(0.1f * z));

	XMVECTOR unitNormal = XMVector3Normalize(XMLoadFloat3(&n));
	XMStoreFloat3(&n, unitNormal);

	return n;
}