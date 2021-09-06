#include "DX12Game/DxRenderer.h"

#include "DX12Game/GameWorld.h"
#include "DX12Game/GameCamera.h"
#include "DX12Game/Mesh.h"
#include "common/GeometryGenerator.h"

#include <ResourceUploadBatch.h>

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

namespace {
	const std::wstring TextureFileNamePrefix = L"./../../../../Assets/Textures/";
	const std::wstring ShaderFileNamePrefix = L".\\..\\..\\..\\..\\Assets\\Shaders\\";
	const std::wstring FontFileNamePrefix = L"./../../../../Assets/Fonts/";

	const float DefaultFontSize = 32;
	const float InvDefaultFontSize = 1.0f / DefaultFontSize;

	const float SceneBoundsRadius = 64.0f;
	const float SceneBoundQuarterRadius = SceneBoundsRadius * 0.75f;
}

DxRenderer::DxRenderer()
	: DxLowRenderer() {
	// Estimate the scene bounding sphere manually since we know how the scene was constructed.
	// The grid is the "widest object" with a width of 20 and depth of 30.0f, and centered at
	// the world space origin.  In general, you need to loop over every world space vertex
	// position and compute the bounding sphere.
	mSceneBounds.Center = XMFLOAT3(0.0f, 0.0f, 0.0f);
	mSceneBounds.Radius = sqrtf(SceneBoundsRadius * SceneBoundsRadius * 2.0f);

	mConstantSettings.resize(2);
	mConstantSettings[0] = 0.0f;
	mConstantSettings[1] = 128.0f;
}

DxRenderer::~DxRenderer() {
	if (!bIsCleaned)
		CleanUp();
}

GameResult DxRenderer::Initialize(HWND hMainWnd, UINT inClientWidth, UINT inClientHeight, UINT inNumThreads) {
	CheckGameResult(DxLowRenderer::Initialize(hMainWnd, inClientWidth, inClientHeight, inNumThreads));

#ifdef MT_World
	mUpdateBarrier = std::make_unique<CVBarrier>(mNumThreads);

	mNumInstances.resize(mNumThreads);
	mEachUpdateFunctions.resize(mNumThreads);

	{
		UINT i = 1;
		if (i >= mNumThreads) i = 0;

		UpdateFunc mainPassCB = &DxRenderer::UpdateMainPassCB;
		mEachUpdateFunctions[i++].push_back(mainPassCB);
		if (i >= mNumThreads) i = 0;

		UpdateFunc shadowPassCB = &DxRenderer::UpdateShadowPassCB;
		mEachUpdateFunctions[i++].push_back(shadowPassCB);
		if (i >= mNumThreads) i = 0;

		UpdateFunc ssaoCB = &DxRenderer::UpdateSsaoCB;
		mEachUpdateFunctions[i++].push_back(ssaoCB);
	}
#endif

	// Reset the command list to prep for initialization commands.
	for (UINT i = 0; i < mNumThreads; ++i)
		ReturnIfFailed(mCommandLists[i]->Reset(mCommandAllocators[i].Get(), nullptr));

	CheckGameResult(
		mGBuffer.Initialize(
			md3dDevice.Get(),
			mClientWidth,
			mClientHeight,
			mBackBufferFormat,
			DXGI_FORMAT_R16G16B16A16_FLOAT,
			DXGI_FORMAT_R24_UNORM_X8_TYPELESS,
			DXGI_FORMAT_R8G8B8A8_UNORM
		)
	);

	CheckGameResult(mShadowMap.Initialize(md3dDevice.Get(), 8096, 8096));

	ID3D12GraphicsCommandList* cmdList =
#ifdef MT_World
		mCommandLists[0].Get();
#else
		mCommandList.Get();
#endif

	CheckGameResult(mSsao.Initialize(md3dDevice.Get(), cmdList, mClientWidth, mClientHeight));
	CheckGameResult(mAnimsMap.Initialize(md3dDevice.Get(), cmdList));

	CheckGameResult(LoadBasicTextures());
	CheckGameResult(BuildRootSignature());
	CheckGameResult(BuildSsaoRootSignature());
	CheckGameResult(BuildDescriptorHeaps());
	CheckGameResult(BuildShadersAndInputLayout());
	CheckGameResult(BuildBasicGeometry());
	CheckGameResult(BuildBasicMaterials());
	CheckGameResult(BuildBasicRenderItems());
	CheckGameResult(BuildFrameResources());
	CheckGameResult(BuildPSOs());

	mSsao.SetPSOs(mPSOs["ssao"].Get(), mPSOs["ssaoBlur"].Get());

	// Execute the initialization commands.
#ifdef MT_World
	ExecuteCommandLists();
#else
	ReturnIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
#endif

	// Wait until initialization is complete.
	CheckGameResult(FlushCommandQueue());

	//
	// Initializes the things for drawing text.
	//
	mGraphicsMemory = std::make_unique<GraphicsMemory>(md3dDevice.Get());

	ResourceUploadBatch resourceUpload(md3dDevice.Get());
	resourceUpload.Begin();

	mDefaultFont = std::make_unique<SpriteFont>(
		md3dDevice.Get(),
		resourceUpload,
		(FontFileNamePrefix + L"D2Coding.spritefont").c_str(),
		GetCpuSrv(mDescHeapIdx.mDefaultFontIndex),
		GetGpuSrv(mDescHeapIdx.mDefaultFontIndex)
		);

	RenderTargetState rtState(mBackBufferFormat, mDepthStencilFormat);
	SpriteBatchPipelineStateDescription pd(rtState);
	mSpriteBatch = std::make_unique<SpriteBatch>(md3dDevice.Get(), resourceUpload, pd);

	auto uploadResourcesFinished = resourceUpload.End(mCommandQueue.Get());
	uploadResourcesFinished.wait();

	// After uploadResourcesFinished.wait() returned.
	mSpriteBatch->SetViewport(mScreenViewport);

	bIsValid = true;

	return GameResult(S_OK);
}

void DxRenderer::CleanUp() {
	DxLowRenderer::CleanUp();

	bIsCleaned = true;
}

GameResult DxRenderer::Update(const GameTimer& gt, UINT inTid) {
	if (inTid == 0) {
		// Cycle through the circular frame resource array.
		mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
		mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

		// Has the GPU finished processing the commands of the current frame resource?
		// If not, wait until the GPU has completed commands up to this fence point.
		if (mCurrFrameResource->mFence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->mFence) {
			HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
			ReturnIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->mFence, eventHandle));
			WaitForSingleObject(eventHandle, INFINITE);
			CloseHandle(eventHandle);
		}
	}

	SyncHost(mUpdateBarrier)

	CheckGameResult(AnimateMaterials(gt, inTid));
	CheckGameResult(UpdateObjectCBsAndInstanceBuffers(gt, inTid, mUpdateBarrier.get()));
	CheckGameResult(UpdateMaterialBuffers(gt, inTid, mUpdateBarrier.get()));
	
	if (inTid == 0)
		CheckGameResult(UpdateShadowTransform(gt));		

	SyncHost(mUpdateBarrier)

	for (auto& func : mEachUpdateFunctions[inTid])
		func(*this, std::ref(gt), inTid, mUpdateBarrier.get());
	
	if (inTid == 0) {
		GVector<std::string> textsToRemove;
		for (auto& text : mOutputTexts) {
			float& lifeTime = text.second.second.w;

			if (lifeTime == -1.0f)
				continue;

			lifeTime -= gt.DeltaTime();
			if (lifeTime <= 0.0f)
				textsToRemove.push_back(text.first);
		}

		for (const auto& text : textsToRemove)
			mOutputTexts.erase(text);
	}

	return GameResult(S_OK);
}

GameResult DxRenderer::Draw(const GameTimer& gt, UINT inTid) {
	if (inTid == 0) {
#ifdef MT_World
		auto cmdListAlloc = mCurrFrameResource->mCmdListAllocs[0];
#else
		auto cmdListAlloc = mCurrFrameResource->mCmdListAlloc;
#endif

		// Reuse the memory associated with command recording.
		// We can only reset when the associated command lists have finished execution on the GPU.
		ReturnIfFailed(cmdListAlloc->Reset());

		ID3D12GraphicsCommandList* cmdList =
#ifdef MT_World
			mCommandLists[0].Get();
#else
			mCommandList.Get();
#endif

		CheckGameResult(DrawSceneToShadowMap());
		CheckGameResult(DrawSceneToGBuffer());
		CheckGameResult(DrawSceneToRenderTarget());
	}

	return GameResult(S_OK);
}

GameResult DxRenderer::OnResize(UINT inClientWidth, UINT inClientHeight) {
	if (!mMainCamera)
		ReturnGameResult(S_FALSE, L"Main camera does not exist")

	CheckGameResult(DxLowRenderer::OnResize(inClientWidth, inClientHeight));

	mMainCamera->SetLens(XM_PI * 0.3f, AspectRatio(), 0.1f, 1000.0f);

	mGBuffer.OnResize(mClientWidth, mClientHeight, mDepthStencilBuffer.Get());

	mSsao.OnResize(mClientWidth, mClientHeight);

	// Resources changed, so need to rebuild descriptors.
	mSsao.RebuildDescriptors(mGBuffer.GetNormalMapSrv(), mGBuffer.GetDepthMapSrv());

	BoundingFrustum::CreateFromMatrix(mCamFrustum, mMainCamera->GetProj());

	return GameResult(S_OK);
}

void DxRenderer::UpdateWorldTransform(const std::string& inRenderItemName,
	const DirectX::XMMATRIX& inTransform, bool inIsSkeletal) {
	auto iter = mRefRitems.find(inRenderItemName);
	if (iter != mRefRitems.end()) {
		{
			auto& ritems = iter->second;
			for (auto ritem : ritems) {
				auto& inst = ritem->mInstances[mInstancesIndex[inRenderItemName]];

				XMStoreFloat4x4(&inst.mWorld, inTransform);
				inst.SetFramesDirty(gNumFrameResources);
			}
		}

		std::string name = inRenderItemName + "_skeleton";
		if (inIsSkeletal) {
			auto skelIter = mRefRitems.find(name);
			if (skelIter != mRefRitems.cend()) {
				auto& ritems = skelIter->second;
				for (auto ritem : ritems) {
					auto& inst = ritem->mInstances[mInstancesIndex[name]];

					XMStoreFloat4x4(&inst.mWorld, inTransform);
					inst.SetFramesDirty(gNumFrameResources);
				}
			}
		}
	}
}

void DxRenderer::UpdateInstanceAnimationData(const std::string& inRenderItemName,
	UINT inAnimClipIdx, float inTimePos, bool inIsSkeletal) {
		{
			auto iter = mRefRitems.find(inRenderItemName);
			if (iter != mRefRitems.end()) {
				auto& ritems = iter->second;
				for (auto ritem : ritems) {
					auto& inst = ritem->mInstances[mInstancesIndex[inRenderItemName]];

					inst.mAnimClipIndex = inAnimClipIdx == -1 ?
						-1 : static_cast<UINT>(inAnimClipIdx * mAnimsMap.GetInvLineSize());
					inst.mTimePos = inTimePos;
					inst.SetFramesDirty(gNumFrameResources);
				}
			}
		}

		{
			std::string name = inRenderItemName + "_skeleton";
			auto iter = mRefRitems.find(name);
			if (iter != mRefRitems.end()) {
				auto& ritems = iter->second;
				for (auto ritem : ritems) {
					auto& inst = ritem->mInstances[mInstancesIndex[name]];

					inst.mAnimClipIndex = inAnimClipIdx == -1 ?
						-1 : static_cast<UINT>(inAnimClipIdx * mAnimsMap.GetInvLineSize());
					inst.mTimePos = inTimePos;
					inst.SetFramesDirty(gNumFrameResources);
				}
			}
		}
}

void DxRenderer::SetVisible(const std::string& inRenderItemName, bool inState) {
	auto iter = mRefRitems.find(inRenderItemName);
	if (iter != mRefRitems.cend()) {
		auto& ritems = iter->second;
		for (auto ritem : ritems) {
			auto& inst = ritem->mInstances[mInstancesIndex[inRenderItemName]];

			if (inState)
				InstanceData::SetRenderState(inst.mRenderState, EInstanceRenderState::EID_Visible);
			else
				InstanceData::UnsetRenderState(inst.mRenderState, EInstanceRenderState::EID_Visible);
		}
	}
}

void DxRenderer::SetSkeletonVisible(const std::string& inRenderItemName, bool inState) {
	std::string name = inRenderItemName + "_skeleton";
	auto iter = mRefRitems.find(name);
	if (iter != mRefRitems.cend()) {
		auto& ritems = iter->second;
		for (auto ritem : ritems) {
			auto& inst = ritem->mInstances[mInstancesIndex[name]];

			if (inState)
				InstanceData::SetRenderState(inst.mRenderState, EInstanceRenderState::EID_Visible);
			else
				InstanceData::UnsetRenderState(inst.mRenderState, EInstanceRenderState::EID_Visible);
		}
	}
}

GameResult DxRenderer::AddGeometry(const Mesh* inMesh) {
	const auto& meshName = inMesh->GetMeshName();

	auto iter = mGeometries.find(meshName);
	if (iter != mGeometries.cend())
		return GameResult(S_OK);

	CheckGameResult(FlushCommandQueue());

	// Reset the command list to prep for initialization commands.
#ifdef MT_World
	ReturnIfFailed(mCommandLists[0]->Reset(mCommandAllocators[0].Get(), nullptr));
#else
	ReturnIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));
