#include "LearningDXR/Renderer.h"
#include "common/GeometryGenerator.h"

using namespace DirectX;
using namespace DirectX::PackedVector;
using namespace Microsoft::WRL;

namespace {
	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers() {
		const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
			0, // shaderRegister
			D3D12_FILTER_MIN_MAG_MIP_POINT,		// filter
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,	// addressU
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,	// addressV
			D3D12_TEXTURE_ADDRESS_MODE_WRAP		// addressW
		);

		const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
			1, // shaderRegister
			D3D12_FILTER_MIN_MAG_MIP_POINT,		// filter
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,	// addressU
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,	// addressV
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP	// addressW
		);

		const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
			2, // shaderRegister
			D3D12_FILTER_MIN_MAG_MIP_LINEAR,	// filter
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,	// addressU
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,	// addressV
			D3D12_TEXTURE_ADDRESS_MODE_WRAP		// addressW
		);

		const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
			3, // shaderRegister
			D3D12_FILTER_MIN_MAG_MIP_LINEAR,	// filter
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,	// addressU
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,	// addressV
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP	// addressW
		);

		const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
			4, // shaderRegister
			D3D12_FILTER_ANISOTROPIC,			// filter
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,	// addressU
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,	// addressV
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,	// addressW
			0.0f,								// mipLODBias
			8									// maxAnisotropy
		);

		const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
			5, // shaderRegister
			D3D12_FILTER_ANISOTROPIC,			// filter
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,	// addressU
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,	// addressV
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,	// addressW
			0.0f,								// mipLODBias
			8									// maxAnisotropy
		);

		return { pointWrap, pointClamp, linearWrap, linearClamp, anisotropicWrap, anisotropicClamp };
	}
}

FrameResource::FrameResource(ID3D12Device* inDevice, UINT inPassCount, UINT inObjectCount, UINT inMaterialCount) : 
		PassCount(inPassCount),
		ObjectCount(inObjectCount),
		MaterialCount(inMaterialCount) {
	Device = inDevice;
}

GameResult FrameResource::Initialize() {
	ReturnIfFailed(Device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(CmdListAlloc.GetAddressOf())
	));

	PassCB.Initialize(Device, PassCount, true);
	ObjectCB.Initialize(Device, ObjectCount, true);
	MaterialCB.Initialize(Device, MaterialCount, false);

	return GameResult(S_OK);
}

Renderer::Renderer() {}

Renderer::~Renderer() {
	if (!bIsCleanedUp)
		CleanUp();
}

GameResult Renderer::Initialize(HWND hMainWnd, UINT inClientWidth /* = 800 */, UINT inClientHeight /* = 600 */) {
	CheckGameResult(LowRenderer::Initialize(hMainWnd, inClientWidth, inClientHeight));

	mMainRenderTargets.resize(gNumFrameResources);

	XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
	XMStoreFloat4x4(&mProj, P);

	ReturnIfFailed(mDirectCmdListAlloc->Reset());
	ReturnIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));
	
	CheckGameResult(mShaderManager.Initialize());
	CheckGameResult(mDxcShaderManager.Initialize());

	CheckGameResult(BuildGeometries());
	CheckGameResult(CompileShader());
	CheckGameResult(BuildRootSignature());
	CheckGameResult(BuildResources());
	CheckGameResult(BuildDescriptorHeaps());
	CheckGameResult(BuildDescriptors());
	CheckGameResult(BuildPSOs());
	CheckGameResult(BuildMaterials());
	CheckGameResult(BuildFrameResources());
	CheckGameResult(BuildRenderItems());

	ReturnIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };	
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	CheckGameResult(FlushCommandQueue());

	return GameResultOk;
}
 
void Renderer::CleanUp() {
	LowRenderer::CleanUp();

	mDxcShaderManager.CleanUp();

	bIsCleanedUp = true;
}

GameResult Renderer::Update(const GameTimer& gt) {
	CheckGameResult(UpdateCamera(gt));

	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	// Has the GPU finished processing the commands of the current frame resource?
	// If not, wait until the GPU has completed commands up to this fence point.
	if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence) {
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		ReturnIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	CheckGameResult(UpdateObjectCB(gt));
	CheckGameResult(UpdatePassCB(gt));
	CheckGameResult(UpdateMaterialCB(gt));

	return GameResultOk;
}

