#include "DX12Game/Renderer.h"
#include "DX12Game/GameWorld.h"
#include "DX12Game/GameCamera.h"
#include "DX12Game/Mesh.h"
#include "common/GeometryGenerator.h"

#include <ResourceUploadBatch.h>

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

namespace {
	const std::wstring textureFileNamePrefix = L"./../../../../Assets/Textures/";
	const std::wstring shaderFileNamePrefix = L".\\..\\..\\..\\..\\Assets\\Shaders\\";
	const std::wstring fontFileNamePrefix = L"./../../../../Assets/Fonts/";

	const UINT gMaxInstanceCount = 32;
}

Renderer::Renderer() 
	: LowRenderer() {
	// Estimate the scene bounding sphere manually since we know how the scene was constructed.
	// The grid is the "widest object" with a width of 20 and depth of 30.0f, and centered at
	// the world space origin.  In general, you need to loop over every world space vertex
	// position and compute the bounding sphere.
	mSceneBounds.Center = XMFLOAT3(0.0f, 0.0f, 0.0f);
	mSceneBounds.Radius = sqrtf(40.0f * 40.0f + 40.0f * 40.0f);
}

Renderer::~Renderer() {
	if (md3dDevice != nullptr)
		FlushCommandQueue();
}

bool Renderer::Initialize(HWND hMainWnd, UINT inWidth, UINT inHeight) {
	if (!LowRenderer::Initialize(hMainWnd, inWidth, inHeight))
		return false;

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	mShadowMap = std::make_unique<ShadowMap>(md3dDevice.Get(),
		4096, 4096);

	mSsao = std::make_unique<Ssao>(
		md3dDevice.Get(),
		mCommandList.Get(),
		mClientWidth, mClientHeight);

	mAnimsMap = std::make_unique<AnimationsMap>(md3dDevice.Get(), mCommandList.Get());

	LoadBasicTextures();
	BuildRootSignature();
	BuildSsaoRootSignature();
	BuildDescriptorHeaps();
	BuildShadersAndInputLayout();
	BuildBasicGeometry();
	BuildBasicMaterials();
	BuildBasicRenderItems();
	BuildFrameResources();
	BuildPSOs();

	mSsao->SetPSOs(mPSOs["ssao"].Get(), mPSOs["ssaoBlur"].Get());

	// Execute the initialization commands.
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	FlushCommandQueue();

	//
	// Initializes the things for drawing text.
	//
	mGraphicsMemory = std::make_unique<GraphicsMemory>(md3dDevice.Get());

	ResourceUploadBatch resourceUpload(md3dDevice.Get());
	resourceUpload.Begin();

	mDefaultFont = std::make_unique<SpriteFont>(
		md3dDevice.Get(),
		resourceUpload,
		(fontFileNamePrefix + L"D2Coding.spritefont").c_str(),
		GetCpuSrv(mDescHeapIdx.DefaultFontIndex),
		GetGpuSrv(mDescHeapIdx.DefaultFontIndex));

	RenderTargetState rtState(mBackBufferFormat, mDepthStencilFormat);
	SpriteBatchPipelineStateDescription pd(rtState);
	mSpriteBatch = std::make_unique<SpriteBatch>(md3dDevice.Get(), resourceUpload, pd);

	auto uploadResourcesFinished = resourceUpload.End(mCommandQueue.Get());
	uploadResourcesFinished.wait();

	// After uploadResourcesFinished.wait() returned.
	mSpriteBatch->SetViewport(mScreenViewport);
	
	auto world = GameWorld::GetWorld();
	mTextPos.x = static_cast<float>(mScreenViewport.Width * 0.01f);
	mTextPos.y = static_cast<float>(mScreenViewport.Height * 0.01f);

	return true;
}

void Renderer::Update(const GameTimer& gt) {
	// Cycle through the circular frame resource array.
	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	// Has the GPU finished processing the commands of the current frame resource?
	// If not, wait until the GPU has completed commands up to this fence point.
	if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence) {
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	AnimateMaterials(gt);
	UpdateObjectCBsAndInstanceBuffer(gt);
	UpdateMaterialBuffer(gt);
	UpdateShadowTransform(gt);
	UpdateMainPassCB(gt);
	UpdateShadowPassCB(gt);
	UpdateSsaoCB(gt);
}