#endif

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = meshName;

	BoundingBox bound;

	if (inMesh->GetIsSkeletal()) {
		CheckGameResult(LoadDataFromSkeletalMesh(inMesh, geo.get(), bound));
	}
	else {
		CheckGameResult(LoadDataFromMesh(inMesh, geo.get(), bound));
	}

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
		CheckGameResult(AddSkeletonGeometry(inMesh));

	// Execute the initialization commands.
#ifdef MT_World
	ReturnIfFailed(mCommandLists[0]->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandLists[0].Get() };
#else
	ReturnIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };	
#endif
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	CheckGameResult(FlushCommandQueue());

	if (!inMesh->GetMaterials().empty())
		AddMaterials(inMesh->GetMaterials());

	return GameResult(S_OK);
}

void DxRenderer::DrawTexts() {
#ifdef MT_World
	mSpriteBatch->Begin(mCommandLists[0].Get());
#else
	mSpriteBatch->Begin(mCommandList.Get());
#endif

	auto origin = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);

	for (const auto& textIter : mOutputTexts) {
		const auto& text = textIter.second.first;

		const auto& refVec3 = textIter.second.second;
		SimpleMath::Vector2 textPos = { refVec3.x, refVec3.y };
		float scalef = refVec3.z * InvDefaultFontSize;
		auto scalev = XMVectorSet(scalef, scalef, 0.0f, 0.0f);

		const wchar_t* outputTexts = text.c_str();

		// Left outline.
		mDefaultFont->DrawString(
			mSpriteBatch.get(),
			outputTexts,
			textPos - SimpleMath::Vector2(1.0f, 0.0f),
			Colors::Black,
			0.0f,
			origin,
			scalev);
		// Right outline.
		mDefaultFont->DrawString(
			mSpriteBatch.get(),
			outputTexts,
			textPos + SimpleMath::Vector2(1.0f, 0.0f),
			Colors::Black,
			0.0f,
			origin,
			scalev);
		// Top outline.
		mDefaultFont->DrawString(
			mSpriteBatch.get(),
			outputTexts,
			textPos - SimpleMath::Vector2(0.0f, 1.0f),
			Colors::Black,
			0.0f,
			origin,
			scalev);
		// Bottom outline.
		mDefaultFont->DrawString(
			mSpriteBatch.get(),
			outputTexts,
			textPos + SimpleMath::Vector2(0.0f, 1.0f),
			Colors::Black,
			0.0f,
			origin,
			scalev);
		// Draw texts.
		mDefaultFont->DrawString(
			mSpriteBatch.get(),
			outputTexts,
			textPos,
			Colors::White,
			0.0f,
			origin,
			scalev);
	}

	mSpriteBatch->End();
}

void DxRenderer::AddRenderItem(std::string& ioRenderItemName, const Mesh* inMesh) {
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

GameResult DxRenderer::AddMaterials(const GUnorderedMap<std::string, MaterialIn>& inMaterials) {
	CheckGameResult(AddTextures(inMaterials));
	CheckGameResult(AddDescriptors(inMaterials));

	for (const auto& matList : inMaterials) {
		const auto& materialIn = matList.second;

		auto iter = mMaterialRefs.find(materialIn.MaterialName);
		if (iter != mMaterialRefs.cend())
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

		mMaterialRefs[materialIn.MaterialName] = material.get();
		mMaterials.push_back(std::move(material));
	}

	return GameResult(S_OK);
}

UINT DxRenderer::AddAnimations(const std::string& inClipName, const Animation& inAnim) {
	GVector<GVector<XMFLOAT4>> data;
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

	return mAnimsMap.AddAnimation(inClipName, data);
}

GameResult DxRenderer::UpdateAnimationsMap() {
	// Reset the command list to prep for initialization commands.
#ifdef MT_World
	ReturnIfFailed(mCommandLists[0]->Reset(mCommandAllocators[0].Get(), nullptr));
#else
	ReturnIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));
#endif

	mAnimsMap.UpdateAnimationsMap();

	// Execute the initialization commands.
#ifdef MT_World
	ReturnIfFailed(mCommandLists[0]->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandLists[0].Get() };
#else
	ReturnIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
#endif
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	CheckGameResult(FlushCommandQueue());

	return GameResult(S_OK);
}

GameResult DxRenderer::CreateRtvAndDsvDescriptorHeaps() {
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
#ifdef DeferredRendering
	// Add + 1 for diffuse map, +1 for screen normal map, +1 for specular map, +2 for ambient maps.
	rtvHeapDesc.NumDescriptors = SwapChainBufferCount + GBuffer::NumRenderTargets + 2;
#else
	// Add +1 for screen normal map, +2 for ambient maps.
	rtvHeapDesc.NumDescriptors = SwapChainBufferCount + 3;
#endif
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	ReturnIfFailed(md3dDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(mRtvHeap.GetAddressOf())));

	// Add +1 DSV for shadow map.
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 2;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	ReturnIfFailed(md3dDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(mDsvHeap.GetAddressOf())));

	return GameResult(S_OK);
}

void DxRenderer::AddRenderItem(const std::string& inRenderItemName, const Mesh* inMesh, bool inIsNested) {
	if (inIsNested) {
		auto iter = mMeshToRitem.find(inMesh);
		if (iter != mMeshToRitem.end()) {
			auto& ritems = iter->second;
			for (auto ritem : ritems) {
				mInstancesIndex[inRenderItemName] = static_cast<UINT>(ritem->mInstances.size());

				ritem->mInstances.emplace_back(
					MathHelper::Identity4x4(),
					MathHelper::Identity4x4(),
					0.0f,
					static_cast<UINT>(ritem->mMat->MatCBIndex)
				);

				mRefRitems[inRenderItemName].push_back(ritem);
			}
		}
	}
	else {
		const auto& drawArgs = inMesh->GetDrawArgs();
		const auto& materials = inMesh->GetMaterials();

		for (size_t i = 0, end = drawArgs.size(); i < end; ++i) {
			auto ritem = std::make_unique<RenderItem>();
			ritem->mObjCBIndex = mNumObjCB++;

			ritem->mMat = mMaterialRefs["default"];
			if (!materials.empty()) {
				auto iter = materials.find(drawArgs[i]);
				if (iter != materials.cend()) {
					auto matIter = mMaterialRefs.find(iter->second.MaterialName);
					if (matIter != mMaterialRefs.cend())
						ritem->mMat = matIter->second;
				}
			}

			ritem->mInstances.emplace_back(
				MathHelper::Identity4x4(),
				MathHelper::Identity4x4(),
				0.0f,
				static_cast<UINT>(ritem->mMat->MatCBIndex)
			);
			ritem->mGeo = mGeometries[inMesh->GetMeshName()].get();
			ritem->mPrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			ritem->mIndexCount = ritem->mGeo->DrawArgs[drawArgs[i]].IndexCount;
			ritem->mStartIndexLocation = ritem->mGeo->DrawArgs[drawArgs[i]].StartIndexLocation;
			ritem->mBaseVertexLocation = ritem->mGeo->DrawArgs[drawArgs[i]].BaseVertexLocation;
			ritem->mBoundType = BoundType::AABB;
			ritem->mBoundingUnion.mAABB = ritem->mGeo->DrawArgs[drawArgs[i]].AABB;

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

GameResult DxRenderer::LoadDataFromMesh(const Mesh* inMesh, MeshGeometry* outGeo, BoundingBox& inBound) {
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
		XMVECTOR P = XMLoadFloat3(&vertices[i].mPos);

		vMin = XMVectorMin(vMin, P);
		vMax = XMVectorMax(vMax, P);
	}
	XMStoreFloat3(&inBound.Center, 0.5f * (vMax + vMin));
	XMStoreFloat3(&inBound.Extents, 0.5f * (vMax - vMin));

	ReturnIfFailed(D3DCreateBlob(vbByteSize, &outGeo->VertexBufferCPU));
	CopyMemory(outGeo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ReturnIfFailed(D3DCreateBlob(ibByteSize, &outGeo->IndexBufferCPU));
	CopyMemory(outGeo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	ID3D12GraphicsCommandList* cmdList =
#ifdef MT_World
		mCommandLists[0].Get();
#else
		mCommandList.Get();
#endif

	CheckGameResult(D3D12Util::CreateDefaultBuffer(
		md3dDevice.Get(),
		cmdList,
		vertices.data(),
		vbByteSize,
		outGeo->VertexBufferUploader,
		outGeo->VertexBufferGPU)
	);

	CheckGameResult(D3D12Util::CreateDefaultBuffer(
		md3dDevice.Get(),
		cmdList,
		indices.data(),
		ibByteSize,
		outGeo->IndexBufferUploader,
		outGeo->IndexBufferGPU)
	);

	outGeo->VertexByteStride = static_cast<UINT>(vertexSize);
	outGeo->VertexBufferByteSize = vbByteSize;
	outGeo->IndexFormat = DXGI_FORMAT_R32_UINT;
	outGeo->IndexBufferByteSize = ibByteSize;

	return GameResult(S_OK);
}

GameResult DxRenderer::LoadDataFromSkeletalMesh(const Mesh* mesh, MeshGeometry* geo, BoundingBox& inBound) {
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
		XMVECTOR P = XMLoadFloat3(&vertices[i].mPos);

		vMin = XMVectorMin(vMin, P);
		vMax = XMVectorMax(vMax, P);
	}
	XMStoreFloat3(&inBound.Center, 0.5f * (vMax + vMin));
	XMStoreFloat3(&inBound.Extents, 0.5f * (vMax - vMin));

	ReturnIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ReturnIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	ID3D12GraphicsCommandList* cmdList =
#ifdef MT_World
		mCommandLists[0].Get();
#else
		mCommandList.Get();
#endif

	CheckGameResult(D3D12Util::CreateDefaultBuffer(
		md3dDevice.Get(),
		cmdList,
		vertices.data(),
		vbByteSize, 
		geo->VertexBufferUploader,
		geo->VertexBufferGPU)
	);

	CheckGameResult(D3D12Util::CreateDefaultBuffer(
		md3dDevice.Get(),
		cmdList,
		indices.data(),
		ibByteSize,
		geo->IndexBufferUploader, 
		geo->IndexBufferGPU)
	);

	geo->VertexByteStride = static_cast<UINT>(vertexSize);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R32_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	return GameResult(S_OK);
}

GameResult DxRenderer::AddSkeletonGeometry(const Mesh* inMesh) {
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
		XMVECTOR P = XMLoadFloat3(&vertices[i].mPos);

		vMin = XMVectorMin(vMin, P);
		vMax = XMVectorMax(vMax, P);
	}
	XMStoreFloat3(&bound.Center, 0.5f * (vMax + vMin));
	XMStoreFloat3(&bound.Extents, 0.5f * (vMax - vMin));

	ReturnIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ReturnIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	ID3D12GraphicsCommandList* cmdList =
#ifdef MT_World
		mCommandLists[0].Get();
#else
		mCommandList.Get();
#endif

	CheckGameResult(D3D12Util::CreateDefaultBuffer(
		md3dDevice.Get(),
		cmdList,
		vertices.data(), 
		vbByteSize,
		geo->VertexBufferUploader,
		geo->VertexBufferGPU)
	);

	CheckGameResult(D3D12Util::CreateDefaultBuffer(
		md3dDevice.Get(),
		cmdList,
		indices.data(), 
		ibByteSize, 
		geo->IndexBufferUploader, 
		geo->IndexBufferGPU)
	);

	geo->VertexByteStride = static_cast<UINT>(vertexSize);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R32_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = static_cast<UINT>(indices.size());
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;
	submesh.AABB = bound;

	geo->DrawArgs["skeleton"] = submesh;

	mGeometries[geo->Name] = std::move(geo);

	return GameResult(S_OK);
}

GameResult DxRenderer::AddSkeletonRenderItem(const std::string& inRenderItemName, const Mesh* inMesh, bool inIsNested) {
	if (inIsNested) {
		auto iter = mMeshToSkeletonRitem.find(inMesh);
		if (iter != mMeshToSkeletonRitem.end()) {
			std::string name = inRenderItemName + "_skeleton";
			auto& ritems = iter->second;
			for (auto ritem : ritems) {
				mInstancesIndex[name] = static_cast<UINT>(ritem->mInstances.size());

				ritem->mInstances.push_back({
					MathHelper::Identity4x4(),
					MathHelper::Identity4x4(),
					0.0f,
					static_cast<UINT>(ritem->mMat->MatCBIndex),
					0
					});

				mRefRitems[name].push_back(ritem);
			}
		}
	}
	else {
		auto ritem = std::make_unique<RenderItem>();
		ritem->mObjCBIndex = mNumObjCB++;
		ritem->mMat = mMaterialRefs["default"];
		ritem->mGeo = mGeometries[inMesh->GetMeshName() + "_skeleton"].get();
		ritem->mPrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
		ritem->mIndexCount = ritem->mGeo->DrawArgs["skeleton"].IndexCount;
		ritem->mInstances.push_back({
			MathHelper::Identity4x4(),
			MathHelper::Identity4x4(),
			0.0f,
			static_cast<UINT>(ritem->mMat->MatCBIndex),
			0
			});
		ritem->mStartIndexLocation = ritem->mGeo->DrawArgs["skeleton"].StartIndexLocation;
		ritem->mBaseVertexLocation = ritem->mGeo->DrawArgs["skeleton"].BaseVertexLocation;

		mRitemLayer[RenderLayer::Skeleton].push_back(ritem.get());

		std::string name = inRenderItemName + "_skeleton";
		mMeshToSkeletonRitem[inMesh].push_back(ritem.get());
		mRefRitems[name].push_back(ritem.get());
		mInstancesIndex[name] = 0;
		mAllRitems.push_back(std::move(ritem));
	}

	return GameResult(S_OK);
}

GameResult DxRenderer::AddTextures(const GUnorderedMap<std::string, MaterialIn>& inMaterials) {
	CheckGameResult(FlushCommandQueue());

	ID3D12GraphicsCommandList* cmdList =
#ifdef MT_World
		mCommandLists[0].Get();
#else
		mCommandList.Get();
#endif

	// Reset the command list to prep for initialization commands.
	ReturnIfFailed(cmdList->Reset(mCommandAllocators[0].Get(), nullptr));

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
		wsstream << TextureFileNamePrefix << material.DiffuseMapFileName.c_str();
		texMap->Filename = wsstream.str();

		HRESULT status = DirectX::CreateDDSTextureFromFile12(
			md3dDevice.Get(),
			cmdList,
			texMap->Filename.c_str(),
			texMap->Resource, 
			texMap->UploadHeap
		);

		if (FAILED(status)) {
			mDiffuseSrvHeapIndices[material.DiffuseMapFileName] = 0;
		}
		else {
			mDiffuseSrvHeapIndices[material.DiffuseMapFileName] = mDescHeapIdx.mCurrSrvHeapIndex++;
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
		wsstream << TextureFileNamePrefix << material.NormalMapFileName.c_str();
		texMap->Filename = wsstream.str();

		HRESULT status = DirectX::CreateDDSTextureFromFile12(
			md3dDevice.Get(),
			cmdList,
			texMap->Filename.c_str(),
			texMap->Resource,
			texMap->UploadHeap
		);

		if (FAILED(status)) {
			mNormalSrvHeapIndices[material.NormalMapFileName] = 1;
		}
		else {
			mNormalSrvHeapIndices[material.NormalMapFileName] = mDescHeapIdx.mCurrSrvHeapIndex++;
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
		wsstream << TextureFileNamePrefix << material.SpecularMapFileName.c_str();
		texMap->Filename = wsstream.str();

		HRESULT status = DirectX::CreateDDSTextureFromFile12(
			md3dDevice.Get(),
			cmdList,
			texMap->Filename.c_str(),
			texMap->Resource,
			texMap->UploadHeap
		);

		if (FAILED(status)) {
			mSpecularSrvHeapIndices[material.SpecularMapFileName] = -1;
		}
		else {
			mSpecularSrvHeapIndices[material.SpecularMapFileName] = mDescHeapIdx.mCurrSrvHeapIndex++;
			mTextures[texMap->Name] = std::move(texMap);
		}
	}

	// Execute the initialization commands.
	ReturnIfFailed(cmdList->Close());
	ID3D12CommandList* cmdsLists[] = { cmdList };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	CheckGameResult(FlushCommandQueue());

	return GameResult(S_OK);
}

GameResult DxRenderer::AddDescriptors(const GUnorderedMap<std::string, MaterialIn>& inMaterials) {
	CheckGameResult(FlushCommandQueue());

	// Reset the command list to prep for initialization commands.
#ifdef MT_World
	ReturnIfFailed(mCommandLists[0]->Reset(mCommandAllocators[0].Get(), nullptr));
#else
	ReturnIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));
#endif

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
#ifdef MT_World
	ReturnIfFailed(mCommandLists[0]->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandLists[0].Get() };
#else
	ReturnIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
#endif
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	CheckGameResult(FlushCommandQueue());

	return GameResult(S_OK);
}

///
// Update helper classes
///
bool DxRenderer::IsContained(BoundType inType, const RenderItem::BoundingStruct& inBound,
		const BoundingFrustum& inFrustum, UINT inTid) {

	if (inType == BoundType::AABB)
		return (inFrustum.Contains(inBound.mAABB) != DirectX::DISJOINT);
	else if (inType == BoundType::OBB)
		return (inFrustum.Contains(inBound.mOBB) != DirectX::DISJOINT);
	else if (inType == BoundType::Sphere)
		return (inFrustum.Contains(inBound.mSphere) != DirectX::DISJOINT);
	else
		return false;
}

UINT DxRenderer::UpdateEachInstances(RenderItem* inRitem) {
	auto& currInstDataBuffer = mCurrFrameResource->mInstanceDataBuffer;
	auto& currInstIdxBuffer = mCurrFrameResource->mInstanceIdxBuffer;

	XMMATRIX view = mMainCamera->GetView();
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);

	UINT offset = inRitem->mObjCBIndex * MaxInstanceCount;
	UINT accum = 0;
	UINT cnt = 0;

	for (auto& i : inRitem->mInstances) {
		XMMATRIX world = XMLoadFloat4x4(&i.mWorld);
		XMMATRIX texTransform = XMLoadFloat4x4(&i.mTexTransform);

		XMMATRIX invWorld = XMMatrixInverse(&XMMatrixDeterminant(world), world);

		// View space to the object's local space.
		XMMATRIX viewToLocal = XMMatrixMultiply(invView, invWorld);

		// Transform the camera frustum from view space to the object's local space.
		BoundingFrustum localSpaceFrustum;
		mCamFrustum.Transform(localSpaceFrustum, viewToLocal);

		if (InstanceData::IsMatched(i.mRenderState, EInstanceRenderState::EID_DrawAlways) ||
			InstanceData::IsMatched(i.mRenderState, EInstanceRenderState::EID_Visible) &&
			IsContained(inRitem->mBoundType, inRitem->mBoundingUnion, localSpaceFrustum)) {

			UINT instDataIdx = offset + cnt;

			InstanceIdxData instIdxData;
			instIdxData.mInstanceIdx = instDataIdx;

			currInstIdxBuffer.CopyData(offset + accum, instIdxData);

			// Only update the cbuffer data if the constants have changed.
			// This needs to be tracked per frame resource.
			if (i.CheckFrameDirty(mCurrFrameResourceIndex)) {
				InstanceData instData;
				XMStoreFloat4x4(&instData.mWorld, XMMatrixTranspose(world));
				XMStoreFloat4x4(&instData.mTexTransform, XMMatrixTranspose(texTransform));
				instData.mTimePos = i.mTimePos;
				instData.mAnimClipIndex = i.mAnimClipIndex;
				instData.mMaterialIndex = i.mMaterialIndex;

				currInstDataBuffer.CopyData(instDataIdx, instData);

				// Next FrameResource need to be updated too.
				i.UnsetFrameDirty(mCurrFrameResourceIndex);
			}

			++accum;
		}

		++cnt;
	}

	return accum;
}
/// Update helper classes