GameResult Renderer::Draw() {
	ReturnIfFailed(mCurrFrameResource->CmdListAlloc->Reset());
	
	if (bRaytracing) {
		CheckGameResult(Raytrace());
	}
	else {
		CheckGameResult(Rasterize());
	}

	ReturnIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	ReturnIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	mCurrFrameResource->Fence = static_cast<UINT>(++mCurrentFence);
	mCommandQueue->Signal(mFence.Get(), mCurrentFence);

	return GameResultOk;
}

GameResult Renderer::OnResize(UINT inClientWidth, UINT inClientHeight) {
	CheckGameResult(LowRenderer::OnResize(inClientWidth, inClientHeight));

	XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
	XMStoreFloat4x4(&mProj, P);

	CheckGameResult(BuildResources());
	CheckGameResult(BuildDescriptors());

	return GameResultOk;
}

bool Renderer::GetRenderType() const {
	return bRaytracing;
}

void Renderer::SetRenderType(bool inValue) {
	bRaytracing = inValue;
}

GameResult Renderer::CreateRtvAndDsvDescriptorHeaps() {
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = SwapChainBufferCount + gNumFrameResources /* for UAVs */;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	ReturnIfFailed(md3dDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(mRtvHeap.GetAddressOf())));

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	ReturnIfFailed(md3dDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(mDsvHeap.GetAddressOf())));

	return GameResultOk;
}

GameResult Renderer::BuildGeometries() {
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData sphere = geoGen.CreateSphere(1.0f, 32, 32);

	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.IndexCount = static_cast<UINT>(sphere.GetIndices16().size());
	sphereSubmesh.BaseVertexLocation = 0;
	sphereSubmesh.StartIndexLocation = 0;

	std::vector<Vertex> vertices(sphere.Vertices.size());
	for (size_t i = 0, end = vertices.size(); i < end; ++i) {
		vertices[i].Pos = sphere.Vertices[i].Position;
		vertices[i].Normal = sphere.Vertices[i].Normal;
		vertices[i].TexC = sphere.Vertices[i].TexC;
		vertices[i].Tangent = sphere.Vertices[i].TangentU;
	}

	std::vector<std::uint16_t> indices;
	indices.insert(indices.end(), std::begin(sphere.GetIndices16()), std::end(sphere.GetIndices16()));

	const UINT vbByteSize = static_cast<UINT>(vertices.size() * sizeof(Vertex));
	const UINT ibByteSize = static_cast<UINT>(indices.size() * sizeof(std::uint16_t));

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "sphereGeo";

	ReturnIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);
	
	ReturnIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	CheckGameResult(D3D12Util::CreateDefaultBuffer(
		md3dDevice.Get(),
		mCommandList.Get(),
		vertices.data(),
		vbByteSize,
		geo->VertexBufferUploader,
		geo->VertexBufferGPU)
	);

	CheckGameResult(D3D12Util::CreateDefaultBuffer(
		md3dDevice.Get(),
		mCommandList.Get(),
		indices.data(),
		ibByteSize,
		geo->IndexBufferUploader,
		geo->IndexBufferGPU)
	);

	geo->VertexByteStride = static_cast<UINT>(sizeof(Vertex));
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	geo->DrawArgs["sphere"] = sphereSubmesh;

	//D3D12_RAYTRACING_GEOMETRY_DESC sphereGeoDesc = {};
	//sphereGeoDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
	//sphereGeoDesc.Triangles.VertexBuffer.StartAddress = geo->VertexBufferGPU->GetGPUVirtualAddress();
	//sphereGeoDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);
	//sphereGeoDesc.Triangles.VertexCount = static_cast<UINT>(vertices.size());
	//sphereGeoDesc.Triangles.IndexBuffer = geo->IndexBufferGPU->GetGPUVirtualAddress();
	//sphereGeoDesc.Triangles.IndexFormat = DXGI_FORMAT_R16_UINT;
	//sphereGeoDesc.Triangles.IndexCount = static_cast<UINT>(indices.size());
	//sphereGeoDesc.Triangles.Transform3x4 = 0;
	//sphereGeoDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
	//mGeoDescs["sphere"] = sphereGeoDesc;

	mGeometries[geo->Name] = std::move(geo);

	return GameResultOk;
}