void Renderer::Draw(const GameTimer& gt) {
	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

	// Reuse the memory associated with command recording.
	// We can only reset when the associated command lists have finished execution on the GPU.
	ThrowIfFailed(cmdListAlloc->Reset());

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));

	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvSrvUavDescriptorHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	// Bind all the materials used in this scene.  For structured buffers, we can bypass the heap and 
	// set as a root descriptor.
	auto matBuffer = mCurrFrameResource->MaterialBuffer->Resource();
	mCommandList->SetGraphicsRootShaderResourceView(mRootParams.MatBufferIndex, matBuffer->GetGPUVirtualAddress());

	auto instBuffer = mCurrFrameResource->InstanceBuffer->Resource();
	mCommandList->SetGraphicsRootShaderResourceView(mRootParams.InstBufferIndex, instBuffer->GetGPUVirtualAddress());

	// Bind null SRV for shadow map pass.
	mCommandList->SetGraphicsRootDescriptorTable(mRootParams.MiscTextureMapIndex, mNullSrv);

	// Bind all the textures used in this scene.  Observe
	// that we only have to specify the first descriptor in the table.  
	// The root signature knows how many descriptors are expected in the table.
	mCommandList->SetGraphicsRootDescriptorTable(
		mRootParams.TextureMapIndex, 
		mCbvSrvUavDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

	// Bind the texture for extracing animations data.
	mCommandList->SetGraphicsRootDescriptorTable(
		mRootParams.AnimationsMapIndex,
		mAnimsMap->AnimationsMapSrv());

	DrawSceneToShadowMap();

	//
	// Normal/depth pass.
	//
	DrawNormalsAndDepth();
	
	//
	// Compute SSAO.
	//
	mCommandList->SetGraphicsRootSignature(mSsaoRootSignature.Get());
	mSsao->ComputeSsao(mCommandList.Get(), mCurrFrameResource, 3);

	//
	// Main rendering pass.
	//
	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	// Rebind state whenever graphics root signature changes.

	// Bind all the materials used in this scene.  For structured buffers, we can bypass the heap and 
	// set as a root descriptor.
	matBuffer = mCurrFrameResource->MaterialBuffer->Resource();
	mCommandList->SetGraphicsRootShaderResourceView(mRootParams.MatBufferIndex, matBuffer->GetGPUVirtualAddress());

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	// Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// Clear the back buffer and depth buffer.
	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(),
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Specify the buffers we are going to render to.
	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	auto passCB = mCurrFrameResource->PassCB->Resource();
	mCommandList->SetGraphicsRootConstantBufferView(mRootParams.PassCBIndex, passCB->GetGPUVirtualAddress());

	// Bind the sky cube map.  For our demos, we just use one "world" cube map representing the environment
	// from far away, so all objects will use the same cube map and we only need to set it once per-frame.  
	// If we wanted to use "local" cube maps, we would have to change them per-object, or dynamically
	// index into an array of cube maps.
	CD3DX12_GPU_DESCRIPTOR_HANDLE skyTexDescriptor(mCbvSrvUavDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	skyTexDescriptor.Offset(mDescHeapIdx.SkyTexHeapIndex, mCbvSrvUavDescriptorSize);

	mCommandList->SetGraphicsRootDescriptorTable(mRootParams.MiscTextureMapIndex, skyTexDescriptor);

	mCommandList->SetPipelineState(mPSOs["debug"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[RenderLayer::Debug]);

	mCommandList->SetPipelineState(mPSOs["skeleton"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[RenderLayer::Skeleton]);
	
	mCommandList->SetPipelineState(mPSOs["opaque"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[RenderLayer::Opaque]);

	mCommandList->SetPipelineState(mPSOs["skinnedOpaque"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[RenderLayer::SkinnedOpaque]);
	
	mCommandList->SetPipelineState(mPSOs["sky"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[RenderLayer::Sky]);

	//
	// Draw texts.
	//
	DrawTexts();
	

	// Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PRESENT));

	// Done recording commands.
	ThrowIfFailed(mCommandList->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Swap the back and front buffers
	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	mGraphicsMemory->Commit(mCommandQueue.Get());

	// Advance the fence value to mark commands up to this fence point.
	mCurrFrameResource->Fence = ++mCurrentFence;

	// Add an instruction to the command queue to set a new fence point. 
	// Because we are on the GPU timeline, the new fence point won't be 
	// set until the GPU finishes processing all the commands prior to this Signal().
	mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void Renderer::OnResize(UINT inClientWidth, UINT inClientHeight) {
	LowRenderer::OnResize(inClientWidth, inClientHeight);

	if (mMainCamera != nullptr)
		mMainCamera->SetLens(XM_PIDIV4, AspectRatio(), 0.1f, 1000.0f);

	if (mSsao != nullptr) {
		mSsao->OnResize(mClientWidth, mClientHeight);

		// Resources changed, so need to rebuild descriptors.
		mSsao->RebuildDescriptors(mDepthStencilBuffer.Get());
	}

	if (mMainCamera != nullptr)
		BoundingFrustum::CreateFromMatrix(mCamFrustum, mMainCamera->GetProj());
}

void Renderer::UpdateWorldTransform(const std::string& inRenderItemName, 
									const DirectX::XMMATRIX& inTransform, bool inIsSkeletal) {
	auto iter = mRefRitems.find(inRenderItemName);
	if (iter != mRefRitems.end()) {
		{
			auto& ritems = iter->second;
			for (auto ritem : ritems) {
				UINT index = mInstancesIndex[inRenderItemName];

				XMStoreFloat4x4(&ritem->Instances[index].World, inTransform);
				//ritem->Instances[index].NumFramesDirty = gNumFrameResources;
			}
		}
	
		std::string name = inRenderItemName + "_skeleton";
		if (inIsSkeletal) {
			auto skelIter = mRefRitems.find(name);
			if (skelIter != mRefRitems.cend()) {
				auto& ritems = skelIter->second;
				for (auto ritem : ritems) {
					UINT index = mInstancesIndex[name];

					XMStoreFloat4x4(&ritem->Instances[index].World, inTransform);
					//ritem->Instances[index].NumFramesDirty = gNumFrameResources;
				}
			}
		}
	}
}

void Renderer::UpdateInstanceAnimationData(const std::string& inRenderItemName, 
		UINT inAnimClipIdx, float inTimePos, bool inIsSkeletal) {
	{
		auto iter = mRefRitems.find(inRenderItemName);
		if (iter != mRefRitems.end()) {
			auto& ritems = iter->second;
			for (auto ritem : ritems) {
				UINT index = mInstancesIndex[inRenderItemName];

				ritem->Instances[index].AnimClipIndex = static_cast<UINT>(inAnimClipIdx * mAnimsMap->GetInvLineSize());
				ritem->Instances[index].TimePos = inTimePos;
			}
		}
	}

	{
		std::string name = inRenderItemName + "_skeleton";
		auto iter = mRefRitems.find(name);
		if (iter != mRefRitems.end()) {
			auto& ritems = iter->second;
			for (auto ritem : ritems) {
				UINT index = mInstancesIndex[inRenderItemName];

				ritem->Instances[index].AnimClipIndex = static_cast<UINT>(inAnimClipIdx * mAnimsMap->GetInvLineSize());
				ritem->Instances[index].TimePos = inTimePos;
			}
		}
	}
}

void Renderer::SetVisible(const std::string& inRenderItemName, bool inState) {
	auto iter = mRefRitems.find(inRenderItemName);
	if (iter != mRefRitems.cend()) {
		auto& ritems = iter->second;
		for (auto ritem : ritems) {
			UINT index = mInstancesIndex[inRenderItemName];

			if (inState)
				ritem->Instances[index].State &= ~EInstanceDataState::EID_Invisible;
			else
				ritem->Instances[index].State |= EInstanceDataState::EID_Invisible;
		}
	}
}

void Renderer::SetSkeletonVisible(const std::string& inRenderItemName, bool inState) {
	std::string name = inRenderItemName + "_skeleton";
	auto iter = mRefRitems.find(name);
	if (iter != mRefRitems.cend()) {
		auto& ritems = iter->second;
		for (auto ritem : ritems) {
			UINT index = mInstancesIndex[name];

			if (inState)
				ritem->Instances[index].State &= ~EInstanceDataState::EID_Invisible;
			else
				ritem->Instances[index].State |= EInstanceDataState::EID_Invisible;
		}
	}
}

void Renderer::AddGeometry(const Mesh* inMesh) {
	const auto& meshName = inMesh->GetMeshName();

	auto iter = mGeometries.find(meshName);
	if (iter != mGeometries.cend())
		return;

	FlushCommandQueue();

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = meshName;

	BoundingBox bound;

	if (inMesh->GetIsSkeletal())
		LoadDataFromSkeletalMesh(inMesh, geo.get(), bound);
	else
		LoadDataFromMesh(inMesh, geo.get(), bound);

	const auto& drawArgs = inMesh->GetDrawArgs();
	const auto& subsets = inMesh->GetSubsets();

	for (size_t i = 0; i < drawArgs.size(); ++i) {
		SubmeshGeometry submesh;
		submesh.IndexCount = subsets[i].first;
		submesh.StartIndexLocation = subsets[i].second;
		submesh.BaseVertexLocation = 0;
		submesh.AABB = bound;

		geo->DrawArgs[drawArgs[i]] = submesh;
	}

	mGeometries[geo->Name] = std::move(geo);

	if (inMesh->GetIsSkeletal())
		AddSkeletonGeometry(inMesh);

	// Execute the initialization commands.
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	FlushCommandQueue();

	if (!inMesh->GetMaterials().empty())
		AddMaterials(inMesh->GetMaterials());
}

void Renderer::DrawTexts() {
	mSpriteBatch->Begin(mCommandList.Get());

	std::wstringstream textsStream;
	for (const auto& text : mOutputTexts)
		textsStream << text << std::endl;

	const wchar_t* outputTexts = textsStream.str().c_str();
	auto origin = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
	auto scale = XMVectorSet(0.5f, 0.5f, 0.0f, 0.0f);

	// Left outline.
	mDefaultFont->DrawString(
		mSpriteBatch.get(),
		outputTexts,
		mTextPos - SimpleMath::Vector2(mTextPos.x * 0.1f, 0.0f),
		Colors::Black,
		0.0f,
		origin,
		scale);
	// Right outline.
	mDefaultFont->DrawString(
		mSpriteBatch.get(),
		outputTexts,
		mTextPos + SimpleMath::Vector2(mTextPos.x * 0.1f, 0.0f),
		Colors::Black,
		0.0f,
		origin,
		scale);
	// Top outline.
	mDefaultFont->DrawString(
		mSpriteBatch.get(),
		outputTexts,
		mTextPos - SimpleMath::Vector2(0.0f, mTextPos.y * 0.1f),
		Colors::Black,
		0.0f,
		origin,
		scale);
	// Bottom outline.
	mDefaultFont->DrawString(
		mSpriteBatch.get(),
		outputTexts,
		mTextPos + SimpleMath::Vector2(0.0f, mTextPos.y * 0.1f),
		Colors::Black,
		0.0f,
		origin,
		scale);
	// Draw texts.
	mDefaultFont->DrawString(
		mSpriteBatch.get(),
		outputTexts,
		mTextPos,
		Colors::White,
		0.0f,
		origin,
		scale);

	mSpriteBatch->End();
}

void Renderer::AddRenderItem(std::string& ioRenderItemName, const Mesh* inMesh) {
	auto iter = mRefRitems.find(ioRenderItemName);
	if (iter != mRefRitems.cend()) {
		std::stringstream sstream;
		UINT suffix = 1;

		do {
			sstream.str("");
			std::string modifiedName;
			sstream << ioRenderItemName << '_' << suffix++;
			modifiedName = sstream.str();
			iter = mRefRitems.find(modifiedName);
		} while (iter != mRefRitems.cend());

		ioRenderItemName = sstream.str();
	}

	bool isNested = false;
	{
		auto iter = std::find(mNestedMeshes.begin(), mNestedMeshes.end(), inMesh);
		if (iter != mNestedMeshes.end())
			isNested = true;
	}
	mNestedMeshes.push_back(inMesh);
	
	AddRenderItem(ioRenderItemName, inMesh, isNested);

	if (inMesh->GetIsSkeletal())
		AddSkeletonRenderItem(ioRenderItemName, inMesh, isNested);
}

void Renderer::AddMaterials(const std::unordered_map<std::string, MaterialIn>& inMaterials) {
	AddTextures(inMaterials);
	AddDescriptors(inMaterials);
	
	for (const auto& matList : inMaterials) {
		const auto& materialIn = matList.second;

		auto iter = mMaterials.find(materialIn.MaterialName);
		if (iter != mMaterials.cend())
			continue;
		
		auto material = std::make_unique<Material>();
		material->Name = materialIn.MaterialName;
		material->MatTransform = materialIn.MatTransform;
		material->MatCBIndex = mNumMatCB++;
		material->DiffuseSrvHeapIndex = mDiffuseSrvHeapIndices[materialIn.DiffuseMapFileName];
		material->NormalSrvHeapIndex = mNormalSrvHeapIndices[materialIn.NormalMapFileName];
		material->SpecularSrvHeapIndex = mSpecularSrvHeapIndices[materialIn.SpecularMapFileName];

		material->DiffuseAlbedo = materialIn.DiffuseAlbedo;
		material->FresnelR0 = materialIn.FresnelR0;
		material->Roughness = materialIn.Roughness;
		
		mMaterials[materialIn.MaterialName] = std::move(material);
	}
}

UINT Renderer::AddAnimations(const std::string& inClipName, const Animation& inAnim) {
	std::vector<std::vector<XMFLOAT4>> data;
	data.resize(inAnim.mNumFrames);

	for (size_t frame = 0; frame < inAnim.mNumFrames; ++frame) {
		for (const auto& curve : inAnim.mCurves) {
			for (int row = 0; row < 4; ++row) {
				data[frame].emplace_back(
					curve[frame].m[row][0],
					curve[frame].m[row][1],
					curve[frame].m[row][2],
					curve[frame].m[row][3]);
			}
		}		
	}

	return mAnimsMap->AddAnimation(inClipName, data);
}

void Renderer::UpdateAnimationsMap() {
	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	mAnimsMap->UpdateAnimationsMap();

	// Execute the initialization commands.
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	FlushCommandQueue();
}

void Renderer::AddOutputText(const std::wstring& inText, size_t inIdx) {
	size_t preperSize = inIdx + 1;
	if (mOutputTexts.size() < preperSize)
		mOutputTexts.resize(preperSize);

	mOutputTexts[inIdx] = inText;
}

void Renderer::RemoveOutputText(size_t inIdx) {
	auto size = mOutputTexts.size();
	if (size < inIdx + 1)
		return;

	size_t lastIdx = size - 1;
	auto temp = mOutputTexts[inIdx];
	mOutputTexts[inIdx] = mOutputTexts[lastIdx];
	mOutputTexts[lastIdx] = temp;
	mOutputTexts.pop_back();
}

ID3D12Device* Renderer::GetDevice() const {
	return LowRenderer::GetDevice();
}

ID3D12GraphicsCommandList* Renderer::GetCommandList() const {
	return LowRenderer::GetCommandList();
}

GameCamera* Renderer::GetMainCamera() const {
	return mMainCamera;
}

void Renderer::SetMainCamerea(GameCamera* inCamera) {
	mMainCamera = inCamera;
}

bool Renderer::IsValid() const {
	return LowRenderer::IsValid();
}

void Renderer::CreateRtvAndDsvDescriptorHeaps() {
	// Add +1 for screen normal map, +2 for ambient maps.
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = SwapChainBufferCount + 3;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(mRtvHeap.GetAddressOf())));

	// Add +1 DSV for shadow map.
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 2;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(mDsvHeap.GetAddressOf())));
}

void Renderer::AddRenderItem(const std::string& inRenderItemName, const Mesh* inMesh, bool inIsNested) {
	if (inIsNested) {
		auto iter = mMeshToRitem.find(inMesh);
		if (iter != mMeshToRitem.end()) {
			auto& ritems = iter->second;
			for (auto ritem : ritems) {
				ritem->Instances.push_back({ MathHelper::Identity4x4(), MathHelper::Identity4x4(),
					0.0f, (UINT)ritem->Mat->MatCBIndex, 0 });
				mRefRitems[inRenderItemName].push_back(ritem);
				mInstancesIndex[inRenderItemName] = static_cast<UINT>(ritem->Instances.size() - 1);
			}
		}
	}
	else {
		const auto& drawArgs = inMesh->GetDrawArgs();
		const auto& materials = inMesh->GetMaterials();

		for (size_t i = 0, end = drawArgs.size(); i < end; ++i) {
			auto ritem = std::make_unique<RenderItem>();
			ritem->ObjCBIndex = mNumObjCB++;

			ritem->Mat = mMaterials["default"].get();
			if (!materials.empty()) {
				auto iter = materials.find(drawArgs[i]);
				if (iter != materials.cend()) {
					auto matIter = mMaterials.find(iter->second.MaterialName);
					if (matIter != mMaterials.cend())
						ritem->Mat = matIter->second.get();
				}
			}

			ritem->Instances.push_back({
				MathHelper::Identity4x4(), MathHelper::Identity4x4(),
				0.0f, (UINT)ritem->Mat->MatCBIndex, 0 });
			ritem->Geo = mGeometries[inMesh->GetMeshName()].get();
			ritem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			ritem->IndexCount = ritem->Geo->DrawArgs[drawArgs[i]].IndexCount;
			ritem->StartIndexLocation = ritem->Geo->DrawArgs[drawArgs[i]].StartIndexLocation;
			ritem->BaseVertexLocation = ritem->Geo->DrawArgs[drawArgs[i]].BaseVertexLocation;
			ritem->AABB = ritem->Geo->DrawArgs[drawArgs[i]].AABB;

			if (inMesh->GetIsSkeletal())
				mRitemLayer[RenderLayer::SkinnedOpaque].push_back(ritem.get());
			else
				mRitemLayer[RenderLayer::Opaque].push_back(ritem.get());

			mMeshToRitem[inMesh].push_back(ritem.get());
			mRefRitems[inRenderItemName].push_back(ritem.get());
			mInstancesIndex[inRenderItemName] = 0;
			mAllRitems.push_back(std::move(ritem));
		}
	}
}

void Renderer::LoadDataFromMesh(const Mesh* inMesh, MeshGeometry* outGeo, BoundingBox& inBound) {
	const auto& vertices = inMesh->GetVertices();
	const auto& indices = inMesh->GetIndices();

	const size_t vertexSize = sizeof(Vertex);

	const UINT vbByteSize = static_cast<UINT>(vertices.size() * vertexSize);
	const UINT ibByteSize = static_cast<UINT>(indices.size() * sizeof(std::uint32_t));

	XMFLOAT3 vMinf3(+MathHelper::Infinity, +MathHelper::Infinity, +MathHelper::Infinity);
	XMFLOAT3 vMaxf3(-MathHelper::Infinity, -MathHelper::Infinity, -MathHelper::Infinity);

	XMVECTOR vMin = XMLoadFloat3(&vMinf3);
	XMVECTOR vMax = XMLoadFloat3(&vMaxf3); 

	for (size_t i = 0; i < vertices.size(); ++i) {
		XMVECTOR P = XMLoadFloat3(&vertices[i].Pos);

		vMin = XMVectorMin(vMin, P);
		vMax = XMVectorMax(vMax, P);
	}
	XMStoreFloat3(&inBound.Center, 0.5f * (vMax + vMin));
	XMStoreFloat3(&inBound.Extents, 0.5f * (vMax - vMin));

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &outGeo->VertexBufferCPU));
	CopyMemory(outGeo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &outGeo->IndexBufferCPU));
	CopyMemory(outGeo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	outGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, outGeo->VertexBufferUploader);

	outGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, outGeo->IndexBufferUploader);
	
	outGeo->VertexByteStride = (UINT)vertexSize;
	outGeo->VertexBufferByteSize = vbByteSize;
	outGeo->IndexFormat = DXGI_FORMAT_R32_UINT;
	outGeo->IndexBufferByteSize = ibByteSize;
}