///
// Update functions
///
GameResult DxRenderer::AnimateMaterials(const GameTimer& gt, UINT inTid) {
	return GameResult(S_OK);
}

GameResult DxRenderer::UpdateObjectCBsAndInstanceBuffers(const GameTimer& gt, UINT inTid, ThreadBarrier* inBarrier) {
	if (!mMainCamera)
		ReturnGameResult(S_FALSE, L"Main camera does not exist");

	auto& currObjectCB = mCurrFrameResource->mObjectCB;

	UINT numRitems = static_cast<UINT>(mAllRitems.size());
	UINT eachNumRitems = numRitems / mNumThreads;
	UINT remaining = numRitems % mNumThreads;

	UINT begin = inTid * eachNumRitems + (inTid < remaining ? inTid : remaining);
	UINT end = begin + eachNumRitems + (inTid < remaining ? 1 : 0);

	for (UINT i = begin; i < end; ++i) {
		auto ritem = mAllRitems[i].get();

		mNumInstances[inTid] = UpdateEachInstances(ritem);
		ritem->mNumInstancesToDraw = mNumInstances[inTid];

		ObjectConstants objConstants;
		objConstants.mObjectIndex = ritem->mObjCBIndex;

		currObjectCB.CopyData(ritem->mObjCBIndex, objConstants);
	}
	
	if (inBarrier != nullptr)
		inBarrier->Wait();

	if (inTid == 0) {
		UINT visibleObjectCount = 0;

		for (UINT i = 0; i < mNumThreads; ++i)
			visibleObjectCount += mNumInstances[i];

		AddOutputText(
			"TEXT_VOC",
			L"voc: " + std::to_wstring(visibleObjectCount),
			static_cast<float>(mScreenViewport.Width * 0.01f),
			static_cast<float>(mScreenViewport.Height * 0.09f),
			16.0f
		);
	}

	return GameResult(S_OK);
}

GameResult DxRenderer::UpdateMaterialBuffers(const GameTimer& gt, UINT inTid, ThreadBarrier* inBarrier) {
	auto& currMaterialBuffer = mCurrFrameResource->mMaterialBuffer;

	UINT numMaterials = static_cast<UINT>(mMaterials.size());
	UINT eachNumMaterials = numMaterials / mNumThreads;
	UINT remaining = numMaterials % mNumThreads;
	
	UINT begin = inTid * eachNumMaterials + (inTid < remaining ? inTid : remaining);
	UINT end = begin + eachNumMaterials + (inTid < remaining ? 1 : 0);
	
	for (UINT i = begin; i < end; ++i) {	
		Material* mat = mMaterials[i].get();

		// Only update the cbuffer data if the constants have changed.  If the cbuffer
		// data changes, it needs to be updated for each FrameResource.
		if (mat->NumFramesDirty > 0) {
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialData matData;
			matData.mDiffuseAlbedo = mat->DiffuseAlbedo;
			matData.mFresnelR0 = mat->FresnelR0;
			matData.mRoughness = mat->Roughness;
			XMStoreFloat4x4(&matData.mMatTransform, XMMatrixTranspose(matTransform));
			matData.mDiffuseMapIndex = mat->DiffuseSrvHeapIndex;
			matData.mNormalMapIndex = mat->NormalSrvHeapIndex;
			matData.mSpecularMapIndex = mat->SpecularSrvHeapIndex;

			currMaterialBuffer.CopyData(mat->MatCBIndex, matData);

			// Next FrameResource need to be updated too.
			mat->NumFramesDirty--;
		}
	}

	return GameResult(S_OK);
}

GameResult DxRenderer::UpdateShadowTransform(const GameTimer& gt, UINT inTid, ThreadBarrier* inBarrier) {
	if (!mMainCamera)
		ReturnGameResult(S_FALSE, L"Main camera does not exist");

	XMFLOAT3 camPos = { 0.0f, 0.0f, 0.0f };
	XMFLOAT3 dirf = { 0.0f, 0.0f, 0.0f };

	camPos = mMainCamera->GetPosition3f();

	auto dir = mMainCamera->GetLook() * SceneBoundQuarterRadius;
	XMStoreFloat3(&dirf, dir);

	mSceneBounds.Center = XMFLOAT3(camPos.x + dirf.x, 0.0f, camPos.z + dirf.z);

	// Only the first "main" light casts a shadow.
	XMVECTOR lightDir = XMLoadFloat3(&mLightingVars.mBaseLightDirections[0]);
	XMVECTOR lightPos = -2.0f * mSceneBounds.Radius * lightDir + 
		XMVectorSet(camPos.x + dirf.x, 0.0f, camPos.z + dirf.z, 0.0f);
	XMVECTOR targetPos = XMLoadFloat3(&mSceneBounds.Center);
	XMVECTOR lightUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMMATRIX lightView = XMMatrixLookAtLH(lightPos, targetPos, lightUp);

	XMStoreFloat3(&mLightingVars.mLightPosW, lightPos);

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

	mLightingVars.mLightNearZ = n;
	mLightingVars.mLightFarZ = f;
	XMMATRIX lightProj = XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);

	// Transform NDC space [-1,+1]^2 to texture space [0,1]^2
	XMMATRIX T(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f
	);

	XMMATRIX S = lightView * lightProj*T;
	XMStoreFloat4x4(&mLightingVars.mLightView, lightView);
	XMStoreFloat4x4(&mLightingVars.mLightProj, lightProj);
	XMStoreFloat4x4(&mLightingVars.mShadowTransform, S);

	return GameResult(S_OK);
}