GameResult Renderer::CompileShader() {
	{
		const std::wstring filePath = ShaderFilePathW + L"LearningDXR\\Default.hlsl";
		CheckGameResult(mShaderManager.CompileShader(filePath, nullptr, "VS", "vs_5_1", "defaultVS"));
		CheckGameResult(mShaderManager.CompileShader(filePath, nullptr, "PS", "ps_5_1", "defaultPS"));
	}
	{
		const std::wstring filePath = ShaderFilePathW + L"LearningDXR\\SampleToBackBuffer.hlsl";
		CheckGameResult(mShaderManager.CompileShader(filePath, nullptr, "VS", "vs_5_1", "backBufferVS"));
		CheckGameResult(mShaderManager.CompileShader(filePath, nullptr, "PS", "ps_5_1", "backBufferPS"));
	}

	return GameResultOk;
}

GameResult Renderer::BuildRootSignature() {
	CD3DX12_ROOT_PARAMETER slotRootParameter[4];

	CD3DX12_DESCRIPTOR_RANGE uavTable;
	uavTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

	slotRootParameter[0].InitAsDescriptorTable(1, &uavTable);
	slotRootParameter[1].InitAsConstantBufferView(0);
	slotRootParameter[2].InitAsConstantBufferView(1);
	slotRootParameter[3].InitAsConstantBufferView(2);

	auto samplers = GetStaticSamplers();

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		_countof(slotRootParameter), slotRootParameter,
		static_cast<UINT>(samplers.size()), samplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	CheckGameResult(D3D12Util::CreateRootSignature(md3dDevice.Get(), rootSigDesc, mRootSignature.GetAddressOf()));

	return GameResultOk;
}

GameResult Renderer::BuildResources() {
	D3D12_RESOURCE_DESC rscDesc = {};
	ZeroMemory(&rscDesc, sizeof(D3D12_RESOURCE_DESC));
	rscDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	rscDesc.Alignment = 0;
	rscDesc.Width = mClientWidth;
	rscDesc.Height = mClientHeight;
	rscDesc.Format = mBackBufferFormat;
	rscDesc.DepthOrArraySize = 1;
	rscDesc.MipLevels = 1;
	rscDesc.SampleDesc.Count = 1;
	rscDesc.SampleDesc.Quality = 0;
	rscDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	rscDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	for (int i = 0; i < gNumFrameResources; ++i) {
		ReturnIfFailed(md3dDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&rscDesc,
			D3D12_RESOURCE_STATE_COPY_SOURCE,
			nullptr,
			IID_PPV_ARGS(mMainRenderTargets[i].GetAddressOf())
		));
	}

	return GameResultOk;
}

GameResult Renderer::BuildDescriptorHeaps() {
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = 32;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	md3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mCbvSrvUavDescriptorHeaps));

	return GameResultOk;
}

GameResult Renderer::BuildDescriptors() {
	auto hRtvDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(mRtvHeap->GetCPUDescriptorHandleForHeapStart(), SwapChainBufferCount, mRtvDescriptorSize);
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Format = mBackBufferFormat;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;	

	auto hUavDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvSrvUavDescriptorHeaps->GetCPUDescriptorHandleForHeapStart());
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Format = mBackBufferFormat;
	uavDesc.Texture2D.MipSlice = 0;
	uavDesc.Texture2D.PlaneSlice = 0;

	for (int i = 0; i < gNumFrameResources; ++i) {
		md3dDevice->CreateRenderTargetView(mMainRenderTargets[i].Get(), &rtvDesc, hRtvDescriptor);
		hRtvDescriptor.Offset(1, mRtvDescriptorSize);

		md3dDevice->CreateUnorderedAccessView(mMainRenderTargets[i].Get(), nullptr, &uavDesc, hUavDescriptor);
		hUavDescriptor.Offset(1, mCbvSrvUavDescriptorSize);
	}

	return GameResultOk;
}