void Renderer::LoadDataFromSkeletalMesh(const Mesh* mesh, MeshGeometry* geo, BoundingBox& inBound) {
	const auto& vertices = mesh->GetSkinnedVertices();
	const auto& indices = mesh->GetIndices();

	const size_t vertexSize = sizeof(SkinnedVertex);

	const UINT vbByteSize = static_cast<UINT>(vertices.size() * vertexSize);
	const UINT ibByteSize = static_cast<UINT>(indices.size() * sizeof(std::uint32_t));

	XMFLOAT3 vMinf3(+MathHelper::Infinity, +MathHelper::Infinity, +MathHelper::Infinity);
	XMFLOAT3 vMaxf3(-MathHelper::Infinity, -MathHelper::Infinity, -MathHelper::Infinity);

	XMVECTOR vMin = XMLoadFloat3(&vMinf3);
	XMVECTOR vMax = XMLoadFloat3(&vMaxf3);

	for (size_t i = 0; i < vertices.size(); ++i) {
		XMVECTOR P = XMLoadFloat3(&vertices[i].Pos);

		vMin = XMVectorMin(vMin, P);
		vMax = XMVectorMax(vMax, P);
	}
	XMStoreFloat3(&inBound.Center, 0.5f * (vMax + vMin));
	XMStoreFloat3(&inBound.Extents, 0.5f * (vMax - vMin));

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = (UINT)vertexSize;
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R32_UINT;
	geo->IndexBufferByteSize = ibByteSize;
}

void Renderer::AddSkeletonGeometry(const Mesh* inMesh) {
	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = inMesh->GetMeshName() + "_skeleton";

	const auto& vertices = inMesh->GetSkeletonVertices();
	const auto& indices = inMesh->GetSkeletonIndices();

	const size_t vertexSize = sizeof(Vertex);

	const UINT vbByteSize = static_cast<UINT>(vertices.size() * vertexSize);
	const UINT ibByteSize = static_cast<UINT>(indices.size() * sizeof(std::uint32_t));

	XMFLOAT3 vMinf3(+MathHelper::Infinity, +MathHelper::Infinity, +MathHelper::Infinity);
	XMFLOAT3 vMaxf3(-MathHelper::Infinity, -MathHelper::Infinity, -MathHelper::Infinity);

	XMVECTOR vMin = XMLoadFloat3(&vMinf3);
	XMVECTOR vMax = XMLoadFloat3(&vMaxf3);

	BoundingBox bound;

	for (size_t i = 0; i < vertices.size(); ++i) {
		XMVECTOR P = XMLoadFloat3(&vertices[i].Pos);

		vMin = XMVectorMin(vMin, P);
		vMax = XMVectorMax(vMax, P);
	}
	XMStoreFloat3(&bound.Center, 0.5f * (vMax + vMin));
	XMStoreFloat3(&bound.Extents, 0.5f * (vMax - vMin));

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = (UINT)vertexSize;
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R32_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;
	submesh.AABB = bound;

	geo->DrawArgs["skeleton"] = submesh;

	mGeometries[geo->Name] = std::move(geo);
}

void Renderer::AddSkeletonRenderItem(const std::string& inRenderItemName, const Mesh* inMesh, bool inIsNested) {
	if (inIsNested) {
		auto iter = mMeshToSkeletonRitem.find(inMesh);
		if (iter != mMeshToSkeletonRitem.end()) {
			std::string name = inRenderItemName + "_skeleton";
			auto& ritems = iter->second;			
			for (auto ritem : ritems) {
				ritem->Instances.push_back({ MathHelper::Identity4x4(), MathHelper::Identity4x4(),
					0.0f, (UINT)ritem->Mat->MatCBIndex, 0 });
				mRefRitems[name].push_back(ritem);
				mInstancesIndex[name] = static_cast<UINT>(ritem->Instances.size() - 1);
			}
		}
	}
	else {
		auto ritem = std::make_unique<RenderItem>();
		ritem->ObjCBIndex = mNumObjCB++;
		ritem->Mat = mMaterials["default"].get();
		ritem->Geo = mGeometries[inMesh->GetMeshName() + "_skeleton"].get();
		ritem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
		ritem->IndexCount = ritem->Geo->DrawArgs["skeleton"].IndexCount;
		ritem->Instances.push_back({ MathHelper::Identity4x4(), MathHelper::Identity4x4(),
			0.0f, (UINT)ritem->Mat->MatCBIndex, 0 });
		ritem->StartIndexLocation = ritem->Geo->DrawArgs["skeleton"].StartIndexLocation;
		ritem->BaseVertexLocation = ritem->Geo->DrawArgs["skeleton"].BaseVertexLocation;

		mRitemLayer[RenderLayer::Skeleton].push_back(ritem.get());

		std::string name = inRenderItemName + "_skeleton";
		mMeshToSkeletonRitem[inMesh].push_back(ritem.get());
		mRefRitems[name].push_back(ritem.get());
		mInstancesIndex[name] = 0;
		mAllRitems.push_back(std::move(ritem));
	}
}

void Renderer::AddTextures(const std::unordered_map<std::string, MaterialIn>& inMaterials) {
	FlushCommandQueue();

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));
	
	mDiffuseSrvHeapIndices[""] = 0;
	for (const auto& matList : inMaterials) {
		const auto& material = matList.second;

		if (material.DiffuseMapFileName.empty())
			continue;

		auto iter = mTextures.find(material.DiffuseMapFileName);
		if (iter != mTextures.end())
			continue;

		auto texMap = std::make_unique<Texture>();

		texMap->Name = material.DiffuseMapFileName;

		std::wstringstream wsstream;
		wsstream << textureFileNamePrefix << material.DiffuseMapFileName.c_str();
		texMap->Filename = wsstream.str();

		HRESULT status = DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
			mCommandList.Get(), texMap->Filename.c_str(),
			texMap->Resource, texMap->UploadHeap);

		if (FAILED(status)) {
			mDiffuseSrvHeapIndices[material.DiffuseMapFileName] = 0;
		}
		else {
			mDiffuseSrvHeapIndices[material.DiffuseMapFileName] = mDescHeapIdx.CurrSrvHeapIndex++;
			mTextures[texMap->Name] = std::move(texMap);
		}
	}

	mNormalSrvHeapIndices[""] = 1;
	for (const auto& matList : inMaterials) {
		const auto& material = matList.second;

		if (material.NormalMapFileName.empty())
			continue;

		auto iter = mTextures.find(material.NormalMapFileName);
		if (iter != mTextures.end())
			continue;
	
		auto texMap = std::make_unique<Texture>();

		texMap->Name = material.NormalMapFileName;
		
		std::wstringstream wsstream;
		wsstream << textureFileNamePrefix << material.NormalMapFileName.c_str();
		texMap->Filename = wsstream.str();

		HRESULT status = DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
			mCommandList.Get(), texMap->Filename.c_str(),
			texMap->Resource, texMap->UploadHeap);
	
		if (FAILED(status)) {
			mNormalSrvHeapIndices[material.NormalMapFileName] = 1;
		}
		else {
			mNormalSrvHeapIndices[material.NormalMapFileName] = mDescHeapIdx.CurrSrvHeapIndex++;
			mTextures[texMap->Name] = std::move(texMap);
		}
	}

	mSpecularSrvHeapIndices[""] = -1;
	for (const auto& matList : inMaterials) {
		const auto& material = matList.second;

		if (material.SpecularMapFileName.empty())
			continue;

		auto iter = mTextures.find(material.SpecularMapFileName);
		if (iter != mTextures.end())
			continue;

		auto texMap = std::make_unique<Texture>();

		texMap->Name = material.SpecularMapFileName;

		std::wstringstream wsstream;
		wsstream << textureFileNamePrefix << material.SpecularMapFileName.c_str();
		texMap->Filename = wsstream.str();

		HRESULT status = DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
			mCommandList.Get(), texMap->Filename.c_str(),
			texMap->Resource, texMap->UploadHeap);

		if (FAILED(status)) {
			mSpecularSrvHeapIndices[material.SpecularMapFileName] = -1;
		}
		else {
			mSpecularSrvHeapIndices[material.SpecularMapFileName] = mDescHeapIdx.CurrSrvHeapIndex++;
			mTextures[texMap->Name] = std::move(texMap);
		}
	}
	
	// Execute the initialization commands.
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	FlushCommandQueue();
}