GameResult DxRenderer::UpdateMainPassCB(const GameTimer& gt, UINT inTid, ThreadBarrier* inBarrier) {
	if (!mMainCamera)
		ReturnGameResult(S_FALSE, L"Main camera does not exist");

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
		0.5f, 0.5f, 0.0f, 1.0f
	);

	XMMATRIX viewProjTex = XMMatrixMultiply(viewProj, T);
	XMMATRIX shadowTransform = XMLoadFloat4x4(&mLightingVars.mShadowTransform);

	XMStoreFloat4x4(&mMainPassCB.mView, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.mInvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.mProj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.mInvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.mViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.mInvViewProj, XMMatrixTranspose(invViewProj));
	XMStoreFloat4x4(&mMainPassCB.mViewProjTex, XMMatrixTranspose(viewProjTex));
	XMStoreFloat4x4(&mMainPassCB.mShadowTransform, XMMatrixTranspose(shadowTransform));
	mMainPassCB.mEyePosW = mMainCamera->GetPosition3f();
	mMainPassCB.mRenderTargetSize = XMFLOAT2(static_cast<float>(mClientWidth), static_cast<float>(mClientHeight));
	mMainPassCB.mInvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.mNearZ = 1.0f;
	mMainPassCB.mFarZ = 1000.0f;
	mMainPassCB.mTotalTime = gt.TotalTime();
	mMainPassCB.mDeltaTime = gt.DeltaTime();
	mMainPassCB.mAmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
	mMainPassCB.mLights[0].Direction = mLightingVars.mBaseLightDirections[0];
	mMainPassCB.mLights[0].Strength = mLightingVars.mBaseLightStrengths[0];
	mMainPassCB.mLights[1].Direction = mLightingVars.mBaseLightDirections[1];
	mMainPassCB.mLights[1].Strength = mLightingVars.mBaseLightStrengths[1];
	mMainPassCB.mLights[2].Direction = mLightingVars.mBaseLightDirections[2];
	mMainPassCB.mLights[2].Strength = mLightingVars.mBaseLightStrengths[2];

	auto& currPassCB = mCurrFrameResource->mPassCB;
	currPassCB.CopyData(0, mMainPassCB);

	return GameResult(S_OK);
}

GameResult DxRenderer::UpdateShadowPassCB(const GameTimer& gt, UINT inTid, ThreadBarrier* inBarrier) {
	XMMATRIX view = XMLoadFloat4x4(&mLightingVars.mLightView);
	XMMATRIX proj = XMLoadFloat4x4(&mLightingVars.mLightProj);

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	UINT w = mShadowMap.Width();
	UINT h = mShadowMap.Height();

	XMStoreFloat4x4(&mShadowPassCB.mView, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mShadowPassCB.mInvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mShadowPassCB.mProj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mShadowPassCB.mInvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mShadowPassCB.mViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mShadowPassCB.mInvViewProj, XMMatrixTranspose(invViewProj));
	mShadowPassCB.mEyePosW = mLightingVars.mLightPosW;
	mShadowPassCB.mRenderTargetSize = XMFLOAT2(static_cast<float>(w), static_cast<float>(h));
	mShadowPassCB.mInvRenderTargetSize = XMFLOAT2(1.0f / w, 1.0f / h);
	mShadowPassCB.mNearZ = mLightingVars.mLightNearZ;
	mShadowPassCB.mFarZ = mLightingVars.mLightFarZ;

	auto& currPassCB = mCurrFrameResource->mPassCB;
	currPassCB.CopyData(1, mShadowPassCB);

	return GameResult(S_OK);
}

GameResult DxRenderer::UpdateSsaoCB(const GameTimer& gt, UINT inTid, ThreadBarrier* inBarrier) {
	if (!mMainCamera)
		ReturnGameResult(S_FALSE, L"Main camera does not exist");

	SsaoConstants ssaoCB;

	XMMATRIX P = mMainCamera->GetProj();

	// Transform NDC space [-1,+1]^2 to texture space [0,1]^2
	XMMATRIX T(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f
	);

	ssaoCB.mProj = mMainPassCB.mProj;
	ssaoCB.mInvProj = mMainPassCB.mInvProj;
	XMStoreFloat4x4(&ssaoCB.mProjTex, XMMatrixTranspose(P*T));

	mSsao.GetOffsetVectors(ssaoCB.mOffsetVectors);

	auto blurWeights = mSsao.CalcGaussWeights(2.5f);
	ssaoCB.mBlurWeights[0] = XMFLOAT4(&blurWeights[0]);
	ssaoCB.mBlurWeights[1] = XMFLOAT4(&blurWeights[4]);
	ssaoCB.mBlurWeights[2] = XMFLOAT4(&blurWeights[8]);

	ssaoCB.mInvRenderTargetSize = XMFLOAT2(1.0f / mSsao.SsaoMapWidth(), 1.0f / mSsao.SsaoMapHeight());

	// Coordinates given in view space.
	ssaoCB.mOcclusionRadius = 0.5f;
	ssaoCB.mOcclusionFadeStart = 0.2f;
	ssaoCB.mOcclusionFadeEnd = 2.0f;
	ssaoCB.mSurfaceEpsilon = 0.05f;

	auto& currSsaoCB = mCurrFrameResource->mSsaoCB;
	currSsaoCB.CopyData(0, ssaoCB);

	return GameResult(S_OK);
}
/// Update functions

GameResult DxRenderer::LoadBasicTextures() {
	GVector<std::string> texNames = {
		"defaultDiffuseMap",
		"defaultNormalMap",
		"skyCubeMap",
		"blurSkyCubeMap"
	};

	GVector<std::wstring> texFileNames = {
		TextureFileNamePrefix + L"white1x1.dds",
		TextureFileNamePrefix + L"default_nmap.dds",
		TextureFileNamePrefix + L"skycube.dds",
		TextureFileNamePrefix + L"blurskycube.dds"
	};

	ID3D12GraphicsCommandList* cmdList =
#ifdef MT_World
		mCommandLists[0].Get();
#else
		mCommandList.Get();
#endif

	for (int i = 0; i < texNames.size(); ++i) {
		auto texMap = std::make_unique<Texture>();
		texMap->Name = texNames[i];
		texMap->Filename = texFileNames[i];
		ReturnIfFailed(
			DirectX::CreateDDSTextureFromFile12(
				md3dDevice.Get(),
				cmdList,
				texMap->Filename.c_str(),
				texMap->Resource,
				texMap->UploadHeap
			)
		);

		mTextures[texMap->Name] = std::move(texMap);
	}

	return GameResult(S_OK);
}

namespace {
	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 8> GetStaticSamplers() {
		// Applications usually only need a handful of samplers.  So just define them all up front
		// and keep them available as part of the root signature.  

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

		const CD3DX12_STATIC_SAMPLER_DESC shadow(
			6,													// shaderRegister
			D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,	// filter
			D3D12_TEXTURE_ADDRESS_MODE_BORDER,					// addressU
			D3D12_TEXTURE_ADDRESS_MODE_BORDER,					// addressV
			D3D12_TEXTURE_ADDRESS_MODE_BORDER,					// addressW
			0.0f,												// mipLODBias
			16,													// maxAnisotropy
			D3D12_COMPARISON_FUNC_LESS_EQUAL,
			D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK
		);

		const CD3DX12_STATIC_SAMPLER_DESC depthMapSam(
			7,									// shaderRegister
			D3D12_FILTER_MIN_MAG_MIP_LINEAR,	// filter
			D3D12_TEXTURE_ADDRESS_MODE_BORDER,	// addressU
			D3D12_TEXTURE_ADDRESS_MODE_BORDER,	// addressV
			D3D12_TEXTURE_ADDRESS_MODE_BORDER,	// addressW
			0.0f,
			0,
			D3D12_COMPARISON_FUNC_LESS_EQUAL,
			D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE
		);

		return {
			pointWrap, pointClamp, linearWrap, linearClamp,
			anisotropicWrap, anisotropicClamp, shadow, depthMapSam
		};
	}
}

GameResult DxRenderer::BuildRootSignature() {
	CD3DX12_DESCRIPTOR_RANGE texTable0;
	texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 8, 0, 0);

	CD3DX12_DESCRIPTOR_RANGE texTable1;
	texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 64, 8, 0);
	
	CD3DX12_DESCRIPTOR_RANGE texTable2;
	texTable2.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 72, 0);

	const UINT numParameters = 9;

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[numParameters];

	UINT cnt = 0;
	mRootParams.mObjectCBIndex = cnt++;
	mRootParams.mPassCBIndex = cnt++;
	mRootParams.mInstIdxBufferIndex = cnt++;
	mRootParams.mInstBufferIndex = cnt++;
	mRootParams.mMatBufferIndex = cnt++;
	mRootParams.mMiscTextureMapIndex = cnt++;
	mRootParams.mConstSettingsIndex = cnt++;
	mRootParams.mTextureMapIndex = cnt++;
	mRootParams.mAnimationsMapIndex = cnt++;

	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[mRootParams.mObjectCBIndex].InitAsConstantBufferView(
		0, 0, D3D12_SHADER_VISIBILITY_VERTEX
	);
	slotRootParameter[mRootParams.mPassCBIndex].InitAsConstantBufferView(
		1, 0, D3D12_SHADER_VISIBILITY_ALL
	);
	slotRootParameter[mRootParams.mInstIdxBufferIndex].InitAsShaderResourceView(
		0, 1, D3D12_SHADER_VISIBILITY_VERTEX
	);
	slotRootParameter[mRootParams.mInstBufferIndex].InitAsShaderResourceView(
		1, 1, D3D12_SHADER_VISIBILITY_VERTEX
	);
	slotRootParameter[mRootParams.mMatBufferIndex].InitAsShaderResourceView(
		2, 1, D3D12_SHADER_VISIBILITY_ALL
	);
	slotRootParameter[mRootParams.mMiscTextureMapIndex].InitAsDescriptorTable(
		1, &texTable0, D3D12_SHADER_VISIBILITY_PIXEL
	);
	slotRootParameter[mRootParams.mTextureMapIndex].InitAsDescriptorTable(
		1, &texTable1, D3D12_SHADER_VISIBILITY_PIXEL
	);
	slotRootParameter[mRootParams.mConstSettingsIndex].InitAsConstants(
		3, 2, 0, D3D12_SHADER_VISIBILITY_ALL
	);
	slotRootParameter[mRootParams.mAnimationsMapIndex].InitAsDescriptorTable(
		1, &texTable2, D3D12_SHADER_VISIBILITY_ALL
	);

	auto staticSamplers = GetStaticSamplers();

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(numParameters, slotRootParameter,
		static_cast<UINT>(staticSamplers.size()), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	{
		std::wstringstream wsstream;
		if (errorBlob != nullptr)
			wsstream << reinterpret_cast<char*>(errorBlob->GetBufferPointer());
			
		if (FAILED(hr))
			ReturnGameResult(S_FALSE, wsstream.str());
	}

	ReturnIfFailed(
		md3dDevice->CreateRootSignature(
			0,
			serializedRootSig->GetBufferPointer(),
			serializedRootSig->GetBufferSize(),
			IID_PPV_ARGS(mRootSignature.GetAddressOf())
		)
	);

	return GameResult(S_OK);
}

GameResult DxRenderer::BuildSsaoRootSignature() {
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
		0,									// shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT,		// filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,	// addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,	// addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP	// addressW
	);

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		1,									// shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,	// filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,	// addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,	// addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP);	// addressW

	const CD3DX12_STATIC_SAMPLER_DESC depthMapSam(
		2,									// shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,	// filter
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,	// addressU
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,	// addressV
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,	// addressW
		0.0f,
		0,
		D3D12_COMPARISON_FUNC_LESS_EQUAL,
		D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE
	);

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		3,									// shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,	// filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,	// addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,	// addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP		// addressW
	);

	std::array<CD3DX12_STATIC_SAMPLER_DESC, 4> staticSamplers = {
		pointClamp, linearClamp, depthMapSam, linearWrap
	};

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter,
		static_cast<UINT>(staticSamplers.size()), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
		::OutputDebugStringA(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
	ReturnIfFailed(hr);

	ReturnIfFailed(
		md3dDevice->CreateRootSignature(
			0,
			serializedRootSig->GetBufferPointer(),
			serializedRootSig->GetBufferSize(),
			IID_PPV_ARGS(mSsaoRootSignature.GetAddressOf())
		)
	);

	return GameResult(S_OK);
}

void DxRenderer::BuildDescriptorHeapIndices(UINT inOffset) {
	mDescHeapIdx.mSkyTexHeapIndex		= inOffset;
	mDescHeapIdx.mBlurSkyTexHeapIndex	= mDescHeapIdx.mSkyTexHeapIndex			+ 1;
	mDescHeapIdx.mDiffuseMapHeapIndex	= mDescHeapIdx.mBlurSkyTexHeapIndex		+ 1;
	mDescHeapIdx.mNormalMapHeapIndex	= mDescHeapIdx.mDiffuseMapHeapIndex		+ 1;
	mDescHeapIdx.mDepthMapHeapIndex		= mDescHeapIdx.mNormalMapHeapIndex		+ 1;
	mDescHeapIdx.mSpecularMapHeapIndex	= mDescHeapIdx.mDepthMapHeapIndex		+ 1;
	mDescHeapIdx.mShadowMapHeapIndex	= mDescHeapIdx.mSpecularMapHeapIndex	+ 1;
	mDescHeapIdx.mSsaoAmbientMapIndex	= mDescHeapIdx.mShadowMapHeapIndex		+ 1;
	mDescHeapIdx.mAnimationsMapIndex	= mDescHeapIdx.mSsaoAmbientMapIndex		+ 3;
	mDescHeapIdx.mNullCubeSrvIndex1		= mDescHeapIdx.mAnimationsMapIndex		+ 1;
	mDescHeapIdx.mNullCubeSrvIndex2		= mDescHeapIdx.mNullCubeSrvIndex1		+ 1;
	mDescHeapIdx.mNullTexSrvIndex1		= mDescHeapIdx.mNullCubeSrvIndex2		+ 1;
	mDescHeapIdx.mNullTexSrvIndex2		= mDescHeapIdx.mNullTexSrvIndex1		+ 1;
	mDescHeapIdx.mNullTexSrvIndex3		= mDescHeapIdx.mNullTexSrvIndex2		+ 1;
	mDescHeapIdx.mNullTexSrvIndex4		= mDescHeapIdx.mNullTexSrvIndex3		+ 1;
	mDescHeapIdx.mNullTexSrvIndex5		= mDescHeapIdx.mNullTexSrvIndex4		+ 1;
	mDescHeapIdx.mNullTexSrvIndex6		= mDescHeapIdx.mNullTexSrvIndex5		+ 1;
	mDescHeapIdx.mDefaultFontIndex		= mDescHeapIdx.mNullTexSrvIndex6		+ 1;
	mDescHeapIdx.mCurrSrvHeapIndex		= mDescHeapIdx.mDefaultFontIndex		+ 1;

	mNumDescriptor = mDescHeapIdx.mCurrSrvHeapIndex;
}