GameResult Renderer::BuildPSOs() {
	std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout = {
		{ "POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT,			0, 0,	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL",		0, DXGI_FORMAT_R32G32B32_FLOAT,			0, 12,	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD",	0, DXGI_FORMAT_R32G32_FLOAT,			0, 24,	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT",	0, DXGI_FORMAT_R32G32B32_FLOAT,			0, 32,	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC defaultPsoDesc;
	ZeroMemory(&defaultPsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	defaultPsoDesc.InputLayout = { inputLayout.data(), static_cast<UINT>(inputLayout.size()) };
	defaultPsoDesc.pRootSignature = mRootSignature.Get();
	{
		auto vs = mShaderManager.GetShader("defaultVS");
		auto ps = mShaderManager.GetShader("defaultPS");
		defaultPsoDesc.VS = {
			reinterpret_cast<BYTE*>(vs->GetBufferPointer()),
			vs->GetBufferSize()
		};
		defaultPsoDesc.PS = {
			reinterpret_cast<BYTE*>(ps->GetBufferPointer()),
			ps->GetBufferSize()
		};
	}
	defaultPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	defaultPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	defaultPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	defaultPsoDesc.SampleMask = UINT_MAX;
	defaultPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	defaultPsoDesc.NumRenderTargets = 1;
	defaultPsoDesc.RTVFormats[0] = mBackBufferFormat;
	defaultPsoDesc.SampleDesc.Count = 1;
	defaultPsoDesc.SampleDesc.Quality = 0;
	defaultPsoDesc.DSVFormat = mDepthStencilFormat;
	ReturnIfFailed(md3dDevice->CreateGraphicsPipelineState(&defaultPsoDesc, IID_PPV_ARGS(&mPSOs["Default"])));

	return GameResultOk;
}

GameResult Renderer::BuildMaterials() {
	auto defaultMat = std::make_unique<Material>();
	defaultMat->Name = "default";
	defaultMat->MatCBIndex = 0;
	defaultMat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	defaultMat->FresnelR0 = XMFLOAT3(0.8f, 0.8f, 0.8f);
	defaultMat->Roughness = 0.25f;

	mMaterials[defaultMat->Name] = std::move(defaultMat);

	return GameResultOk;
}

GameResult Renderer::BuildFrameResources() {
	for (int i = 0; i < gNumFrameResources; i++) {
		mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(), 1, 32, 32));
		CheckGameResult(mFrameResources.back()->Initialize());
	}

	return GameResultOk;
}

GameResult Renderer::BuildRenderItems() {
	auto sphereRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&sphereRitem->World, XMMatrixScaling(2.0f, 2.0f, 2.0f)*XMMatrixTranslation(0.0f, 0.5f, 0.0f));
	sphereRitem->ObjCBIndex = 0;
	sphereRitem->Geo = mGeometries["sphereGeo"].get();
	sphereRitem->Mat = mMaterials["default"].get();
	sphereRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	sphereRitem->IndexCount = sphereRitem->Geo->DrawArgs["sphere"].IndexCount;
	sphereRitem->StartIndexLocation = sphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
	sphereRitem->BaseVertexLocation = sphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;
	mAllRitems.push_back(std::move(sphereRitem));

	return GameResultOk;
}

GameResult Renderer::UpdateCamera(const GameTimer& gt) {
	mEyePos.x = mRadius * sinf(mPhi) * cosf(mTheta);
	mEyePos.z = mRadius * sinf(mPhi) * sinf(mTheta);
	mEyePos.y = mRadius * cosf(mPhi);

	XMVECTOR pos = XMVectorSet(mEyePos.x, mEyePos.y, mEyePos.z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&mView, view);

	return GameResultOk;
}

GameResult Renderer::UpdateObjectCB(const GameTimer& gt) {
	auto& currObjectCB = mCurrFrameResource->ObjectCB;
	for (auto& e : mAllRitems) {
		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		if (e->NumFramesDirty > 0) {
			XMMATRIX world = XMLoadFloat4x4(&e->World);
			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);
	
			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));
	
			currObjectCB.CopyData(e->ObjCBIndex, objConstants);
	
			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;
		}
	}

	return GameResultOk;
}

GameResult Renderer::UpdatePassCB(const GameTimer& gt) {
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
	mMainPassCB.AmbientLight = { 0.8f, 0.3f, 0.15f, 1.0f };
	mMainPassCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[0].Strength = { 0.6f, 0.6f, 0.6f };
	mMainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[1].Strength = { 0.3f, 0.3f, 0.3f };
	mMainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	mMainPassCB.Lights[2].Strength = { 0.15f, 0.15f, 0.15f };

	auto& currPassCB = mCurrFrameResource->PassCB;
	currPassCB.CopyData(0, mMainPassCB);

	return GameResultOk;
}

GameResult Renderer::UpdateMaterialCB(const GameTimer& gt) {
	auto& currMaterialCB = mCurrFrameResource->MaterialCB;
	for (auto& e : mMaterials) {
		// Only update the cbuffer data if the constants have changed.  If the cbuffer
		// data changes, it needs to be updated for each FrameResource.
		Material* mat = e.second.get();
		if (mat->NumFramesDirty > 0) {
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialConstants matConsts;
			matConsts.DiffuseAlbedo = mat->DiffuseAlbedo;
			matConsts.FresnelR0 = mat->FresnelR0;
			matConsts.Roughness = mat->Roughness;
			XMStoreFloat4x4(&matConsts.MatTransform, XMMatrixTranspose(matTransform));

			currMaterialCB.CopyData(mat->MatCBIndex, matConsts);

			// Next FrameResource need to be updated too.
			mat->NumFramesDirty--;
		}
	}

	return GameResultOk;
}