void Renderer::AddDescriptors(const std::unordered_map<std::string, MaterialIn>& inMaterials) {
	FlushCommandQueue();

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mCbvSrvUavDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	hDescriptor.Offset(mNumDescriptor, mCbvSrvUavDescriptorSize);

	for (const auto& matList : inMaterials) {
		const auto& material = matList.second;

		if (material.DiffuseMapFileName.empty())
			continue;

		auto nestedIter = std::find(mBuiltDiffuseTexDescriptors.cbegin(), mBuiltDiffuseTexDescriptors.cend(), 
			material.DiffuseMapFileName);
		if (nestedIter != mBuiltDiffuseTexDescriptors.cend())
			continue;

		auto texIter = mTextures.find(material.DiffuseMapFileName);
		if (texIter == mTextures.end())
			continue;

		auto tex = mTextures[material.DiffuseMapFileName]->Resource;

		srvDesc.Format = tex->GetDesc().Format;
		srvDesc.Texture2D.MipLevels = tex->GetDesc().MipLevels;

		md3dDevice->CreateShaderResourceView(tex.Get(), &srvDesc, hDescriptor);

		hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);
		++mNumDescriptor;

		mBuiltDiffuseTexDescriptors.push_back(material.DiffuseMapFileName);
	}

	for (const auto& matList : inMaterials) {
		const auto& material = matList.second;

		if (material.NormalMapFileName.empty())
			continue;

		auto nestedIter = std::find(mBuiltNormalTexDescriptors.cbegin(), mBuiltNormalTexDescriptors.cend(), 
			material.NormalMapFileName);
		if (nestedIter != mBuiltNormalTexDescriptors.cend())
			continue;

		auto texIter = mTextures.find(material.NormalMapFileName);
		if (texIter == mTextures.end())
			continue;

		auto tex = mTextures[material.NormalMapFileName]->Resource;

		srvDesc.Format = tex->GetDesc().Format;
		srvDesc.Texture2D.MipLevels = tex->GetDesc().MipLevels;

		md3dDevice->CreateShaderResourceView(tex.Get(), &srvDesc, hDescriptor);

		hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);
		++mNumDescriptor;

		mBuiltNormalTexDescriptors.push_back(material.NormalMapFileName);
	}

	for (const auto& matList : inMaterials) {
		const auto& material = matList.second;

		if (material.SpecularMapFileName.empty())
			continue;

		auto nestedIter = std::find(mBuiltSpecularTexDescriptors.cbegin(), mBuiltSpecularTexDescriptors.cend(), 
			material.SpecularMapFileName);
		if (nestedIter != mBuiltSpecularTexDescriptors.cend())
			continue;

		auto texIter = mTextures.find(material.SpecularMapFileName);
		if (texIter == mTextures.end())
			continue;

		auto tex = mTextures[material.SpecularMapFileName]->Resource;

		srvDesc.Format = tex->GetDesc().Format;
		srvDesc.Texture2D.MipLevels = tex->GetDesc().MipLevels;

		md3dDevice->CreateShaderResourceView(tex.Get(), &srvDesc, hDescriptor);

		hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);
		++mNumDescriptor;

		mBuiltSpecularTexDescriptors.push_back(material.SpecularMapFileName);
	}

	// Execute the initialization commands.
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	FlushCommandQueue();
}

void Renderer::AnimateMaterials(const GameTimer& gt) {}

void Renderer::UpdateObjectCBsAndInstanceBuffer(const GameTimer& gt) {
	XMMATRIX view = mMainCamera->GetView();
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);

	UINT visibleObjectCount = 0;
	UINT index;

	auto currObjectCB = mCurrFrameResource->ObjectCB.get();
	auto currInstBuffer = mCurrFrameResource->InstanceBuffer.get();
	for (auto& e : mAllRitems) {
		index = e->ObjCBIndex * gMaxInstanceCount;
		UINT offset = 0;

		bool needToBeUpdated = false;
		for (auto& i : e->Instances) {
			XMMATRIX world = XMLoadFloat4x4(&i.World);
			XMMATRIX texTransform = XMLoadFloat4x4(&i.TexTransform);

			XMMATRIX invWorld = XMMatrixInverse(&XMMatrixDeterminant(world), world);
			
			// View space to the object's local space.
			XMMATRIX viewToLocal = XMMatrixMultiply(invView, invWorld);
			
			// Transform the camera frustum from view space to the object's local space.
			BoundingFrustum localSpaceFrustum;
			mCamFrustum.Transform(localSpaceFrustum, viewToLocal);

			if (i.State == EInstanceDataState::EID_DrawAlways || 
					(i.State & EInstanceDataState::EID_Invisible) == 0 &&
					(localSpaceFrustum.Contains(e->AABB) != DirectX::DISJOINT)) {
				// Only update the cbuffer data if the constants have changed.
				// This needs to be tracked per frame resource.
				//if (i.NumFramesDirty > 0) {
					InstanceData instData;
					XMStoreFloat4x4(&instData.World, XMMatrixTranspose(world));
					XMStoreFloat4x4(&instData.TexTransform, XMMatrixTranspose(texTransform));
					instData.TimePos = i.TimePos;
					instData.AnimClipIndex = i.AnimClipIndex;
					instData.MaterialIndex = i.MaterialIndex;					

					currInstBuffer->CopyData(index + offset, instData);

					// Next FrameResource need to be updated too.
					//--i.NumFramesDirty;
					needToBeUpdated = true;
				//}

				i.State &= ~EInstanceDataState::EID_Culled;
				++offset;
				++visibleObjectCount;
			}
			else {
				i.State |= EInstanceDataState::EID_Culled;
			}
		}

		e->NumInstancesToDraw = offset;

		if (needToBeUpdated) {
			ObjectConstants objConstants;
			objConstants.InstanceIndex = e->ObjCBIndex;
		
			currObjectCB->CopyData(e->ObjCBIndex, objConstants);
		}
	}

	AddOutputText(L"voc: " + std::to_wstring(visibleObjectCount), 2);
}

void Renderer::UpdateMaterialBuffer(const GameTimer& gt) {
	auto currMaterialBuffer = mCurrFrameResource->MaterialBuffer.get();
	for (auto& e : mMaterials) {
		// Only update the cbuffer data if the constants have changed.  If the cbuffer
		// data changes, it needs to be updated for each FrameResource.
		Material* mat = e.second.get();
		if (mat->NumFramesDirty > 0) {
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialData matData;
			matData.DiffuseAlbedo = mat->DiffuseAlbedo;
			matData.FresnelR0 = mat->FresnelR0;
			matData.Roughness = mat->Roughness;
			XMStoreFloat4x4(&matData.MatTransform, XMMatrixTranspose(matTransform));
			matData.DiffuseMapIndex = mat->DiffuseSrvHeapIndex;
			matData.NormalMapIndex = mat->NormalSrvHeapIndex;
			matData.SpecularMapIndex = mat->SpecularSrvHeapIndex;

			currMaterialBuffer->CopyData(mat->MatCBIndex, matData);

			// Next FrameResource need to be updated too.
			mat->NumFramesDirty--;
		}
	}
}

void Renderer::UpdateShadowTransform(const GameTimer& gt) {
	// Only the first "main" light casts a shadow.
	XMVECTOR lightDir = XMLoadFloat3(&mLightingVars.BaseLightDirections[0]);
	XMVECTOR lightPos = -2.0f*mSceneBounds.Radius*lightDir;
	XMVECTOR targetPos = XMLoadFloat3(&mSceneBounds.Center);
	XMVECTOR lightUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMMATRIX lightView = XMMatrixLookAtLH(lightPos, targetPos, lightUp);

	XMStoreFloat3(&mLightingVars.LightPosW, lightPos);

	// Transform bounding sphere to light space.
	XMFLOAT3 sphereCenterLS;
	XMStoreFloat3(&sphereCenterLS, XMVector3TransformCoord(targetPos, lightView));

	// Ortho frustum in light space encloses scene.
	float l = sphereCenterLS.x - mSceneBounds.Radius;
	float b = sphereCenterLS.y - mSceneBounds.Radius;
	float n = sphereCenterLS.z - mSceneBounds.Radius;
	float r = sphereCenterLS.x + mSceneBounds.Radius;
	float t = sphereCenterLS.y + mSceneBounds.Radius;
	float f = sphereCenterLS.z + mSceneBounds.Radius;

	mLightingVars.LightNearZ = n;
	mLightingVars.LightFarZ = f;
	XMMATRIX lightProj = XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);

	// Transform NDC space [-1,+1]^2 to texture space [0,1]^2
	XMMATRIX T(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f);
	
	XMMATRIX S = lightView * lightProj*T;
	XMStoreFloat4x4(&mLightingVars.LightView, lightView);
	XMStoreFloat4x4(&mLightingVars.LightProj, lightProj);
	XMStoreFloat4x4(&mLightingVars.ShadowTransform, S);
}

void Renderer::UpdateMainPassCB(const GameTimer& gt) {
	XMMATRIX view = mMainCamera->GetView();
	XMMATRIX proj = mMainCamera->GetProj();

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	// Transform NDC space [-1,+1]^2 to texture space [0,1]^2
	XMMATRIX T(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f);

	XMMATRIX viewProjTex = XMMatrixMultiply(viewProj, T);
	XMMATRIX shadowTransform = XMLoadFloat4x4(&mLightingVars.ShadowTransform);

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProjTex, XMMatrixTranspose(viewProjTex));
	XMStoreFloat4x4(&mMainPassCB.ShadowTransform, XMMatrixTranspose(shadowTransform));
	mMainPassCB.EyePosW = mMainCamera->GetPosition3f();
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.NearZ = 1.0f;
	mMainPassCB.FarZ = 1000.0f;
	mMainPassCB.TotalTime = gt.TotalTime();
	mMainPassCB.DeltaTime = gt.DeltaTime();
	mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
	mMainPassCB.Lights[0].Direction = mLightingVars.BaseLightDirections[0];
	mMainPassCB.Lights[0].Strength = mLightingVars.BaseLightStrengths[0];
	mMainPassCB.Lights[1].Direction = mLightingVars.BaseLightDirections[1];
	mMainPassCB.Lights[1].Strength = mLightingVars.BaseLightStrengths[1];
	mMainPassCB.Lights[2].Direction = mLightingVars.BaseLightDirections[2];
	mMainPassCB.Lights[2].Strength = mLightingVars.BaseLightStrengths[2];

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}