void DxRenderer::BuildNullShaderResourceViews() {
	auto skyCubeMap = mTextures["skyCubeMap"]->Resource;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.MipLevels = skyCubeMap->GetDesc().MipLevels;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	srvDesc.Format = skyCubeMap->GetDesc().Format;

	auto nullSrv = GetCpuSrv(mDescHeapIdx.mNullCubeSrvIndex1);
	mNullSrv = GetGpuSrv(mDescHeapIdx.mNullCubeSrvIndex1);

	for (INT i = 0; i < 2; ++i) {
		md3dDevice->CreateShaderResourceView(nullptr, &srvDesc, nullSrv);

		nullSrv.Offset(1, mCbvSrvUavDescriptorSize);
	}

	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	for (INT i = 0; i < 6; ++i) {
		md3dDevice->CreateShaderResourceView(nullptr, &srvDesc, nullSrv);

		nullSrv.Offset(1, mCbvSrvUavDescriptorSize);
	}
}

void DxRenderer::BuildDescriptorsForEachHelperClass() {
	mGBuffer.BuildDescriptors(
		mDepthStencilBuffer.Get(),
		GetCpuSrv(mDescHeapIdx.mDiffuseMapHeapIndex),
		GetGpuSrv(mDescHeapIdx.mDiffuseMapHeapIndex),
		GetRtv(SwapChainBufferCount),
		mCbvSrvUavDescriptorSize,
		mRtvDescriptorSize
	);

	mShadowMap.BuildDescriptors(
		GetCpuSrv(mDescHeapIdx.mShadowMapHeapIndex),
		GetGpuSrv(mDescHeapIdx.mShadowMapHeapIndex),
		GetDsv(1)
	);

	mSsao.BuildDescriptors(
		GetGpuSrv(mDescHeapIdx.mNormalMapHeapIndex),
		GetGpuSrv(mDescHeapIdx.mDepthMapHeapIndex),
		GetCpuSrv(mDescHeapIdx.mSsaoAmbientMapIndex),
		GetGpuSrv(mDescHeapIdx.mSsaoAmbientMapIndex),
		GetRtv(SwapChainBufferCount + GBuffer::NumRenderTargets),
		mCbvSrvUavDescriptorSize,
		mRtvDescriptorSize
	);

	mAnimsMap.BuildDescriptors(
		GetCpuSrv(mDescHeapIdx.mAnimationsMapIndex),
		GetGpuSrv(mDescHeapIdx.mAnimationsMapIndex)
	);
}

GameResult DxRenderer::BuildDescriptorHeaps() {
	//
	// Create the SRV heap.
	//
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 64; // default: 14, gbuffer: ?
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ReturnIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mCbvSrvUavDescriptorHeap)));

	//
	// Fill out the heap with actual descriptors.
	//
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mCbvSrvUavDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	GVector<ComPtr<ID3D12Resource>> tex2DList = {
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

	BuildDescriptorHeapIndices(static_cast<UINT>(tex2DList.size()));
	BuildNullShaderResourceViews();
	BuildDescriptorsForEachHelperClass();

	return GameResult(S_OK);
}

GameResult DxRenderer::BuildShadersAndInputLayout() {
	const D3D_SHADER_MACRO alphaTestDefines[] = {
		"ALPHA_TEST", "1",
		"BLUR_RADIUS_3", "1",
		NULL, NULL
	};

	const D3D_SHADER_MACRO skinnedDefines[] = {
		"SKINNED", "1",
		NULL, NULL
	};

	{
		std::wstring defaulthlsl = ShaderFileNamePrefix + L"Default.hlsl";
		CheckGameResult(D3D12Util::CompileShader(defaulthlsl, nullptr,			"VS", "vs_5_1", mShaders["standardVS"]));
		CheckGameResult(D3D12Util::CompileShader(defaulthlsl, alphaTestDefines, "PS", "ps_5_1", mShaders["opaquePS"]));
		CheckGameResult(D3D12Util::CompileShader(defaulthlsl, skinnedDefines,	"VS", "vs_5_1", mShaders["skinnedVS"]));
	}

	{
		std::wstring skeletonhlsl = ShaderFileNamePrefix + L"Skeleton.hlsl";
		CheckGameResult(D3D12Util::CompileShader(skeletonhlsl, nullptr, "VS", "vs_5_1", mShaders["skeletonVS"]));
		CheckGameResult(D3D12Util::CompileShader(skeletonhlsl, nullptr, "GS", "gs_5_1", mShaders["skeletonGS"]));
		CheckGameResult(D3D12Util::CompileShader(skeletonhlsl, nullptr, "PS", "ps_5_1", mShaders["skeletonPS"]));
	}

	{
		std::wstring waveshlsl = ShaderFileNamePrefix + L"Waves.hlsl";
		CheckGameResult(D3D12Util::CompileShader(waveshlsl, nullptr, "VS", "vs_5_1", mShaders["wavesVS"]));
		CheckGameResult(D3D12Util::CompileShader(waveshlsl, nullptr, "HS", "hs_5_1", mShaders["wavesHS"]));
		CheckGameResult(D3D12Util::CompileShader(waveshlsl, nullptr, "DS", "ds_5_1", mShaders["wavesDS"]));
		CheckGameResult(D3D12Util::CompileShader(waveshlsl, nullptr, "PS", "ps_5_1", mShaders["wavesPS"]));
	}

	{
		std::wstring shadowhlsl = ShaderFileNamePrefix + L"Shadows.hlsl";
		CheckGameResult(D3D12Util::CompileShader(
			shadowhlsl, nullptr, "VS", "vs_5_1", mShaders["shadowVS"])
		);
		CheckGameResult(D3D12Util::CompileShader(
			shadowhlsl, skinnedDefines, "VS", "vs_5_1", mShaders["skinnedShadowVS"])
		);
		CheckGameResult(D3D12Util::CompileShader(
			shadowhlsl, alphaTestDefines, "PS", "ps_5_1", mShaders["shadowOpaquePS"])
		);
	}

	{
		std::wstring drawdebughlsl = ShaderFileNamePrefix + L"DrawDebug.hlsl";
		CheckGameResult(D3D12Util::CompileShader(drawdebughlsl, nullptr, "VS", "vs_5_1", mShaders["debugVS"]));
		CheckGameResult(D3D12Util::CompileShader(drawdebughlsl, nullptr, "PS", "ps_5_1", mShaders["debugPS"]));
	}

	{
		std::wstring drawnormalhlsl = ShaderFileNamePrefix + L"DrawNormals.hlsl";
		CheckGameResult(
			D3D12Util::CompileShader(drawnormalhlsl, nullptr, "VS", "vs_5_1", mShaders["drawNormalsVS"])
		);
		CheckGameResult(
			D3D12Util::CompileShader(drawnormalhlsl, skinnedDefines, "VS", "vs_5_1", mShaders["skinnedDrawNormalsVS"])
		);
		CheckGameResult(
			D3D12Util::CompileShader(drawnormalhlsl, alphaTestDefines, "PS", "ps_5_1", mShaders["drawNormalsPS"])
		);
	}

	{
		std::wstring ssaohlsl = ShaderFileNamePrefix + L"Ssao.hlsl";
		CheckGameResult(D3D12Util::CompileShader(ssaohlsl, nullptr, "VS", "vs_5_1", mShaders["ssaoVS"]));
		CheckGameResult(D3D12Util::CompileShader(ssaohlsl, nullptr, "PS", "ps_5_1", mShaders["ssaoPS"]));
	}

	{
		std::wstring ssaoblurhlsl = ShaderFileNamePrefix + L"SsaoBlur.hlsl";
		CheckGameResult(D3D12Util::CompileShader(ssaoblurhlsl, nullptr, "VS", "vs_5_1", mShaders["ssaoBlurVS"]));
		CheckGameResult(D3D12Util::CompileShader(ssaoblurhlsl, nullptr, "PS", "ps_5_1", mShaders["ssaoBlurPS"]));
	}

	{
		std::wstring skyhlsl = ShaderFileNamePrefix + L"Sky.hlsl";
		CheckGameResult(D3D12Util::CompileShader(skyhlsl, nullptr, "VS", "vs_5_1", mShaders["skyVS"]));
		CheckGameResult(D3D12Util::CompileShader(skyhlsl, nullptr, "PS", "ps_5_1", mShaders["skyPS"]));
	}

	{
		std::wstring gbufferhlsl = ShaderFileNamePrefix + L"DrawGBuffer.hlsl";
		CheckGameResult(D3D12Util::CompileShader(gbufferhlsl, nullptr,			"VS", "vs_5_1", mShaders["gbufferVS"]));
		CheckGameResult(D3D12Util::CompileShader(gbufferhlsl, alphaTestDefines, "PS", "ps_5_1", mShaders["gbufferPS"]));	
	}

	{
		std::wstring gbufferhlsl = ShaderFileNamePrefix + L"DrawGBuffer.hlsl";
		CheckGameResult(
			D3D12Util::CompileShader(gbufferhlsl, skinnedDefines, "VS", "vs_5_1", mShaders["skinnedGBufferVS"])
		);
	}

	{
		std::wstring defaultghlsl = ShaderFileNamePrefix + L"DefaultUsingGBuffer.hlsl";
		CheckGameResult(D3D12Util::CompileShader(defaultghlsl, nullptr, "VS", "vs_5_1", mShaders["defaultGBufferVS"]));
		CheckGameResult(D3D12Util::CompileShader(defaultghlsl, nullptr, "PS", "ps_5_1", mShaders["defaultGBufferPS"]));
	}

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

	return GameResult(S_OK);
}