GameResult Renderer::Rasterize() {
	ReturnIfFailed(mCommandList->Reset(mCurrFrameResource->CmdListAlloc.Get(), mPSOs["Default"].Get()));

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvSrvUavDescriptorHeaps.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	auto hDesc = CD3DX12_CPU_DESCRIPTOR_HANDLE(mRtvHeap->GetCPUDescriptorHandleForHeapStart(), SwapChainBufferCount + mCurrFrameResourceIndex, mRtvDescriptorSize);
	mCommandList->ClearRenderTargetView(hDesc, Colors::AliceBlue, 0, nullptr);
	mCommandList->ClearDepthStencilView(
		DepthStencilView(),
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
		1.0f,
		0,
		0,
		nullptr
	);

	mCommandList->OMSetRenderTargets(1, &hDesc, true, &DepthStencilView());

	auto& passCB = mCurrFrameResource->PassCB;
	mCommandList->SetGraphicsRootConstantBufferView(2, passCB.Resource()->GetGPUVirtualAddress());

	auto pMainRenderTarget = mMainRenderTargets[mCurrFrameResourceIndex].Get();

	mCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			pMainRenderTarget,
			D3D12_RESOURCE_STATE_COPY_SOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET)
	);

	CheckGameResult(DrawRenderItems());

	mCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			pMainRenderTarget,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_COPY_SOURCE)
	);

	mCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			CurrentBackBuffer(),
			D3D12_RESOURCE_STATE_PRESENT,
			D3D12_RESOURCE_STATE_COPY_DEST)
	);

	mCommandList->CopyResource(CurrentBackBuffer(), pMainRenderTarget);

	mCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			CurrentBackBuffer(),
			D3D12_RESOURCE_STATE_COPY_DEST,
			D3D12_RESOURCE_STATE_PRESENT)
	);

	return GameResultOk;
}

GameResult Renderer::Raytrace() {
	ReturnIfFailed(mCommandList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	auto hDesc = CD3DX12_CPU_DESCRIPTOR_HANDLE(mRtvHeap->GetCPUDescriptorHandleForHeapStart(), SwapChainBufferCount + mCurrFrameResourceIndex, mRtvDescriptorSize);
	mCommandList->ClearRenderTargetView(hDesc, Colors::AliceBlue, 0, nullptr);
	mCommandList->ClearDepthStencilView(
		DepthStencilView(),
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
		1.0f,
		0,
		0,
		nullptr
	);

	mCommandList->OMSetRenderTargets(1, &hDesc, true, &DepthStencilView());

	auto pMainRenderTarget = mMainRenderTargets[mCurrFrameResourceIndex].Get();

	mCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			pMainRenderTarget,
			D3D12_RESOURCE_STATE_COPY_SOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET)
	);

	mCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			pMainRenderTarget,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_COPY_SOURCE)
	);

	mCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			CurrentBackBuffer(),
			D3D12_RESOURCE_STATE_PRESENT,
			D3D12_RESOURCE_STATE_COPY_DEST)
	);

	mCommandList->CopyResource(CurrentBackBuffer(), pMainRenderTarget);

	mCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			CurrentBackBuffer(),
			D3D12_RESOURCE_STATE_COPY_DEST,
			D3D12_RESOURCE_STATE_PRESENT)
	);

	return GameResultOk;
}

GameResult Renderer::DrawRenderItems() {
	UINT objCBByteSize = D3D12Util::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT matCBByteSize = D3D12Util::CalcConstantBufferByteSize(sizeof(MaterialConstants));

	auto& objectCB = mCurrFrameResource->ObjectCB;
	auto& matCB = mCurrFrameResource->MaterialCB;

	// For each render item...
	for (size_t i = 0; i < mAllRitems.size(); ++i) {
		auto& ri = mAllRitems[i];

		mCommandList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
		mCommandList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
		mCommandList->IASetPrimitiveTopology(ri->PrimitiveType);
		
		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB.Resource()->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;
		D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB.Resource()->GetGPUVirtualAddress() + ri->Mat->MatCBIndex * matCBByteSize;

		mCommandList->SetGraphicsRootConstantBufferView(1, objCBAddress);
		mCommandList->SetGraphicsRootConstantBufferView(3, matCBAddress);

		mCommandList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}

	return GameResultOk;
}