void Renderer::UpdateShadowPassCB(const GameTimer& gt) {
	XMMATRIX view = XMLoadFloat4x4(&mLightingVars.LightView);
	XMMATRIX proj = XMLoadFloat4x4(&mLightingVars.LightProj);

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	UINT w = mShadowMap->Width();
	UINT h = mShadowMap->Height();

	XMStoreFloat4x4(&mShadowPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mShadowPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mShadowPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mShadowPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mShadowPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mShadowPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	mShadowPassCB.EyePosW = mLightingVars.LightPosW;
	mShadowPassCB.RenderTargetSize = XMFLOAT2((float)w, (float)h);
	mShadowPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / w, 1.0f / h);
	mShadowPassCB.NearZ = mLightingVars.LightNearZ;
	mShadowPassCB.FarZ = mLightingVars.LightFarZ;

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(1, mShadowPassCB);
}

void Renderer::UpdateSsaoCB(const GameTimer& gt) {
	SsaoConstants ssaoCB;

	XMMATRIX P = mMainCamera->GetProj();

	// Transform NDC space [-1,+1]^2 to texture space [0,1]^2
	XMMATRIX T(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f);

	ssaoCB.Proj = mMainPassCB.Proj;
	ssaoCB.InvProj = mMainPassCB.InvProj;
	XMStoreFloat4x4(&ssaoCB.ProjTex, XMMatrixTranspose(P*T));

	mSsao->GetOffsetVectors(ssaoCB.OffsetVectors);

	auto blurWeights = mSsao->CalcGaussWeights(2.5f);
	ssaoCB.BlurWeights[0] = XMFLOAT4(&blurWeights[0]);
	ssaoCB.BlurWeights[1] = XMFLOAT4(&blurWeights[4]);
	ssaoCB.BlurWeights[2] = XMFLOAT4(&blurWeights[8]);

	ssaoCB.InvRenderTargetSize = XMFLOAT2(1.0f / mSsao->SsaoMapWidth(), 1.0f / mSsao->SsaoMapHeight());

	// Coordinates given in view space.
	ssaoCB.OcclusionRadius = 0.5f;
	ssaoCB.OcclusionFadeStart = 0.2f;
	ssaoCB.OcclusionFadeEnd = 2.0f;
	ssaoCB.SurfaceEpsilon = 0.05f;

	auto currSsaoCB = mCurrFrameResource->SsaoCB.get();
	currSsaoCB->CopyData(0, ssaoCB);
}

void Renderer::LoadBasicTextures() {
	std::vector<std::string> texNames = {
		"defaultDiffuseMap",
		"defaultNormalMap",
		"skyCubeMap",
		"blurSkyCubeMap"
	};

	std::vector<std::wstring> texFileNames = {
		textureFileNamePrefix + L"white1x1.dds",
		textureFileNamePrefix + L"default_nmap.dds",
		textureFileNamePrefix + L"skycube.dds",
		textureFileNamePrefix + L"blurskycube.dds"
	};

	for (int i = 0; i < texNames.size(); ++i) {
		auto texMap = std::make_unique<Texture>();
		texMap->Name = texNames[i];
		texMap->Filename = texFileNames[i];
		ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
			mCommandList.Get(), texMap->Filename.c_str(),
			texMap->Resource, texMap->UploadHeap));

		mTextures[texMap->Name] = std::move(texMap);
	}
}

namespace {
	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> GetStaticSamplers() {
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

		const CD3DX12_STATIC_SAMPLER_DESC shadow(
			6, // shaderRegister
			D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, // filter
			D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressU
			D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressV
			D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressW
			0.0f,                               // mipLODBias
			16,                                 // maxAnisotropy
			D3D12_COMPARISON_FUNC_LESS_EQUAL,
			D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK);

		return {
			pointWrap, pointClamp, linearWrap, linearClamp,
			anisotropicWrap, anisotropicClamp, shadow
		};
	}
}

void Renderer::BuildRootSignature() {
	CD3DX12_DESCRIPTOR_RANGE texTable0;
	texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 0, 0);

	CD3DX12_DESCRIPTOR_RANGE texTable1;
	texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 64, 4, 0);

	CD3DX12_DESCRIPTOR_RANGE texTable2;
	texTable2.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 68, 0);

	const UINT numParameters = 8;

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[numParameters];

	UINT cnt = 0;
	mRootParams.ObjectCBIndex = cnt++;
	mRootParams.PassCBIndex = cnt++;
	mRootParams.InstBufferIndex = cnt++;
	mRootParams.MatBufferIndex = cnt++;
	mRootParams.MiscTextureMapIndex = cnt++;
	mRootParams.TextureMapIndex = cnt++;
	mRootParams.AnimationsMapIndex = cnt++;

	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[mRootParams.ObjectCBIndex].InitAsConstantBufferView(
		0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
	slotRootParameter[mRootParams.PassCBIndex].InitAsConstantBufferView(1);
	slotRootParameter[mRootParams.InstBufferIndex].InitAsShaderResourceView(
		0, 1, D3D12_SHADER_VISIBILITY_VERTEX);
	slotRootParameter[mRootParams.MatBufferIndex].InitAsShaderResourceView(1, 1);
	slotRootParameter[mRootParams.MiscTextureMapIndex].InitAsDescriptorTable(
		1, &texTable0, D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[mRootParams.TextureMapIndex].InitAsDescriptorTable(
		1, &texTable1, D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[mRootParams.AnimationsMapIndex].InitAsDescriptorTable(
		1, &texTable2);

	auto staticSamplers = GetStaticSamplers();

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(numParameters, slotRootParameter,
		(UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void Renderer::BuildSsaoRootSignature() {
	CD3DX12_DESCRIPTOR_RANGE texTable0;
	texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0);

	CD3DX12_DESCRIPTOR_RANGE texTable1;
	texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0);

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[4];

	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[0].InitAsConstantBufferView(0);
	slotRootParameter[1].InitAsConstants(1, 1);
	slotRootParameter[2].InitAsDescriptorTable(1, &texTable0, D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[3].InitAsDescriptorTable(1, &texTable1, D3D12_SHADER_VISIBILITY_PIXEL);

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC depthMapSam(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressW
		0.0f,
		0,
		D3D12_COMPARISON_FUNC_LESS_EQUAL,
		D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE);

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	std::array<CD3DX12_STATIC_SAMPLER_DESC, 4> staticSamplers = {
		pointClamp, linearClamp, depthMapSam, linearWrap
	};

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter,
		(UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr) {
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mSsaoRootSignature.GetAddressOf())));
}

void Renderer::BuildDescriptorHeaps() {
	//
	// Create the SRV heap.
	//
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 64; // default: 14
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mCbvSrvUavDescriptorHeap)));

	//
	// Fill out the heap with actual descriptors.
	//
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mCbvSrvUavDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	std::vector<ComPtr<ID3D12Resource>> tex2DList = {
		mTextures["defaultDiffuseMap"]->Resource,
		mTextures["defaultNormalMap"]->Resource
	};

	auto skyCubeMap = mTextures["skyCubeMap"]->Resource;
	auto blurSkyCubeMap = mTextures["blurSkyCubeMap"]->Resource;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	for (size_t i = 0; i < tex2DList.size(); ++i) {
		srvDesc.Format = tex2DList[i]->GetDesc().Format;
		srvDesc.Texture2D.MipLevels = tex2DList[i]->GetDesc().MipLevels;
		md3dDevice->CreateShaderResourceView(tex2DList[i].Get(), &srvDesc, hDescriptor);

		// next descriptor
		hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);
	}
	
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.MipLevels = skyCubeMap->GetDesc().MipLevels;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	srvDesc.Format = skyCubeMap->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(skyCubeMap.Get(), &srvDesc, hDescriptor);

	hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);

	srvDesc.TextureCube.MipLevels = blurSkyCubeMap->GetDesc().MipLevels;
	srvDesc.Format = blurSkyCubeMap->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(blurSkyCubeMap.Get(), &srvDesc, hDescriptor);
	
	mDescHeapIdx.SkyTexHeapIndex = (UINT)tex2DList.size();
	mDescHeapIdx.BlurSkyTexHeapIndex = mDescHeapIdx.SkyTexHeapIndex + 1;
	mDescHeapIdx.ShadowMapHeapIndex = mDescHeapIdx.BlurSkyTexHeapIndex + 1;
	mDescHeapIdx.SsaoHeapIndexStart = mDescHeapIdx.ShadowMapHeapIndex + 1;
	mDescHeapIdx.SsaoAmbientMapIndex = mDescHeapIdx.SsaoHeapIndexStart + 3;
	mDescHeapIdx.AnimationsMapIndex = mDescHeapIdx.SsaoHeapIndexStart + 5;
	mDescHeapIdx.NullCubeSrvIndex = mDescHeapIdx.AnimationsMapIndex + 1;
	mDescHeapIdx.NullBlurCubeSrvIndex = mDescHeapIdx.NullCubeSrvIndex + 1;
	mDescHeapIdx.NullTexSrvIndex1 = mDescHeapIdx.NullBlurCubeSrvIndex + 1;
	mDescHeapIdx.NullTexSrvIndex2 = mDescHeapIdx.NullTexSrvIndex1 + 1;
	mDescHeapIdx.DefaultFontIndex = mDescHeapIdx.NullTexSrvIndex2 + 1;
	mDescHeapIdx.CurrSrvHeapIndex = mDescHeapIdx.DefaultFontIndex + 1;

	mNumDescriptor = mDescHeapIdx.CurrSrvHeapIndex;

	auto nullSrv = GetCpuSrv(mDescHeapIdx.NullCubeSrvIndex);
	mNullSrv = GetGpuSrv(mDescHeapIdx.NullCubeSrvIndex);
	
	md3dDevice->CreateShaderResourceView(nullptr, &srvDesc, nullSrv);

	nullSrv.Offset(1, mCbvSrvUavDescriptorSize);

	md3dDevice->CreateShaderResourceView(nullptr, &srvDesc, nullSrv);

	nullSrv.Offset(1, mCbvSrvUavDescriptorSize);
	
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	md3dDevice->CreateShaderResourceView(nullptr, &srvDesc, nullSrv);
	
	nullSrv.Offset(1, mCbvSrvUavDescriptorSize);

	md3dDevice->CreateShaderResourceView(nullptr, &srvDesc, nullSrv);

	mShadowMap->BuildDescriptors(
		GetCpuSrv(mDescHeapIdx.ShadowMapHeapIndex),
		GetGpuSrv(mDescHeapIdx.ShadowMapHeapIndex),
		GetDsv(1));

	mSsao->BuildDescriptors(
		mDepthStencilBuffer.Get(),
		GetCpuSrv(mDescHeapIdx.SsaoHeapIndexStart),
		GetGpuSrv(mDescHeapIdx.SsaoHeapIndexStart),
		GetRtv(SwapChainBufferCount),
		mCbvSrvUavDescriptorSize,
		mRtvDescriptorSize);

	mAnimsMap->BuildDescriptors(
		GetCpuSrv(mDescHeapIdx.AnimationsMapIndex),
		GetGpuSrv(mDescHeapIdx.AnimationsMapIndex));
}