GameResult DxRenderer::BuildBasicGeometry() {
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 3);
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(256.0f, 256.0f, 40, 40);
	GeometryGenerator::MeshData sphere = geoGen.CreateSphere(0.5f, 20, 20);
	GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);
	GeometryGenerator::MeshData quad = geoGen.CreateQuad(0.0f, 0.0f, 1.0f, 1.0f, 0.0f);

	//
	// We are concatenating all the geometry into one big vertex/index buffer.  So
	// define the regions in the buffer each submesh covers.
	//

	// Cache the vertex offsets to each object in the concatenated vertex buffer.
	UINT boxVertexOffset = 0;
	UINT gridVertexOffset = static_cast<UINT>(box.Vertices.size());
	UINT sphereVertexOffset = gridVertexOffset + static_cast<UINT>(grid.Vertices.size());
	UINT cylinderVertexOffset = sphereVertexOffset + static_cast<UINT>(sphere.Vertices.size());
	UINT quadVertexOffset = cylinderVertexOffset + static_cast<UINT>(cylinder.Vertices.size());

	// Cache the starting index for each object in the concatenated index buffer.
	UINT boxIndexOffset = 0;
	UINT gridIndexOffset = static_cast<UINT>(box.Indices32.size());
	UINT sphereIndexOffset = gridIndexOffset + static_cast<UINT>(grid.Indices32.size());
	UINT cylinderIndexOffset = sphereIndexOffset + static_cast<UINT>(sphere.Indices32.size());
	UINT quadIndexOffset = cylinderIndexOffset + static_cast<UINT>(cylinder.Indices32.size());

	SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount = static_cast<UINT>(box.Indices32.size());
	boxSubmesh.StartIndexLocation = boxIndexOffset;
	boxSubmesh.BaseVertexLocation = boxVertexOffset;

	SubmeshGeometry gridSubmesh;
	gridSubmesh.IndexCount = static_cast<UINT>(grid.Indices32.size());
	gridSubmesh.StartIndexLocation = gridIndexOffset;
	gridSubmesh.BaseVertexLocation = gridVertexOffset;

	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.IndexCount = static_cast<UINT>(sphere.Indices32.size());
	sphereSubmesh.StartIndexLocation = sphereIndexOffset;
	sphereSubmesh.BaseVertexLocation = sphereVertexOffset;

	SubmeshGeometry cylinderSubmesh;
	cylinderSubmesh.IndexCount = static_cast<UINT>(cylinder.Indices32.size());
	cylinderSubmesh.StartIndexLocation = cylinderIndexOffset;
	cylinderSubmesh.BaseVertexLocation = cylinderVertexOffset;

	SubmeshGeometry quadSubmesh;
	quadSubmesh.IndexCount = static_cast<UINT>(quad.Indices32.size());
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

	GVector<Vertex> vertices(totalVertexCount);

	XMFLOAT3 vMinf3(+MathHelper::Infinity, +MathHelper::Infinity, +MathHelper::Infinity);
	XMFLOAT3 vMaxf3(-MathHelper::Infinity, -MathHelper::Infinity, -MathHelper::Infinity);

	XMVECTOR vMin = XMLoadFloat3(&vMinf3);
	XMVECTOR vMax = XMLoadFloat3(&vMaxf3);

	BoundingBox bound;

	UINT k = 0;
	for (size_t i = 0; i < box.Vertices.size(); ++i, ++k) {
		vertices[k].mPos = box.Vertices[i].Position;
		vertices[k].mNormal = box.Vertices[i].Normal;
		vertices[k].mTexC = box.Vertices[i].TexC;
		vertices[k].mTangentU = box.Vertices[i].TangentU;

		XMVECTOR P = XMLoadFloat3(&vertices[k].mPos);

		vMin = XMVectorMin(vMin, P);
		vMax = XMVectorMax(vMax, P);
	}
	XMStoreFloat3(&bound.Center, 0.5f * (vMax + vMin));
	XMStoreFloat3(&bound.Extents, 0.5f * (vMax - vMin));
	boxSubmesh.AABB = bound;

	vMin = XMLoadFloat3(&vMinf3);
	vMax = XMLoadFloat3(&vMaxf3);
	for (size_t i = 0; i < grid.Vertices.size(); ++i, ++k) {
		vertices[k].mPos = grid.Vertices[i].Position;
		vertices[k].mNormal = grid.Vertices[i].Normal;
		vertices[k].mTexC = grid.Vertices[i].TexC;
		vertices[k].mTangentU = grid.Vertices[i].TangentU;

		XMVECTOR P = XMLoadFloat3(&vertices[k].mPos);

		vMin = XMVectorMin(vMin, P);
		vMax = XMVectorMax(vMax, P);
	}
	XMStoreFloat3(&bound.Center, 0.5f * (vMax + vMin));
	XMStoreFloat3(&bound.Extents, 0.5f * (vMax - vMin));
	gridSubmesh.AABB = bound;

	vMin = XMLoadFloat3(&vMinf3);
	vMax = XMLoadFloat3(&vMaxf3);
	for (size_t i = 0; i < sphere.Vertices.size(); ++i, ++k) {
		vertices[k].mPos = sphere.Vertices[i].Position;
		vertices[k].mNormal = sphere.Vertices[i].Normal;
		vertices[k].mTexC = sphere.Vertices[i].TexC;
		vertices[k].mTangentU = sphere.Vertices[i].TangentU;

		XMVECTOR P = XMLoadFloat3(&vertices[k].mPos);

		vMin = XMVectorMin(vMin, P);
		vMax = XMVectorMax(vMax, P);
	}
	XMStoreFloat3(&bound.Center, 0.5f * (vMax + vMin));
	XMStoreFloat3(&bound.Extents, 0.5f * (vMax - vMin));
	sphereSubmesh.AABB = bound;

	vMin = XMLoadFloat3(&vMinf3);
	vMax = XMLoadFloat3(&vMaxf3);
	for (size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k) {
		vertices[k].mPos = cylinder.Vertices[i].Position;
		vertices[k].mNormal = cylinder.Vertices[i].Normal;
		vertices[k].mTexC = cylinder.Vertices[i].TexC;
		vertices[k].mTangentU = cylinder.Vertices[i].TangentU;

		XMVECTOR P = XMLoadFloat3(&vertices[k].mPos);

		vMin = XMVectorMin(vMin, P);
		vMax = XMVectorMax(vMax, P);
	}
	XMStoreFloat3(&bound.Center, 0.5f * (vMax + vMin));
	XMStoreFloat3(&bound.Extents, 0.5f * (vMax - vMin));
	cylinderSubmesh.AABB = bound;

	vMin = XMLoadFloat3(&vMinf3);
	vMax = XMLoadFloat3(&vMaxf3);
	for (int i = 0; i < quad.Vertices.size(); ++i, ++k) {
		vertices[k].mPos = quad.Vertices[i].Position;
		vertices[k].mNormal = quad.Vertices[i].Normal;
		vertices[k].mTexC = quad.Vertices[i].TexC;
		vertices[k].mTangentU = quad.Vertices[i].TangentU;

		XMVECTOR P = XMLoadFloat3(&vertices[k].mPos);

		vMin = XMVectorMin(vMin, P);
		vMax = XMVectorMax(vMax, P);
	}
	XMStoreFloat3(&bound.Center, 0.5f * (vMax + vMin));
	XMStoreFloat3(&bound.Extents, 0.5f * (vMax - vMin));
	quadSubmesh.AABB = bound;

	GVector<std::uint16_t> indices;
	indices.insert(indices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16()));
	indices.insert(indices.end(), std::begin(grid.GetIndices16()), std::end(grid.GetIndices16()));
	indices.insert(indices.end(), std::begin(sphere.GetIndices16()), std::end(sphere.GetIndices16()));
	indices.insert(indices.end(), std::begin(cylinder.GetIndices16()), std::end(cylinder.GetIndices16()));
	indices.insert(indices.end(), std::begin(quad.GetIndices16()), std::end(quad.GetIndices16()));

	const UINT vbByteSize = static_cast<UINT>(vertices.size() * sizeof(Vertex));
	const UINT ibByteSize = static_cast<UINT>(indices.size() * sizeof(std::uint16_t));

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "standardGeo";

	ReturnIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ReturnIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);
	
	ID3D12GraphicsCommandList* cmdList =
#ifdef MT_World
		mCommandLists[0].Get();
#else
		mCommandList.Get();
#endif

	CheckGameResult(D3D12Util::CreateDefaultBuffer(
		md3dDevice.Get(),
		cmdList,
		vertices.data(),
		vbByteSize,
		geo->VertexBufferUploader,
		geo->VertexBufferGPU)
	);

	CheckGameResult(D3D12Util::CreateDefaultBuffer(
		md3dDevice.Get(),
		cmdList,
		indices.data(), 
		ibByteSize, 
		geo->IndexBufferUploader, 
		geo->IndexBufferGPU)
	);
	
	geo->VertexByteStride = static_cast<UINT>(sizeof(Vertex));
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	geo->DrawArgs["box"] = boxSubmesh;
	geo->DrawArgs["grid"] = gridSubmesh;
	geo->DrawArgs["sphere"] = sphereSubmesh;
	geo->DrawArgs["cylinder"] = cylinderSubmesh;
	geo->DrawArgs["quad"] = quadSubmesh;

	mGeometries[geo->Name] = std::move(geo);

	return GameResult(S_OK);
}

GameResult DxRenderer::BuildBasicMaterials() {
	auto defaultMat = std::make_unique<Material>();
	defaultMat->Name = "default";
	defaultMat->MatCBIndex = mNumMatCB++;
	defaultMat->DiffuseSrvHeapIndex = 0;
	defaultMat->NormalSrvHeapIndex = 1;
	defaultMat->SpecularSrvHeapIndex = -1;
	defaultMat->DispSrvHeapIndex = -1;
	defaultMat->DiffuseAlbedo = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	defaultMat->FresnelR0 = XMFLOAT3(0.5f, 0.5f, 0.5f);
	defaultMat->Roughness = 0.5f;

	auto skyMat = std::make_unique<Material>();
	skyMat->Name = "sky";
	skyMat->MatCBIndex = mNumMatCB++;
	skyMat->DiffuseSrvHeapIndex = mDescHeapIdx.mSkyTexHeapIndex;
	defaultMat->NormalSrvHeapIndex = 1;
	defaultMat->SpecularSrvHeapIndex = -1;
	defaultMat->DispSrvHeapIndex = -1;
	skyMat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	skyMat->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	skyMat->Roughness = 1.0f;
	
	mMaterialRefs[defaultMat->Name] = defaultMat.get();
	mMaterials.push_back(std::move(defaultMat));

	mMaterialRefs[skyMat->Name] = skyMat.get();
	mMaterials.push_back(std::move(skyMat));

	return GameResult(S_OK);
}

GameResult DxRenderer::BuildBasicRenderItems() {
	XMFLOAT4X4 world;
	XMFLOAT4X4 tex;

	auto skyRitem = std::make_unique<RenderItem>();
	skyRitem->mObjCBIndex = mNumObjCB++;
	skyRitem->mMat = mMaterialRefs["sky"];
	skyRitem->mGeo = mGeometries["standardGeo"].get();
	skyRitem->mPrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	skyRitem->mIndexCount = skyRitem->mGeo->DrawArgs["sphere"].IndexCount;
	skyRitem->mStartIndexLocation = skyRitem->mGeo->DrawArgs["sphere"].StartIndexLocation;
	skyRitem->mBaseVertexLocation = skyRitem->mGeo->DrawArgs["sphere"].BaseVertexLocation;
	skyRitem->mBoundType = BoundType::AABB;
	skyRitem->mBoundingUnion.mAABB = skyRitem->mGeo->DrawArgs["sphere"].AABB;
	XMStoreFloat4x4(&world, XMMatrixScaling(5000.0f, 5000.0f, 5000.0f));
	skyRitem->mInstances.emplace_back(
		world,
		MathHelper::Identity4x4(),
		0.0f,
		static_cast<UINT>(skyRitem->mMat->MatCBIndex)
	);
	mRitemLayer[RenderLayer::Sky].push_back(skyRitem.get());
	mAllRitems.push_back(std::move(skyRitem));

	auto boxRitem = std::make_unique<RenderItem>();
	boxRitem->mObjCBIndex = mNumObjCB++;
	boxRitem->mMat = mMaterialRefs["default"];
	boxRitem->mGeo = mGeometries["standardGeo"].get();
	boxRitem->mPrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->mIndexCount = boxRitem->mGeo->DrawArgs["box"].IndexCount;
	boxRitem->mStartIndexLocation = boxRitem->mGeo->DrawArgs["box"].StartIndexLocation;
	boxRitem->mBaseVertexLocation = boxRitem->mGeo->DrawArgs["box"].BaseVertexLocation;
	boxRitem->mBoundType = BoundType::AABB;
	boxRitem->mBoundingUnion.mAABB = boxRitem->mGeo->DrawArgs["box"].AABB;
	XMStoreFloat4x4(&world, XMMatrixScaling(1.0f, 1.0f, 1.0f) * XMMatrixTranslation(0.0f, 0.5f, 0.0f));
	boxRitem->mInstances.emplace_back(
		world,
		MathHelper::Identity4x4(),
		0.0f,
		static_cast<UINT>(boxRitem->mMat->MatCBIndex)
	);
	mRitemLayer[RenderLayer::Opaque].push_back(boxRitem.get());
	mAllRitems.push_back(std::move(boxRitem));

	auto quadRitem = std::make_unique<RenderItem>();
	quadRitem->mObjCBIndex = mNumObjCB++;
	quadRitem->mMat = mMaterialRefs["default"];
	quadRitem->mGeo = mGeometries["standardGeo"].get();
	quadRitem->mPrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	quadRitem->mIndexCount = quadRitem->mGeo->DrawArgs["quad"].IndexCount;
	quadRitem->mStartIndexLocation = quadRitem->mGeo->DrawArgs["quad"].StartIndexLocation;
	quadRitem->mBaseVertexLocation = quadRitem->mGeo->DrawArgs["quad"].BaseVertexLocation;
	quadRitem->mBoundType = BoundType::AABB;
	quadRitem->mBoundingUnion.mAABB = quadRitem->mGeo->DrawArgs["quad"].AABB;
	for (size_t i = 0; i < 5; ++i) {
		quadRitem->mInstances.emplace_back(
			MathHelper::Identity4x4(),
			MathHelper::Identity4x4(),
			0.0f,
			static_cast<UINT>(quadRitem->mMat->MatCBIndex),
			-1,
			EInstanceRenderState::EID_DrawAlways
		);
	}
	mRitemLayer[RenderLayer::Debug].push_back(quadRitem.get());
	mAllRitems.push_back(std::move(quadRitem));

	quadRitem = std::make_unique<RenderItem>();
	quadRitem->mObjCBIndex = mNumObjCB++;
	quadRitem->mMat = mMaterialRefs["default"];
	quadRitem->mGeo = mGeometries["standardGeo"].get();
	quadRitem->mPrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	quadRitem->mIndexCount = quadRitem->mGeo->DrawArgs["quad"].IndexCount;
	quadRitem->mStartIndexLocation = quadRitem->mGeo->DrawArgs["quad"].StartIndexLocation;
	quadRitem->mBaseVertexLocation = quadRitem->mGeo->DrawArgs["quad"].BaseVertexLocation;
	quadRitem->mBoundType = BoundType::AABB;
	quadRitem->mBoundingUnion.mAABB = quadRitem->mGeo->DrawArgs["quad"].AABB;
	quadRitem->mInstances.emplace_back(
		MathHelper::Identity4x4(),
		MathHelper::Identity4x4(),
		0.0f,
		static_cast<UINT>(quadRitem->mMat->MatCBIndex),
		-1,
		EInstanceRenderState::EID_DrawAlways
	);
	mRitemLayer[RenderLayer::Screen].push_back(quadRitem.get());
	mAllRitems.push_back(std::move(quadRitem));

	auto gridRitem = std::make_unique<RenderItem>();
	gridRitem->mObjCBIndex = mNumObjCB++;
	gridRitem->mMat = mMaterialRefs["default"];
	gridRitem->mGeo = mGeometries["standardGeo"].get();
	gridRitem->mPrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	gridRitem->mIndexCount = gridRitem->mGeo->DrawArgs["grid"].IndexCount;
	gridRitem->mStartIndexLocation = gridRitem->mGeo->DrawArgs["grid"].StartIndexLocation;
	gridRitem->mBaseVertexLocation = gridRitem->mGeo->DrawArgs["grid"].BaseVertexLocation;
	gridRitem->mBoundType = BoundType::AABB;
	gridRitem->mBoundingUnion.mAABB = gridRitem->mGeo->DrawArgs["grid"].AABB;
	XMStoreFloat4x4(&tex, XMMatrixScaling(8.0f, 8.0f, 1.0f));
	gridRitem->mInstances.emplace_back(
		MathHelper::Identity4x4(),
		tex,
		0.0f,
		static_cast<UINT>(gridRitem->mMat->MatCBIndex)
	);
	mRitemLayer[RenderLayer::Opaque].push_back(gridRitem.get());
	mAllRitems.push_back(std::move(gridRitem));

	return GameResult(S_OK);
}