void Renderer::BuildShadersAndInputLayout() {
	const D3D_SHADER_MACRO alphaTestDefines[] = {
		"ALPHA_TEST", "1",
		"BLUR_RADIUS_2", "1",
		NULL, NULL
	};

	const D3D_SHADER_MACRO skinnedDefines[] = {
		"SKINNED", "1",
		NULL, NULL
	};
	
	mShaders["standardVS"] = d3dUtil::CompileShader(
		shaderFileNamePrefix + L"Default.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["skinnedVS"] = d3dUtil::CompileShader(
		shaderFileNamePrefix + L"Default.hlsl", skinnedDefines, "VS", "vs_5_1");
	mShaders["skeletonVS"] = d3dUtil::CompileShader(
		shaderFileNamePrefix + L"Skeleton.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["shadowVS"] = d3dUtil::CompileShader(
		shaderFileNamePrefix + L"Shadows.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["skinnedShadowVS"] = d3dUtil::CompileShader(
		shaderFileNamePrefix + L"Shadows.hlsl", skinnedDefines, "VS", "vs_5_1");

	mShaders["skeletonGS"] = d3dUtil::CompileShader(
		shaderFileNamePrefix + L"Skeleton.hlsl", nullptr, "GS", "gs_5_1");

	mShaders["opaquePS"] = d3dUtil::CompileShader(
		shaderFileNamePrefix + L"Default.hlsl", alphaTestDefines, "PS", "ps_5_1");
	mShaders["skeletonPS"] = d3dUtil::CompileShader(
		shaderFileNamePrefix + L"Skeleton.hlsl", nullptr, "PS", "ps_5_1");
	mShaders["shadowOpaquePS"] = d3dUtil::CompileShader(
		shaderFileNamePrefix + L"Shadows.hlsl", nullptr, "PS", "ps_5_1");
	mShaders["shadowAlphaTestedPS"] = d3dUtil::CompileShader(
		shaderFileNamePrefix + L"Shadows.hlsl", alphaTestDefines, "PS", "ps_5_1");
	
	mShaders["debugVS"] = d3dUtil::CompileShader(
		shaderFileNamePrefix + L"ShadowDebug.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["debugPS"] = d3dUtil::CompileShader(
		shaderFileNamePrefix + L"ShadowDebug.hlsl", nullptr, "PS", "ps_5_1");
	
	mShaders["drawNormalsVS"] = d3dUtil::CompileShader(
		shaderFileNamePrefix + L"DrawNormals.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["skinnedDrawNormalsVS"] = d3dUtil::CompileShader(
		shaderFileNamePrefix + L"DrawNormals.hlsl", skinnedDefines, "VS", "vs_5_1");
	mShaders["drawNormalsPS"] = d3dUtil::CompileShader(
		shaderFileNamePrefix + L"DrawNormals.hlsl", nullptr, "PS", "ps_5_1");
	
	mShaders["ssaoVS"] = d3dUtil::CompileShader(
		shaderFileNamePrefix + L"Ssao.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["ssaoPS"] = d3dUtil::CompileShader(
		shaderFileNamePrefix + L"Ssao.hlsl", nullptr, "PS", "ps_5_1");

	mShaders["ssaoBlurVS"] = d3dUtil::CompileShader(
		shaderFileNamePrefix + L"SsaoBlur.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["ssaoBlurPS"] = d3dUtil::CompileShader(
		shaderFileNamePrefix + L"SsaoBlur.hlsl", nullptr, "PS", "ps_5_1");

	mShaders["skyVS"] = d3dUtil::CompileShader(
		shaderFileNamePrefix + L"Sky.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["skyPS"] = d3dUtil::CompileShader(
		shaderFileNamePrefix + L"Sky.hlsl", nullptr, "PS", "ps_5_1");
	
	mInputLayout = {
		{ "POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT,	0, 0,	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL",		0, DXGI_FORMAT_R32G32B32_FLOAT,	0, 12,	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD",	0, DXGI_FORMAT_R32G32_FLOAT,	0, 24,	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT",	0, DXGI_FORMAT_R32G32B32_FLOAT,	0, 32,	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	mSkinnedInputLayout = {
		{ "POSITION",		0, DXGI_FORMAT_R32G32B32_FLOAT,		0, 0,	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL",			0, DXGI_FORMAT_R32G32B32_FLOAT,		0, 12,	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD",		0, DXGI_FORMAT_R32G32_FLOAT,		0, 24,	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT",		0, DXGI_FORMAT_R32G32B32_FLOAT,		0, 32,	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "BONEWEIGHTS",	0, DXGI_FORMAT_R32G32B32A32_FLOAT,	0, 44,	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "BONEWEIGHTS",	1, DXGI_FORMAT_R32G32B32A32_FLOAT,	0, 60,	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "BONEINDICES",	0, DXGI_FORMAT_R32G32B32A32_SINT,	0, 76,	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "BONEINDICES",	1, DXGI_FORMAT_R32G32B32A32_SINT,	0, 92,	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
}

void Renderer::BuildBasicGeometry() {
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 3);
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(80.0f, 80.0f, 60, 40);
	GeometryGenerator::MeshData sphere = geoGen.CreateSphere(0.5f, 20, 20);
	GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);
	GeometryGenerator::MeshData quad = geoGen.CreateQuad(0.0f, 0.0f, 1.0f, 1.0f, 0.0f);

	//
	// We are concatenating all the geometry into one big vertex/index buffer.  So
	// define the regions in the buffer each submesh covers.
	//

	// Cache the vertex offsets to each object in the concatenated vertex buffer.
	UINT boxVertexOffset = 0;
	UINT gridVertexOffset = (UINT)box.Vertices.size();
	UINT sphereVertexOffset = gridVertexOffset + (UINT)grid.Vertices.size();
	UINT cylinderVertexOffset = sphereVertexOffset + (UINT)sphere.Vertices.size();
	UINT quadVertexOffset = cylinderVertexOffset + (UINT)cylinder.Vertices.size();

	// Cache the starting index for each object in the concatenated index buffer.
	UINT boxIndexOffset = 0;
	UINT gridIndexOffset = (UINT)box.Indices32.size();
	UINT sphereIndexOffset = gridIndexOffset + (UINT)grid.Indices32.size();
	UINT cylinderIndexOffset = sphereIndexOffset + (UINT)sphere.Indices32.size();
	UINT quadIndexOffset = cylinderIndexOffset + (UINT)cylinder.Indices32.size();

	SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount = (UINT)box.Indices32.size();
	boxSubmesh.StartIndexLocation = boxIndexOffset;
	boxSubmesh.BaseVertexLocation = boxVertexOffset;

	SubmeshGeometry gridSubmesh;
	gridSubmesh.IndexCount = (UINT)grid.Indices32.size();
	gridSubmesh.StartIndexLocation = gridIndexOffset;
	gridSubmesh.BaseVertexLocation = gridVertexOffset;

	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.IndexCount = (UINT)sphere.Indices32.size();
	sphereSubmesh.StartIndexLocation = sphereIndexOffset;
	sphereSubmesh.BaseVertexLocation = sphereVertexOffset;

	SubmeshGeometry cylinderSubmesh;
	cylinderSubmesh.IndexCount = (UINT)cylinder.Indices32.size();
	cylinderSubmesh.StartIndexLocation = cylinderIndexOffset;
	cylinderSubmesh.BaseVertexLocation = cylinderVertexOffset;

	SubmeshGeometry quadSubmesh;
	quadSubmesh.IndexCount = (UINT)quad.Indices32.size();
	quadSubmesh.StartIndexLocation = quadIndexOffset;
	quadSubmesh.BaseVertexLocation = quadVertexOffset;

	//
	// Extract the vertex elements we are interested in and pack the
	// vertices of all the meshes into one vertex buffer.
	//

	auto totalVertexCount =
		box.Vertices.size() +
		grid.Vertices.size() +
		sphere.Vertices.size() +
		cylinder.Vertices.size() +
		quad.Vertices.size();

	std::vector<Vertex> vertices(totalVertexCount);

	XMFLOAT3 vMinf3(+MathHelper::Infinity, +MathHelper::Infinity, +MathHelper::Infinity);
	XMFLOAT3 vMaxf3(-MathHelper::Infinity, -MathHelper::Infinity, -MathHelper::Infinity);

	XMVECTOR vMin = XMLoadFloat3(&vMinf3);
	XMVECTOR vMax = XMLoadFloat3(&vMaxf3);

	BoundingBox bound;

	UINT k = 0;
	for (size_t i = 0; i < box.Vertices.size(); ++i, ++k) {
		vertices[k].Pos = box.Vertices[i].Position;
		vertices[k].Normal = box.Vertices[i].Normal;
		vertices[k].TexC = box.Vertices[i].TexC;
		vertices[k].TangentU = box.Vertices[i].TangentU;

		XMVECTOR P = XMLoadFloat3(&vertices[k].Pos);

		vMin = XMVectorMin(vMin, P);
		vMax = XMVectorMax(vMax, P);
	}
	XMStoreFloat3(&bound.Center, 0.5f * (vMax + vMin));
	XMStoreFloat3(&bound.Extents, 0.5f * (vMax - vMin));
	boxSubmesh.AABB = bound;

	vMin = XMLoadFloat3(&vMinf3);
	vMax = XMLoadFloat3(&vMaxf3);
	for (size_t i = 0; i < grid.Vertices.size(); ++i, ++k) {
		vertices[k].Pos = grid.Vertices[i].Position;
		vertices[k].Normal = grid.Vertices[i].Normal;
		vertices[k].TexC = grid.Vertices[i].TexC;
		vertices[k].TangentU = grid.Vertices[i].TangentU;

		XMVECTOR P = XMLoadFloat3(&vertices[k].Pos);

		vMin = XMVectorMin(vMin, P);
		vMax = XMVectorMax(vMax, P);
	}
	XMStoreFloat3(&bound.Center, 0.5f * (vMax + vMin));
	XMStoreFloat3(&bound.Extents, 0.5f * (vMax - vMin));
	gridSubmesh.AABB = bound;

	vMin = XMLoadFloat3(&vMinf3);
	vMax = XMLoadFloat3(&vMaxf3);
	for (size_t i = 0; i < sphere.Vertices.size(); ++i, ++k) {
		vertices[k].Pos = sphere.Vertices[i].Position;
		vertices[k].Normal = sphere.Vertices[i].Normal;
		vertices[k].TexC = sphere.Vertices[i].TexC;
		vertices[k].TangentU = sphere.Vertices[i].TangentU;

		XMVECTOR P = XMLoadFloat3(&vertices[k].Pos);

		vMin = XMVectorMin(vMin, P);
		vMax = XMVectorMax(vMax, P);
	}
	XMStoreFloat3(&bound.Center, 0.5f * (vMax + vMin));
	XMStoreFloat3(&bound.Extents, 0.5f * (vMax - vMin));
	sphereSubmesh.AABB = bound;

	vMin = XMLoadFloat3(&vMinf3);
	vMax = XMLoadFloat3(&vMaxf3);
	for (size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k) {
		vertices[k].Pos = cylinder.Vertices[i].Position;
		vertices[k].Normal = cylinder.Vertices[i].Normal;
		vertices[k].TexC = cylinder.Vertices[i].TexC;
		vertices[k].TangentU = cylinder.Vertices[i].TangentU;

		XMVECTOR P = XMLoadFloat3(&vertices[k].Pos);

		vMin = XMVectorMin(vMin, P);
		vMax = XMVectorMax(vMax, P);
	}
	XMStoreFloat3(&bound.Center, 0.5f * (vMax + vMin));
	XMStoreFloat3(&bound.Extents, 0.5f * (vMax - vMin));
	cylinderSubmesh.AABB = bound;

	vMin = XMLoadFloat3(&vMinf3);
	vMax = XMLoadFloat3(&vMaxf3);
	for (int i = 0; i < quad.Vertices.size(); ++i, ++k) {
		vertices[k].Pos = quad.Vertices[i].Position;
		vertices[k].Normal = quad.Vertices[i].Normal;
		vertices[k].TexC = quad.Vertices[i].TexC;
		vertices[k].TangentU = quad.Vertices[i].TangentU;

		XMVECTOR P = XMLoadFloat3(&vertices[k].Pos);

		vMin = XMVectorMin(vMin, P);
		vMax = XMVectorMax(vMax, P);
	}
	XMStoreFloat3(&bound.Center, 0.5f * (vMax + vMin));
	XMStoreFloat3(&bound.Extents, 0.5f * (vMax - vMin));
	quadSubmesh.AABB = bound;

	std::vector<std::uint16_t> indices;
	indices.insert(indices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16()));
	indices.insert(indices.end(), std::begin(grid.GetIndices16()), std::end(grid.GetIndices16()));
	indices.insert(indices.end(), std::begin(sphere.GetIndices16()), std::end(sphere.GetIndices16()));
	indices.insert(indices.end(), std::begin(cylinder.GetIndices16()), std::end(cylinder.GetIndices16()));
	indices.insert(indices.end(), std::begin(quad.GetIndices16()), std::end(quad.GetIndices16()));

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "standardGeo";

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

	geo->DrawArgs["box"] = boxSubmesh;
	geo->DrawArgs["grid"] = gridSubmesh;
	geo->DrawArgs["sphere"] = sphereSubmesh;
	geo->DrawArgs["cylinder"] = cylinderSubmesh;
	geo->DrawArgs["quad"] = quadSubmesh;

	mGeometries[geo->Name] = std::move(geo);
}

void Renderer::BuildBasicMaterials() {
	auto defaultMat = std::make_unique<Material>();
	defaultMat->Name = "default";
	defaultMat->MatCBIndex = mNumMatCB++;
	defaultMat->DiffuseSrvHeapIndex = 0;
	defaultMat->NormalSrvHeapIndex = 1;
	defaultMat->SpecularSrvHeapIndex = -1;
	defaultMat->DiffuseAlbedo = XMFLOAT4(0.5f, 0.5f, 0.5f, 0.5f);
	defaultMat->FresnelR0 = XMFLOAT3(0.5f, 0.5f, 0.5f);
	defaultMat->Roughness = 0.5f;

	auto skyMat = std::make_unique<Material>();
	skyMat->Name = "sky";
	skyMat->MatCBIndex = mNumMatCB++;
	skyMat->DiffuseSrvHeapIndex = mDescHeapIdx.SkyTexHeapIndex;
	skyMat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	skyMat->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	skyMat->Roughness = 1.0f;

	mMaterials[defaultMat->Name] = std::move(defaultMat);
	mMaterials[skyMat->Name] = std::move(skyMat);
}

void Renderer::BuildBasicRenderItems() {
	XMFLOAT4X4 world;
	XMFLOAT4X4 tex;

	auto skyRitem = std::make_unique<RenderItem>();
	skyRitem->ObjCBIndex = mNumObjCB++;
	skyRitem->Mat = mMaterials["sky"].get();
	skyRitem->Geo = mGeometries["standardGeo"].get();
	skyRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	skyRitem->IndexCount = skyRitem->Geo->DrawArgs["sphere"].IndexCount;
	skyRitem->StartIndexLocation = skyRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
	skyRitem->BaseVertexLocation = skyRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;
	skyRitem->AABB = skyRitem->Geo->DrawArgs["sphere"].AABB;
	XMStoreFloat4x4(&world, XMMatrixScaling(5000.0f, 5000.0f, 5000.0f));
	skyRitem->Instances.push_back({ world, MathHelper::Identity4x4(),
		0.0f, (UINT)skyRitem->Mat->MatCBIndex, 0 });
	mRitemLayer[RenderLayer::Sky].push_back(skyRitem.get());
	mAllRitems.push_back(std::move(skyRitem));

	auto boxRitem = std::make_unique<RenderItem>();
	boxRitem->ObjCBIndex = mNumObjCB++;
	boxRitem->Mat = mMaterials["default"].get();
	boxRitem->Geo = mGeometries["standardGeo"].get();
	boxRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
	boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;
	boxRitem->AABB = boxRitem->Geo->DrawArgs["box"].AABB;
	XMStoreFloat4x4(&world, XMMatrixScaling(1.0f, 1.0f, 1.0f) * XMMatrixTranslation(0.0f, 0.5f, 0.0f));
	boxRitem->Instances.push_back({ world, MathHelper::Identity4x4(), 
		0.0f, (UINT)boxRitem->Mat->MatCBIndex, 0 });
	mRitemLayer[RenderLayer::Opaque].push_back(boxRitem.get());
	mAllRitems.push_back(std::move(boxRitem));

	auto quadRitem = std::make_unique<RenderItem>();
	quadRitem->ObjCBIndex = mNumObjCB++;
	quadRitem->Mat = mMaterials["default"].get();
	quadRitem->Geo = mGeometries["standardGeo"].get();
	quadRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	quadRitem->IndexCount = quadRitem->Geo->DrawArgs["quad"].IndexCount;
	quadRitem->StartIndexLocation = quadRitem->Geo->DrawArgs["quad"].StartIndexLocation;
	quadRitem->BaseVertexLocation = quadRitem->Geo->DrawArgs["quad"].BaseVertexLocation;
	quadRitem->AABB = quadRitem->Geo->DrawArgs["quad"].AABB;
	quadRitem->Instances.push_back({ MathHelper::Identity4x4(), MathHelper::Identity4x4(), 
		0.0f, (UINT)quadRitem->Mat->MatCBIndex, 0 });
	quadRitem->Instances[0].State = EInstanceDataState::EID_DrawAlways;
	mRitemLayer[RenderLayer::Debug].push_back(quadRitem.get());
	mAllRitems.push_back(std::move(quadRitem));

	auto gridRitem = std::make_unique<RenderItem>();
	gridRitem->ObjCBIndex = mNumObjCB++;
	gridRitem->Mat = mMaterials["default"].get();
	gridRitem->Geo = mGeometries["standardGeo"].get();
	gridRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
	gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;
	gridRitem->AABB = gridRitem->Geo->DrawArgs["grid"].AABB;
	XMStoreFloat4x4(&tex, XMMatrixScaling(8.0f, 8.0f, 1.0f));
	gridRitem->Instances.push_back({ MathHelper::Identity4x4(), tex, 
		0.0f, (UINT)gridRitem->Mat->MatCBIndex, 0 });
	mRitemLayer[RenderLayer::Opaque].push_back(gridRitem.get());
	mAllRitems.push_back(std::move(gridRitem));
}

void Renderer::BuildPSOs() {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

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
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
	opaquePsoDesc.SampleDesc.Count = 1;
	opaquePsoDesc.SampleDesc.Quality = 0;
	opaquePsoDesc.DSVFormat = mDepthStencilFormat;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));
	
	//
	// PSO for skinned pass.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC skinnedOpaquePsoDesc = opaquePsoDesc;
	skinnedOpaquePsoDesc.InputLayout = { mSkinnedInputLayout.data(), (UINT)mSkinnedInputLayout.size() };
	skinnedOpaquePsoDesc.VS = {
		reinterpret_cast<BYTE*>(mShaders["skinnedVS"]->GetBufferPointer()),
		mShaders["skinnedVS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skinnedOpaquePsoDesc, IID_PPV_ARGS(&mPSOs["skinnedOpaque"])));

	//
	// PSO for skeleton objects.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC skeletonPsoDesc = opaquePsoDesc;
	skeletonPsoDesc.VS = {
		reinterpret_cast<BYTE*>(mShaders["skeletonVS"]->GetBufferPointer()),
		mShaders["skeletonVS"]->GetBufferSize()
	};
	skeletonPsoDesc.GS = {
		reinterpret_cast<BYTE*>(mShaders["skeletonGS"]->GetBufferPointer()),
		mShaders["skeletonGS"]->GetBufferSize()
	};
	skeletonPsoDesc.PS = {
		reinterpret_cast<BYTE*>(mShaders["skeletonPS"]->GetBufferPointer()),
		mShaders["skeletonPS"]->GetBufferSize()
	};
	skeletonPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skeletonPsoDesc, IID_PPV_ARGS(&mPSOs["skeleton"])));

	//
	// PSO for shadow map pass.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC smapPsoDesc = opaquePsoDesc;
	smapPsoDesc.RasterizerState.DepthBias = 10000;
	smapPsoDesc.RasterizerState.DepthBiasClamp = 0.1f;
	smapPsoDesc.RasterizerState.SlopeScaledDepthBias = 1.0f;
	smapPsoDesc.pRootSignature = mRootSignature.Get();
	smapPsoDesc.VS = {
		reinterpret_cast<BYTE*>(mShaders["shadowVS"]->GetBufferPointer()),
		mShaders["shadowVS"]->GetBufferSize()
	};
	smapPsoDesc.PS = {
		reinterpret_cast<BYTE*>(mShaders["shadowOpaquePS"]->GetBufferPointer()),
		mShaders["shadowOpaquePS"]->GetBufferSize()
	};
	// Shadow map pass does not have a render target.
	smapPsoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
	smapPsoDesc.NumRenderTargets = 0;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&smapPsoDesc, IID_PPV_ARGS(&mPSOs["shadow_opaque"])));
	
	//
	// PSO for shadow map pass for skinned render items.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC skinnedSmapPsoDesc = smapPsoDesc;
	skinnedSmapPsoDesc.InputLayout = { mSkinnedInputLayout.data(), (UINT)mSkinnedInputLayout.size() };
	skinnedSmapPsoDesc.VS = {
		reinterpret_cast<BYTE*>(mShaders["skinnedShadowVS"]->GetBufferPointer()),
		mShaders["skinnedShadowVS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skinnedSmapPsoDesc, IID_PPV_ARGS(&mPSOs["skinnedShadow_opaque"])));

	//
	// PSO for debug layer.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC debugPsoDesc = opaquePsoDesc;
	debugPsoDesc.pRootSignature = mRootSignature.Get();
	debugPsoDesc.VS = {
		reinterpret_cast<BYTE*>(mShaders["debugVS"]->GetBufferPointer()),
		mShaders["debugVS"]->GetBufferSize()
	};
	debugPsoDesc.PS = {
		reinterpret_cast<BYTE*>(mShaders["debugPS"]->GetBufferPointer()),
		mShaders["debugPS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&debugPsoDesc, IID_PPV_ARGS(&mPSOs["debug"])));
	
	//
	// PSO for drawing normals.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC drawNormalsPsoDesc = opaquePsoDesc;
	drawNormalsPsoDesc.VS = {
		reinterpret_cast<BYTE*>(mShaders["drawNormalsVS"]->GetBufferPointer()),
		mShaders["drawNormalsVS"]->GetBufferSize()
	};
	drawNormalsPsoDesc.PS = {
		reinterpret_cast<BYTE*>(mShaders["drawNormalsPS"]->GetBufferPointer()),
		mShaders["drawNormalsPS"]->GetBufferSize()
	};
	drawNormalsPsoDesc.RTVFormats[0] = Ssao::NormalMapFormat;
	drawNormalsPsoDesc.SampleDesc.Count = 1;
	drawNormalsPsoDesc.SampleDesc.Quality = 0;
	drawNormalsPsoDesc.DSVFormat = mDepthStencilFormat;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&drawNormalsPsoDesc, IID_PPV_ARGS(&mPSOs["drawNormals"])));
	
	//
	// PSO for drawing normals for skinned render items.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC skinnedDrawNormalsPsoDesc = drawNormalsPsoDesc;
	skinnedDrawNormalsPsoDesc.InputLayout = { mSkinnedInputLayout.data(), (UINT)mSkinnedInputLayout.size() };
	skinnedDrawNormalsPsoDesc.VS = {
		reinterpret_cast<BYTE*>(mShaders["skinnedDrawNormalsVS"]->GetBufferPointer()),
		mShaders["skinnedDrawNormalsVS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skinnedDrawNormalsPsoDesc, IID_PPV_ARGS(&mPSOs["skinnedDrawNormals"])));

	//
	// PSO for SSAO.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC ssaoPsoDesc = opaquePsoDesc;
	ssaoPsoDesc.InputLayout = { nullptr, 0 };
	ssaoPsoDesc.pRootSignature = mSsaoRootSignature.Get();
	ssaoPsoDesc.VS = {
		reinterpret_cast<BYTE*>(mShaders["ssaoVS"]->GetBufferPointer()),
		mShaders["ssaoVS"]->GetBufferSize()
	};
	ssaoPsoDesc.PS = {
		reinterpret_cast<BYTE*>(mShaders["ssaoPS"]->GetBufferPointer()),
		mShaders["ssaoPS"]->GetBufferSize()
	};
	// SSAO effect does not need the depth buffer.
	ssaoPsoDesc.DepthStencilState.DepthEnable = false;
	ssaoPsoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	ssaoPsoDesc.RTVFormats[0] = Ssao::AmbientMapFormat;
	ssaoPsoDesc.SampleDesc.Count = 1;
	ssaoPsoDesc.SampleDesc.Quality = 0;
	ssaoPsoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&ssaoPsoDesc, IID_PPV_ARGS(&mPSOs["ssao"])));

	//
	// PSO for SSAO blur.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC ssaoBlurPsoDesc = ssaoPsoDesc;
	ssaoBlurPsoDesc.VS = {
		reinterpret_cast<BYTE*>(mShaders["ssaoBlurVS"]->GetBufferPointer()),
		mShaders["ssaoBlurVS"]->GetBufferSize()
	};
	ssaoBlurPsoDesc.PS = {
		reinterpret_cast<BYTE*>(mShaders["ssaoBlurPS"]->GetBufferPointer()),
		mShaders["ssaoBlurPS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&ssaoBlurPsoDesc, IID_PPV_ARGS(&mPSOs["ssaoBlur"])));

	//
	// PSO for sky.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC skyPsoDesc = opaquePsoDesc;
	// The camera is inside the sky sphere, so just turn off culling.
	skyPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	// Make sure the depth function is LESS_EQUAL and not just LESS.  
	// Otherwise, the normalized depth values at z = 1 (NDC) will 
	// fail the depth test if the depth buffer was cleared to 1.
	skyPsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	skyPsoDesc.pRootSignature = mRootSignature.Get();
	skyPsoDesc.VS = {
		reinterpret_cast<BYTE*>(mShaders["skyVS"]->GetBufferPointer()),
		mShaders["skyVS"]->GetBufferSize()
	};
	skyPsoDesc.PS = {
		reinterpret_cast<BYTE*>(mShaders["skyPS"]->GetBufferPointer()),
		mShaders["skyPS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skyPsoDesc, IID_PPV_ARGS(&mPSOs["sky"])));
}

void Renderer::BuildFrameResources() {
	for (UINT i = 0; i < gNumFrameResources; ++i) {
		mFrameResources.push_back(std::make_unique<FrameResource>(
			md3dDevice.Get(), 2, 128, gMaxInstanceCount, 128));
	}
}

void Renderer::DrawRenderItems(ID3D12GraphicsCommandList* outCmdList, const std::vector<RenderItem*>& inRitems) {
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	auto objectCB = mCurrFrameResource->ObjectCB->Resource();

	// For each render item...
	for (size_t i = 0, end = inRitems.size(); i < end; ++i) {
		auto ri = inRitems[i];

		outCmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
		outCmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
		outCmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;
		outCmdList->SetGraphicsRootConstantBufferView(mRootParams.ObjectCBIndex, objCBAddress);
		
		outCmdList->DrawIndexedInstanced(ri->IndexCount, ri->NumInstancesToDraw, 
			ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}

void Renderer::DrawSceneToShadowMap() {
	mCommandList->RSSetViewports(1, &mShadowMap->Viewport());
	mCommandList->RSSetScissorRects(1, &mShadowMap->ScissorRect());

	// Change to DEPTH_WRITE.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mShadowMap->Resource(),
		D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE));

	// Clear the back buffer and depth buffer.
	mCommandList->ClearDepthStencilView(mShadowMap->Dsv(),
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Specify the buffers we are going to render to.
	mCommandList->OMSetRenderTargets(0, nullptr, false, &mShadowMap->Dsv());

	// Bind the pass constant buffer for the shadow map pass.
	UINT passCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstants));
	auto passCB = mCurrFrameResource->PassCB->Resource();
	D3D12_GPU_VIRTUAL_ADDRESS passCBAddress = passCB->GetGPUVirtualAddress() + 1 * passCBByteSize;
	mCommandList->SetGraphicsRootConstantBufferView(mRootParams.PassCBIndex, passCBAddress);

	mCommandList->SetPipelineState(mPSOs["shadow_opaque"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[RenderLayer::Opaque]);

	mCommandList->SetPipelineState(mPSOs["skinnedShadow_opaque"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[RenderLayer::SkinnedOpaque]);

	// Change back to GENERIC_READ so we can read the texture in a shader.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mShadowMap->Resource(),
		D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ));
}

void Renderer::DrawNormalsAndDepth() {
	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	auto normalMap = mSsao->NormalMap();
	auto normalMapRtv = mSsao->NormalMapRtv();

	// Change to RENDER_TARGET.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(normalMap,
		D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// Clear the screen normal map and depth buffer.
	float clearValue[] = { 0.0f, 0.0f, 1.0f, 0.0f };
	mCommandList->ClearRenderTargetView(normalMapRtv, clearValue, 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Specify the buffers we are going to render to.
	mCommandList->OMSetRenderTargets(1, &normalMapRtv, true, &DepthStencilView());

	// Bind the constant buffer for this pass.
	auto passCB = mCurrFrameResource->PassCB->Resource();
	mCommandList->SetGraphicsRootConstantBufferView(mRootParams.PassCBIndex, passCB->GetGPUVirtualAddress());

	mCommandList->SetPipelineState(mPSOs["drawNormals"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[RenderLayer::Opaque]);

	mCommandList->SetPipelineState(mPSOs["skinnedDrawNormals"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[RenderLayer::SkinnedOpaque]);

	// Change back to GENERIC_READ so we can read the texture in a shader.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(normalMap,
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Renderer::GetCpuSrv(int inIndex) const {
	auto srv = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvSrvUavDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	srv.Offset(inIndex, mCbvSrvUavDescriptorSize);
	return srv;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Renderer::GetGpuSrv(int inIndex) const {
	auto srv = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvSrvUavDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	srv.Offset(inIndex, mCbvSrvUavDescriptorSize);
	return srv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Renderer::GetDsv(int inIndex) const {
	auto dsv = CD3DX12_CPU_DESCRIPTOR_HANDLE(mDsvHeap->GetCPUDescriptorHandleForHeapStart());
	dsv.Offset(inIndex, mDsvDescriptorSize);
	return dsv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Renderer::GetRtv(int inIndex) const {
	auto rtv = CD3DX12_CPU_DESCRIPTOR_HANDLE(mRtvHeap->GetCPUDescriptorHandleForHeapStart());
	rtv.Offset(inIndex, mRtvDescriptorSize);
	return rtv;
}