GameResult DxRenderer::BuildPSOs() {
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
	opaquePsoDesc.DepthStencilState.DepthEnable = true;
	opaquePsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	opaquePsoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
	opaquePsoDesc.SampleDesc.Count = 1;
	opaquePsoDesc.SampleDesc.Quality = 0;
	opaquePsoDesc.DSVFormat = mDepthStencilFormat;
	ReturnIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));

	//
	// PSO for skinned pass.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC skinnedOpaquePsoDesc = opaquePsoDesc;
	skinnedOpaquePsoDesc.InputLayout = { mSkinnedInputLayout.data(), static_cast<UINT>(mSkinnedInputLayout.size()) };
	skinnedOpaquePsoDesc.VS = {
		reinterpret_cast<BYTE*>(mShaders["skinnedVS"]->GetBufferPointer()),
		mShaders["skinnedVS"]->GetBufferSize()
	};
	ReturnIfFailed(md3dDevice->CreateGraphicsPipelineState(&skinnedOpaquePsoDesc, IID_PPV_ARGS(&mPSOs["skinnedOpaque"])));

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
	skeletonPsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	skeletonPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	ReturnIfFailed(md3dDevice->CreateGraphicsPipelineState(&skeletonPsoDesc, IID_PPV_ARGS(&mPSOs["skeleton"])));

	//
	// PSO for shadow map pass.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC smapPsoDesc = opaquePsoDesc;
	smapPsoDesc.RasterizerState.DepthBias = 100000;
	smapPsoDesc.RasterizerState.DepthBiasClamp = 0.001f;
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
	smapPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	ReturnIfFailed(md3dDevice->CreateGraphicsPipelineState(&smapPsoDesc, IID_PPV_ARGS(&mPSOs["shadow_opaque"])));

	//
	// PSO for shadow map pass for skinned render items.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC skinnedSmapPsoDesc = smapPsoDesc;
	skinnedSmapPsoDesc.InputLayout = { mSkinnedInputLayout.data(), static_cast<UINT>(mSkinnedInputLayout.size()) };
	skinnedSmapPsoDesc.VS = {
		reinterpret_cast<BYTE*>(mShaders["skinnedShadowVS"]->GetBufferPointer()),
		mShaders["skinnedShadowVS"]->GetBufferSize()
	};
	ReturnIfFailed(md3dDevice->CreateGraphicsPipelineState(&skinnedSmapPsoDesc, IID_PPV_ARGS(&mPSOs["skinnedShadow_opaque"])));

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
	debugPsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	ReturnIfFailed(md3dDevice->CreateGraphicsPipelineState(&debugPsoDesc, IID_PPV_ARGS(&mPSOs["debug"])));

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
	drawNormalsPsoDesc.RTVFormats[0] = mGBuffer.GetNormalMapFormat();
	drawNormalsPsoDesc.SampleDesc.Count = 1;
	drawNormalsPsoDesc.SampleDesc.Quality = 0;
	drawNormalsPsoDesc.DSVFormat = mDepthStencilFormat;
	drawNormalsPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	ReturnIfFailed(md3dDevice->CreateGraphicsPipelineState(&drawNormalsPsoDesc, IID_PPV_ARGS(&mPSOs["drawNormals"])));

	//
	// PSO for drawing normals for skinned render items.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC skinnedDrawNormalsPsoDesc = drawNormalsPsoDesc;
	skinnedDrawNormalsPsoDesc.InputLayout = { mSkinnedInputLayout.data(), static_cast<UINT>(mSkinnedInputLayout.size()) };
	skinnedDrawNormalsPsoDesc.VS = {
		reinterpret_cast<BYTE*>(mShaders["skinnedDrawNormalsVS"]->GetBufferPointer()),
		mShaders["skinnedDrawNormalsVS"]->GetBufferSize()
	};
	ReturnIfFailed(md3dDevice->CreateGraphicsPipelineState(&skinnedDrawNormalsPsoDesc, IID_PPV_ARGS(&mPSOs["skinnedDrawNormals"])));

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
	ReturnIfFailed(md3dDevice->CreateGraphicsPipelineState(&ssaoPsoDesc, IID_PPV_ARGS(&mPSOs["ssao"])));

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
	ReturnIfFailed(md3dDevice->CreateGraphicsPipelineState(&ssaoBlurPsoDesc, IID_PPV_ARGS(&mPSOs["ssaoBlur"])));

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
	ReturnIfFailed(md3dDevice->CreateGraphicsPipelineState(&skyPsoDesc, IID_PPV_ARGS(&mPSOs["sky"])));

	//
	// PSO for drwaing gbuffer.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gbufferPsoDesc = opaquePsoDesc;
	gbufferPsoDesc.VS = {
		reinterpret_cast<BYTE*>(mShaders["gbufferVS"]->GetBufferPointer()),
		mShaders["gbufferVS"]->GetBufferSize()
	};
	gbufferPsoDesc.PS = {
		reinterpret_cast<BYTE*>(mShaders["gbufferPS"]->GetBufferPointer()),
		mShaders["gbufferPS"]->GetBufferSize()
	};
	gbufferPsoDesc.NumRenderTargets = GBuffer::NumRenderTargets;
	gbufferPsoDesc.RTVFormats[0] = mGBuffer.GetDiffuseMapFormat();
	gbufferPsoDesc.RTVFormats[1] = mGBuffer.GetNormalMapFormat();
	gbufferPsoDesc.RTVFormats[2] = mGBuffer.GetSpecularMapFormat();
	gbufferPsoDesc.SampleDesc.Count = 1;
	gbufferPsoDesc.SampleDesc.Quality = 0;
	gbufferPsoDesc.DSVFormat = mDepthStencilFormat;
	gbufferPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	ReturnIfFailed(md3dDevice->CreateGraphicsPipelineState(&gbufferPsoDesc, IID_PPV_ARGS(&mPSOs["drawGBuffer"])));

	//
	// PSO for drawing gbuffer with alpha blending.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gbufferAlphaBledingPsoDesc = gbufferPsoDesc;
	D3D12_RENDER_TARGET_BLEND_DESC gbufferBlendDesc;
	gbufferBlendDesc.BlendEnable = true;
	gbufferBlendDesc.LogicOpEnable = false;
	gbufferBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
	gbufferBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	gbufferBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	gbufferBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	gbufferBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	gbufferBlendDesc.SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
	gbufferBlendDesc.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
	gbufferBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	gbufferAlphaBledingPsoDesc.BlendState.RenderTarget[0] = gbufferBlendDesc;
	gbufferAlphaBledingPsoDesc.BlendState.RenderTarget[1] = gbufferBlendDesc;
	gbufferAlphaBledingPsoDesc.BlendState.RenderTarget[2] = gbufferBlendDesc;
	ReturnIfFailed(
		md3dDevice->CreateGraphicsPipelineState(
			&gbufferAlphaBledingPsoDesc, IID_PPV_ARGS(&mPSOs["drawGBufferAlphaBlending"])
		)
	);

	//
	// PSO for gbuffer for skinned render items.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC skinnedGBufferPsoDesc = gbufferPsoDesc;
	skinnedGBufferPsoDesc.InputLayout = { mSkinnedInputLayout.data(), static_cast<UINT>(mSkinnedInputLayout.size()) };
	skinnedGBufferPsoDesc.VS = {
		reinterpret_cast<BYTE*>(mShaders["skinnedGBufferVS"]->GetBufferPointer()),
		mShaders["skinnedGBufferVS"]->GetBufferSize()
	};
	ReturnIfFailed(
		md3dDevice->CreateGraphicsPipelineState(&skinnedGBufferPsoDesc, IID_PPV_ARGS(&mPSOs["drawSkinnedGBuffer"]))
	);

	//
	// PSO for drawing screen using gbuffer.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gbufferScreenPsoDesc = opaquePsoDesc;
	gbufferScreenPsoDesc.pRootSignature = mRootSignature.Get();
	gbufferScreenPsoDesc.VS = {
		reinterpret_cast<BYTE*>(mShaders["defaultGBufferVS"]->GetBufferPointer()),
		mShaders["defaultGBufferVS"]->GetBufferSize()
	};
	gbufferScreenPsoDesc.PS = {
		reinterpret_cast<BYTE*>(mShaders["defaultGBufferPS"]->GetBufferPointer()),
		mShaders["defaultGBufferPS"]->GetBufferSize()
	};
	gbufferScreenPsoDesc.DepthStencilState.DepthEnable = false;
	ReturnIfFailed(md3dDevice->CreateGraphicsPipelineState(&gbufferScreenPsoDesc, IID_PPV_ARGS(&mPSOs["gbufferScreen"])));

	return GameResult(S_OK);
}

GameResult DxRenderer::BuildFrameResources() {
	for (UINT i = 0; i < gNumFrameResources; ++i) {
		mFrameResources.push_back(std::make_unique<FrameResource>(
			md3dDevice.Get(), 2, 256, MaxInstanceCount, 256));

		CheckGameResult(mFrameResources.back()->Initialize(mNumThreads));
	}

	return GameResult(S_OK);
}

void DxRenderer::DrawRenderItems(ID3D12GraphicsCommandList* outCmdList, const GVector<RenderItem*>& inRitems) {
	UINT objCBByteSize = D3D12Util::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	auto objectCB = mCurrFrameResource->mObjectCB.Resource();

	// For each render item...
	for (size_t i = 0, end = inRitems.size(); i < end; ++i) {
		auto ri = inRitems[i];

		outCmdList->IASetVertexBuffers(0, 1, &ri->mGeo->VertexBufferView());
		outCmdList->IASetIndexBuffer(&ri->mGeo->IndexBufferView());
		outCmdList->IASetPrimitiveTopology(ri->mPrimitiveType);

		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->mObjCBIndex * objCBByteSize;
		outCmdList->SetGraphicsRootConstantBufferView(mRootParams.mObjectCBIndex, objCBAddress);

		outCmdList->DrawIndexedInstanced(ri->mIndexCount, ri->mNumInstancesToDraw,
			ri->mStartIndexLocation, ri->mBaseVertexLocation, 0);
	}
}

GameResult DxRenderer::DrawSceneToShadowMap() {
	ID3D12CommandAllocator* cmdListAlloc;
	ID3D12GraphicsCommandList* cmdList;
#ifdef MT_World
	cmdListAlloc = mCurrFrameResource->mCmdListAllocs[0].Get();
	cmdList = mCommandLists[0].Get();
#else
	cmdListAlloc = mCurrFrameResource->mCmdListAlloc.Get();
	cmdList = mCommandList.Get();
#endif

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	ReturnIfFailed(cmdList->Reset(cmdListAlloc, mPSOs["shadow_opaque"].Get()));

	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvSrvUavDescriptorHeap.Get() };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	cmdList->SetGraphicsRootSignature(mRootSignature.Get());

	// Bind all the materials used in this scene. For structured buffers,
	// we can bypass the heap and set as a root descriptor.
	auto matBuffer = mCurrFrameResource->mMaterialBuffer.Resource();
	cmdList->SetGraphicsRootShaderResourceView(mRootParams.mMatBufferIndex, matBuffer->GetGPUVirtualAddress());

	auto instIdxBuffer = mCurrFrameResource->mInstanceIdxBuffer.Resource();
	cmdList->SetGraphicsRootShaderResourceView(mRootParams.mInstIdxBufferIndex, instIdxBuffer->GetGPUVirtualAddress());

	auto instDataBuffer = mCurrFrameResource->mInstanceDataBuffer.Resource();
	cmdList->SetGraphicsRootShaderResourceView(mRootParams.mInstBufferIndex, instDataBuffer->GetGPUVirtualAddress());

	// Bind null SRV for shadow map pass.
	cmdList->SetGraphicsRootDescriptorTable(mRootParams.mMiscTextureMapIndex, mNullSrv);

	// Bind all the textures used in this scene.
	// Observe that we only have to specify the first descriptor in the table.  
	// The root signature knows how many descriptors are expected in the table.
	cmdList->SetGraphicsRootDescriptorTable(
		mRootParams.mTextureMapIndex,
		mCbvSrvUavDescriptorHeap->GetGPUDescriptorHandleForHeapStart()
	);

	// Bind the texture for extracing animations data.
	cmdList->SetGraphicsRootDescriptorTable(
		mRootParams.mAnimationsMapIndex,
		mAnimsMap.AnimationsMapSrv()
	);

	cmdList->SetGraphicsRoot32BitConstants(
		mRootParams.mConstSettingsIndex,
		1,
		&MaxInstanceCount,
		0
	);

	cmdList->SetGraphicsRoot32BitConstants(
		mRootParams.mConstSettingsIndex,
		static_cast<UINT>(mConstantSettings.size()),
		mConstantSettings.data(),
		1
	);

	cmdList->RSSetViewports(1, &mShadowMap.Viewport());
	cmdList->RSSetScissorRects(1, &mShadowMap.ScissorRect());

	// Change to DEPTH_WRITE.
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mShadowMap.Resource(),
		D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE));

	// Clear the back buffer and depth buffer.
	cmdList->ClearDepthStencilView(mShadowMap.Dsv(),
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Specify the buffers we are going to render to.
	cmdList->OMSetRenderTargets(0, nullptr, false, &mShadowMap.Dsv());

	// Bind the pass constant buffer for the shadow map pass.
	UINT passCBByteSize = D3D12Util::CalcConstantBufferByteSize(sizeof(PassConstants));
	auto passCB = mCurrFrameResource->mPassCB.Resource();
	D3D12_GPU_VIRTUAL_ADDRESS passCBAddress = passCB->GetGPUVirtualAddress() + 1 * passCBByteSize;
	cmdList->SetGraphicsRootConstantBufferView(mRootParams.mPassCBIndex, passCBAddress);

	DrawRenderItems(cmdList, mRitemLayer[RenderLayer::Opaque]);

	cmdList->SetPipelineState(mPSOs["skinnedShadow_opaque"].Get());
	DrawRenderItems(cmdList, mRitemLayer[RenderLayer::SkinnedOpaque]);

	// Change back to GENERIC_READ so we can read the texture in a shader.
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mShadowMap.Resource(),
		D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ));

	// Done recording commands.
	ReturnIfFailed(cmdList->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { cmdList };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	return GameResult(S_OK);
}

GameResult DxRenderer::DrawSceneToGBuffer() {
	ID3D12CommandAllocator* cmdListAlloc;
	ID3D12GraphicsCommandList* cmdList;
#ifdef MT_World
	cmdListAlloc = mCurrFrameResource->mCmdListAllocs[0].Get();
	cmdList = mCommandLists[0].Get();
#else
	cmdListAlloc = mCurrFrameResource->mCmdListAlloc.Get();
	cmdList = mCommandList.Get();
#endif

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	ReturnIfFailed(cmdList->Reset(cmdListAlloc, mPSOs["drawGBuffer"].Get()));

	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvSrvUavDescriptorHeap.Get() };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	cmdList->SetGraphicsRootSignature(mRootSignature.Get());

	// Bind all the materials used in this scene. For structured buffers,
	// we can bypass the heap and set as a root descriptor.
	auto matBuffer = mCurrFrameResource->mMaterialBuffer.Resource();
	cmdList->SetGraphicsRootShaderResourceView(mRootParams.mMatBufferIndex, matBuffer->GetGPUVirtualAddress());

	auto instIdxBuffer = mCurrFrameResource->mInstanceIdxBuffer.Resource();
	cmdList->SetGraphicsRootShaderResourceView(mRootParams.mInstIdxBufferIndex, instIdxBuffer->GetGPUVirtualAddress());

	auto instDataBuffer = mCurrFrameResource->mInstanceDataBuffer.Resource();
	cmdList->SetGraphicsRootShaderResourceView(mRootParams.mInstBufferIndex, instDataBuffer->GetGPUVirtualAddress());

	// Bind null SRV for gbuffer.
	cmdList->SetGraphicsRootDescriptorTable(mRootParams.mMiscTextureMapIndex, mNullSrv);

	// Bind all the textures used in this scene.
	// Observe that we only have to specify the first descriptor in the table.  
	// The root signature knows how many descriptors are expected in the table.
	cmdList->SetGraphicsRootDescriptorTable(
		mRootParams.mTextureMapIndex,
		mCbvSrvUavDescriptorHeap->GetGPUDescriptorHandleForHeapStart()
	);

	// Bind the texture for extracing animations data.
	cmdList->SetGraphicsRootDescriptorTable(
		mRootParams.mAnimationsMapIndex,
		mAnimsMap.AnimationsMapSrv()
	);

	cmdList->SetGraphicsRoot32BitConstants(
		mRootParams.mConstSettingsIndex,
		1,
		&MaxInstanceCount,
		0
	);

	cmdList->SetGraphicsRoot32BitConstants(
		mRootParams.mConstSettingsIndex,
		static_cast<UINT>(mConstantSettings.size()),
		mConstantSettings.data(),
		1
	);

	cmdList->RSSetViewports(1, &mScreenViewport);
	cmdList->RSSetScissorRects(1, &mScissorRect);

	auto diffuseMap		= mGBuffer.GetDiffuseMap();
	auto diffuseMapRtv	= mGBuffer.GetDiffuseMapRtv();
	auto normalMap		= mGBuffer.GetNormalMap();
	auto normalMapRtv	= mGBuffer.GetNormalMapRtv();
	auto specMap		= mGBuffer.GetSpecularMap();
	auto specMapRtv		= mGBuffer.GetSpecularMapRtv();

	// Change to RENDER_TARGET.
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		diffuseMap, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET));
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		normalMap, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET));
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		specMap, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// Clear the screen normal map and depth buffer.
	{
		float clearValue[] = { 0.0f, 0.0f, 0.0f, 1.0f };
		cmdList->ClearRenderTargetView(diffuseMapRtv, clearValue, 0, nullptr);

		clearValue[1] = 0.0f;
		clearValue[2] = 1.0f;
		clearValue[3] = 0.0f;
		cmdList->ClearRenderTargetView(normalMapRtv, clearValue, 0, nullptr);

		clearValue[2] = 0.0f;
		cmdList->ClearRenderTargetView(specMapRtv, clearValue, 0, nullptr);
	}
	cmdList->ClearDepthStencilView(
		DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Specify the buffers we are going to render to.
	cmdList->OMSetRenderTargets(GBuffer::NumRenderTargets, &diffuseMapRtv, true, &DepthStencilView());

	// Bind the constant buffer for this pass.
	auto passCB = mCurrFrameResource->mPassCB.Resource();
	cmdList->SetGraphicsRootConstantBufferView(mRootParams.mPassCBIndex, passCB->GetGPUVirtualAddress());

	DrawRenderItems(cmdList, mRitemLayer[RenderLayer::Opaque]);

	cmdList->SetPipelineState(mPSOs["drawSkinnedGBuffer"].Get());
	DrawRenderItems(cmdList, mRitemLayer[RenderLayer::SkinnedOpaque]);

	// Change back to GENERIC_READ so we can read the texture in a shader.
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		diffuseMap, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		normalMap, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		specMap, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));

	// Done recording commands.
	ReturnIfFailed(cmdList->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { cmdList };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	return GameResult(S_OK);
}

GameResult DxRenderer::DrawSceneToRenderTarget() {
	ID3D12CommandAllocator* cmdListAlloc;
	ID3D12GraphicsCommandList* cmdList;
#ifdef MT_World
	cmdListAlloc = mCurrFrameResource->mCmdListAllocs[0].Get();
	cmdList = mCommandLists[0].Get();
#else
	cmdListAlloc = mCurrFrameResource->mCmdListAlloc.Get();
	cmdList = mCommandList.Get();
#endif

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	ReturnIfFailed(cmdList->Reset(cmdListAlloc, nullptr));

	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvSrvUavDescriptorHeap.Get() };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	//
	// Compute SSAO.
	//
	cmdList->SetGraphicsRootSignature(mSsaoRootSignature.Get());
	mSsao.ComputeSsao(cmdList, mCurrFrameResource, 3);

	//
	// Main rendering pass.
	//
	// Rebind state whenever graphics root signature changes.
	cmdList->SetGraphicsRootSignature(mRootSignature.Get());

	// Bind all the materials used in this scene. For structured buffers,
	// we can bypass the heap and set as a root descriptor.
	auto matBuffer = mCurrFrameResource->mMaterialBuffer.Resource();
	cmdList->SetGraphicsRootShaderResourceView(mRootParams.mMatBufferIndex, matBuffer->GetGPUVirtualAddress());

	auto instIdxBuffer = mCurrFrameResource->mInstanceIdxBuffer.Resource();
	cmdList->SetGraphicsRootShaderResourceView(mRootParams.mInstIdxBufferIndex, instIdxBuffer->GetGPUVirtualAddress());

	auto instDataBuffer = mCurrFrameResource->mInstanceDataBuffer.Resource();
	cmdList->SetGraphicsRootShaderResourceView(mRootParams.mInstBufferIndex, instDataBuffer->GetGPUVirtualAddress());

	// Bind the sky cube map.  For our demos, we just use one "world" cube map representing the environment
	// from far away, so all objects will use the same cube map and we only need to set it once per-frame.  
	// If we wanted to use "local" cube maps, we would have to change them per-object, or dynamically
	// index into an array of cube maps.
	CD3DX12_GPU_DESCRIPTOR_HANDLE miscTexDescriptor(mCbvSrvUavDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	miscTexDescriptor.Offset(mDescHeapIdx.mSkyTexHeapIndex, mCbvSrvUavDescriptorSize);
	cmdList->SetGraphicsRootDescriptorTable(mRootParams.mMiscTextureMapIndex, miscTexDescriptor);

	// Bind all the textures used in this scene.
	// Observe that we only have to specify the first descriptor in the table.  
	// The root signature knows how many descriptors are expected in the table.
	cmdList->SetGraphicsRootDescriptorTable(
		mRootParams.mTextureMapIndex,
		mCbvSrvUavDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

	// Bind the texture for extracing animations data.
	cmdList->SetGraphicsRootDescriptorTable(
		mRootParams.mAnimationsMapIndex,
		mAnimsMap.AnimationsMapSrv()
	);

	cmdList->SetGraphicsRoot32BitConstants(
		mRootParams.mConstSettingsIndex,
		1,
		&MaxInstanceCount,
		0
	);

	cmdList->SetGraphicsRoot32BitConstants(
		mRootParams.mConstSettingsIndex,
		static_cast<UINT>(mConstantSettings.size()),
		mConstantSettings.data(),
		1
	);

	cmdList->RSSetViewports(1, &mScreenViewport);
	cmdList->RSSetScissorRects(1, &mScissorRect);

	// Indicate a state transition on the resource usage.
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// Clear the back buffer and depth buffer.
	cmdList->ClearRenderTargetView(CurrentBackBufferView(), Colors::Black, 0, nullptr);

	// Specify the buffers we are going to render to.
	cmdList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	auto passCB = mCurrFrameResource->mPassCB.Resource();
	cmdList->SetGraphicsRootConstantBufferView(mRootParams.mPassCBIndex, passCB->GetGPUVirtualAddress());

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mDepthStencilBuffer.Get(),
		D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ));

	cmdList->SetPipelineState(mPSOs["gbufferScreen"].Get());
	DrawRenderItems(cmdList, mRitemLayer[RenderLayer::Screen]);

	cmdList->SetPipelineState(mPSOs["sky"].Get());
	DrawRenderItems(cmdList, mRitemLayer[RenderLayer::Sky]);

	cmdList->SetPipelineState(mPSOs["skeleton"].Get());
	DrawRenderItems(cmdList, mRitemLayer[RenderLayer::Skeleton]);

	cmdList->SetPipelineState(mPSOs["debug"].Get());
	DrawRenderItems(cmdList, mRitemLayer[RenderLayer::Debug]);

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mDepthStencilBuffer.Get(),
		D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE));

	//
	// Draw texts.
	//
	DrawTexts();

	// Indicate a state transition on the resource usage.
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PRESENT));

	// Done recording commands.
	ReturnIfFailed(cmdList->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { cmdList };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	mGraphicsMemory->Commit(mCommandQueue.Get());

	// Swap the back and front buffers
	ReturnIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	// Advance the fence value to mark commands up to this fence point.
	mCurrFrameResource->mFence = ++mCurrentFence;

	// Add an instruction to the command queue to set a new fence point. 
	// Because we are on the GPU timeline, the new fence point won't be 
	// set until the GPU finishes processing all the commands prior to this Signal().
	mCommandQueue->Signal(mFence.Get(), mCurrentFence);

	return GameResult(S_OK);
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DxRenderer::GetCpuSrv(int inIndex) const {
	auto srv = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvSrvUavDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	srv.Offset(inIndex, mCbvSrvUavDescriptorSize);
	return srv;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE DxRenderer::GetGpuSrv(int inIndex) const {
	auto srv = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvSrvUavDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	srv.Offset(inIndex, mCbvSrvUavDescriptorSize);
	return srv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DxRenderer::GetDsv(int inIndex) const {
	auto dsv = CD3DX12_CPU_DESCRIPTOR_HANDLE(mDsvHeap->GetCPUDescriptorHandleForHeapStart());
	dsv.Offset(inIndex, mDsvDescriptorSize);
	return dsv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DxRenderer::GetRtv(int inIndex) const {
	auto rtv = CD3DX12_CPU_DESCRIPTOR_HANDLE(mRtvHeap->GetCPUDescriptorHandleForHeapStart());
	rtv.Offset(inIndex, mRtvDescriptorSize);
	return rtv;
}