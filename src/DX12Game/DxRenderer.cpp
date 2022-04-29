#include "DX12Game/DxRenderer.h"

#include "DX12Game/GameWorld.h"
#include "DX12Game/GameCamera.h"
#include "DX12Game/Mesh.h"
#include "DX12Game/BlurHelper.h"
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

	mRootConstants[0] = 0.0f;
	mRootConstants[1] = 128.0f;

	mEffectEnabled = EffectEnabled::ESsao | EffectEnabled::ESsr | EffectEnabled::EBloom;
}

DxRenderer::~DxRenderer() {
	if (!bIsCleaned)
		CleanUp();
}

GameResult DxRenderer::Initialize(HWND hMainWnd, UINT inClientWidth, UINT inClientHeight, UINT inNumThreads) {
	CheckGameResult(DxLowRenderer::Initialize(hMainWnd, inClientWidth, inClientHeight, inNumThreads));

#ifdef MT_World
	mNumInstances.resize(mNumThreads);
	mEachUpdateFunctions.resize(mNumThreads);

	if (mNumThreads > 1) {
		UINT i = 1;

		UpdateFunc postPassCB = &DxRenderer::UpdatePostPassCB;
		mEachUpdateFunctions[i++].push_back(postPassCB);
		if (i >= mNumThreads) i = 0;

		UpdateFunc ssaoCB = &DxRenderer::UpdateSsaoCB;
		mEachUpdateFunctions[i++].push_back(ssaoCB);
		if (i >= mNumThreads) i = 0;

		UpdateFunc ssrCB = &DxRenderer::UpdateSsrCB;
		mEachUpdateFunctions[i++].push_back(ssrCB);
		if (i >= mNumThreads) i = 0;

		UpdateFunc shadowPassCB = &DxRenderer::UpdateShadowPassCB;
		mEachUpdateFunctions[i++].push_back(shadowPassCB);
		if (i >= mNumThreads) i = 0;

		UpdateFunc bloomCB = &DxRenderer::UpdateBloomCB;
		mEachUpdateFunctions[i++].push_back(bloomCB);
		if (i >= mNumThreads) i = 0;
	}
	else {

	}
#endif

	// Reset the command list to prep for initialization commands.
	for (UINT i = 0; i < mNumThreads; ++i)
		ReturnIfFailed(mCommandLists[i]->Reset(mCommandAllocators[i].Get(), nullptr));

	CheckGameResult(mPsoManager.Initialize(md3dDevice.Get()));
	CheckGameResult(mShaderManager.Initialize(L".\\..\\..\\..\\..\\Assets\\Shaders\\"));

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
	CheckGameResult(mAnimsMap.Initialize(md3dDevice.Get()));
	CheckGameResult(mSsr.Initialize(md3dDevice.Get(), mClientWidth / 4, mClientHeight / 4, DXGI_FORMAT_R8G8B8A8_UNORM, 32, 16, 1, 0.3f));
	CheckGameResult(mMainPass.Initialize(md3dDevice.Get(), mClientWidth, mClientHeight, mBackBufferFormat));
	CheckGameResult(mBloom.Initialize(md3dDevice.Get(), mClientWidth / 16, mClientHeight / 16, mBackBufferFormat));

	ID3D12GraphicsCommandList* cmdList = mCommandLists[0].Get();

	CheckGameResult(mSsao.Initialize(md3dDevice.Get(), cmdList, mClientWidth / 2, mClientHeight / 2, DXGI_FORMAT_R16_UNORM, DXGI_FORMAT_R16G16B16A16_FLOAT));

	CheckGameResult(mRSManager.Initialize(md3dDevice.Get()));

	CheckGameResult(LoadBasicTextures());
	CheckGameResult(mRSManager.BuildRootSignatures());
	CheckGameResult(BuildDescriptorHeaps());
	CheckGameResult(mShaderManager.CompileShaders());
	CheckGameResult(BuildBasicGeometry());
	CheckGameResult(BuildBasicMaterials());
	CheckGameResult(BuildBasicRenderItems());
	CheckGameResult(BuildFrameResources());
	CheckGameResult(BuildPSOs());

	mSsao.SetPSOs(mPsoManager.GetSsaoPsoPtr(), mPsoManager.GetSsaoBlurPsoPtr());
	mSsr.SetPSOs(mPsoManager.GetSsrPsoPtr(), mPsoManager.GetSsrBlurPsoPtr());
	mBloom.SetPSOs(mPsoManager.GetBloomPsoPtr(), mPsoManager.GetBloomBlurPsoPtr());

	// Execute the initialization commands.
	ExecuteCommandLists();
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

	//
	// Set blur weights.
	//
	{
		auto blurWeights = BlurHelper::CalcGaussWeights(2.5f);
		if (blurWeights == nullptr) return GameResultFalse;

		mBlurWeights5.push_back(XMFLOAT4(&blurWeights[0]));
		mBlurWeights5.push_back(XMFLOAT4(&blurWeights[4]));
		mBlurWeights5.push_back(XMFLOAT4(&blurWeights[8]));

		delete[] blurWeights;
	}

	//
	// Set blur weigths for ssr.
	//
	{
		auto blurWeights = BlurHelper::CalcGaussWeights(4.5f);
		if (blurWeights == nullptr) return GameResultFalse;

		mBlurWeights9.push_back(XMFLOAT4(&blurWeights[0]));
		mBlurWeights9.push_back(XMFLOAT4(&blurWeights[4]));
		mBlurWeights9.push_back(XMFLOAT4(&blurWeights[8]));
		mBlurWeights9.push_back(XMFLOAT4(&blurWeights[12]));
		mBlurWeights9.push_back(XMFLOAT4(&blurWeights[16]));

		delete[] blurWeights;
	}

	{
		auto blurWeights = BlurHelper::CalcGaussWeights(8.5f);
		if (blurWeights == nullptr) return GameResultFalse;

		mBlurWeights17.push_back(XMFLOAT4(&blurWeights[0]));
		mBlurWeights17.push_back(XMFLOAT4(&blurWeights[4]));
		mBlurWeights17.push_back(XMFLOAT4(&blurWeights[8]));
		mBlurWeights17.push_back(XMFLOAT4(&blurWeights[12]));
		mBlurWeights17.push_back(XMFLOAT4(&blurWeights[16]));
		mBlurWeights17.push_back(XMFLOAT4(&blurWeights[20]));
		mBlurWeights17.push_back(XMFLOAT4(&blurWeights[24]));
		mBlurWeights17.push_back(XMFLOAT4(&blurWeights[28]));
		mBlurWeights17.push_back(XMFLOAT4(&blurWeights[32]));

		delete[] blurWeights;
	}
	

	// Succeeded initialization.
	bIsValid = true;

	return GameResultOk;
}

GameResult DxRenderer::Initialize(HWND hMainWnd, UINT inClientWidth, UINT inClientHeight,
			CVBarrier* inCV, SpinlockBarrier* inSpinlock, UINT inNumThreads) {
	CheckGameResult(Initialize(hMainWnd, inClientWidth, inClientHeight, inNumThreads));

	mCVBarrier = inCV;
	mSpinlockBarrier = inSpinlock;

	return GameResultOk;
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

	SyncHost(mCVBarrier);

	CheckGameResult(AnimateMaterials(gt, inTid));

	CheckGameResult(UpdateObjectCBsAndInstanceBuffers(gt, inTid));	

	CheckGameResult(UpdateMaterialBuffers(gt, inTid));

	if (inTid == 0) {
		CheckGameResult(UpdateShadowTransform(gt));
		CheckGameResult(UpdateMainPassCB(gt));
	}

	SyncHost(mCVBarrier);

	for (auto& func : mEachUpdateFunctions[inTid])
		CheckGameResult(func(*this, std::ref(gt), inTid));
	
	if (inTid == 0) {
		std::vector<std::string> textsToRemove;
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

	return GameResultOk;
}

GameResult DxRenderer::Draw(const GameTimer& gt, UINT inTid) {
	if (inTid == 0) {
		CheckGameResult(ResetFrameResourceCmdListAlloc());
		CheckGameResult(ClearViews());
	}

	SyncHost(mCVBarrier);

	CheckGameResult(DrawSceneToShadowMap(inTid));

	SyncHost(mCVBarrier);

	CheckGameResult(DrawSceneToGBuffer(inTid));

	SyncHost(mCVBarrier);

	if (inTid == 0)
		CheckGameResult(DrawSceneToRenderTarget());

	return GameResultOk;
}

GameResult DxRenderer::OnResize(UINT inClientWidth, UINT inClientHeight) {
	if (!mMainCamera)
		ReturnGameResult(S_FALSE, L"Main camera does not exist")

	CheckGameResult(DxLowRenderer::OnResize(inClientWidth, inClientHeight));

	mMainCamera->SetLens(XM_PI * 0.3f, AspectRatio(), 0.1f, 1000.0f);

	auto depthStencilBuffer = mDepthStencilBuffer.Get();
	mGBuffer.OnResize(mClientWidth, mClientHeight, depthStencilBuffer);
	mGBuffer.RebuildDescriptors(depthStencilBuffer);

	mSsao.OnResize(mClientWidth, mClientHeight);
	// Resources changed, so need to rebuild descriptors.
	mSsao.RebuildDescriptors(mGBuffer.GetNormalMapSrv());

	mSsr.OnResize(mClientWidth, mClientHeight);
	mSsr.RebuildDescriptors();

	mMainPass.OnResize(mClientWidth, mClientHeight);
	mMainPass.RebuildDescriptors();

	mBloom.OnResize(mClientWidth, mClientHeight);
	mBloom.RebuildDescriptors();

	BoundingFrustum::CreateFromMatrix(mCamFrustum, mMainCamera->GetProj());

	return GameResultOk;
}

void DxRenderer::UpdateWorldTransform(const std::string& inRenderItemName, const DirectX::XMMATRIX& inTransform, bool inIsSkeletal) {
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

void DxRenderer::UpdateInstanceAnimationData(
			const std::string&	inRenderItemName,
			UINT				inAnimClipIdx, 
			float				inTimePos, 
			bool				inIsSkeletal) {
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
	ReturnIfFailed(mCommandLists[0]->Reset(mCommandAllocators[0].Get(), nullptr));

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
	ReturnIfFailed(mCommandLists[0]->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandLists[0].Get() };

	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	CheckGameResult(FlushCommandQueue());

	if (!inMesh->GetMaterials().empty())
		AddMaterials(inMesh->GetMaterials());

	return GameResultOk;
}

void DxRenderer::DrawTexts() {
	mSpriteBatch->Begin(mCommandLists[0].Get());

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

GameResult DxRenderer::AddMaterials(const std::unordered_map<std::string, MaterialIn>& inMaterials) {
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
		material->AlphaSrvHeapIndex = mAlphaSrvHeapIndices[materialIn.AlphaMapFileName];

		material->DiffuseAlbedo = materialIn.DiffuseAlbedo;
		material->FresnelR0 = materialIn.FresnelR0;
		material->Roughness = materialIn.Roughness;

		mMaterialRefs[materialIn.MaterialName] = material.get();
		mMaterials.push_back(std::move(material));
	}

	return GameResultOk;
}

UINT DxRenderer::AddAnimations(const std::string& inClipName, const Animation& inAnim) {
	std::vector<XMFLOAT4> curves;

	for (size_t frame = 0; frame < inAnim.mNumFrames; ++frame) {
		for (const auto& curve : inAnim.mCurves) {
			for (size_t row = 0; row < 4; ++row) {
				curves.emplace_back(
					curve[frame].m[row][0],
					curve[frame].m[row][1],
					curve[frame].m[row][2],
					curve[frame].m[row][3]
				);
			}
		}
	}

	return mAnimsMap.AddAnimation(inClipName, curves.data(), inAnim.mNumFrames, inAnim.mCurves.size());
}

GameResult DxRenderer::UpdateAnimationsMap() {
	ID3D12GraphicsCommandList* cmdList = mCommandLists[0].Get();
	ID3D12CommandAllocator* alloc = mCommandAllocators[0].Get();

	// Reset the command list to prep for initialization commands.
	ReturnIfFailed(cmdList->Reset(alloc, nullptr));

	mAnimsMap.UpdateAnimationsMap(cmdList);

	// Execute the initialization commands.
	ReturnIfFailed(cmdList->Close());
	ID3D12CommandList* cmdsLists[] = { cmdList };

	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	CheckGameResult(FlushCommandQueue());

	return GameResultOk;
}

bool DxRenderer::GetSsaoEnabled() const {
	return (mEffectEnabled & EffectEnabled::ESsao) == EffectEnabled::ESsao;
}

void DxRenderer::SetSsaoEnabled(bool bState) {
	if (bState)
		mEffectEnabled |= EffectEnabled::ESsao;
	else
		mEffectEnabled &= ~EffectEnabled::ESsao;
}

bool DxRenderer::GetSsrEnabled() const {
	return (mEffectEnabled & EffectEnabled::ESsr) == EffectEnabled::ESsr;
}

void DxRenderer::SetSsrEnabled(bool bState) {
	if (bState)
		mEffectEnabled |= EffectEnabled::ESsr;
	else
		mEffectEnabled &= ~EffectEnabled::ESsr;
}

bool DxRenderer::GetBloomEnabled() const {
	return (mEffectEnabled & EffectEnabled::EBloom) == EffectEnabled::EBloom;
}

void DxRenderer::SetBloomEnabled(bool bState) {
	if (bState)
		mEffectEnabled |= EffectEnabled::EBloom;
	else
		mEffectEnabled &= ~EffectEnabled::EBloom;
}

bool DxRenderer::GetDrawDebugSkeletonsEnabled() const {
	return bDrawDebugSkeletonsEnabled;
}

void DxRenderer::SetDrawDebugSkeletonsEnabled(bool bState) {
	bDrawDebugSkeletonsEnabled = bState;
}

bool DxRenderer::GetDrawDebugWindowsEnabled() const {
	return bDrawDeubgWindowsEnabled;
}

void DxRenderer::SetDrawDebugWindowsEnabled(bool bState) {
	bDrawDeubgWindowsEnabled = bState;
}

GameResult DxRenderer::CreateRtvAndDsvDescriptorHeaps() {
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
#ifdef DeferredRendering
	// Add + 1 for diffuse map, +1 for screen normal map, +1 for specular map, +2 for ambient maps.
	rtvHeapDesc.NumDescriptors = SwapChainBufferCount + 
		GBuffer::NumRenderTargets + MainPass::NumRenderTargets + Ssao::NumRenderTargets + Ssr::NumRenderTargets + Bloom::NumRenderTargets;
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

	return GameResultOk;
}

GameResult DxRenderer::ResetFrameResourceCmdListAlloc() {
	for (UINT i = 0; i < mNumThreads; ++i) {
		auto cmdListAlloc = mCurrFrameResource->mCmdListAllocs[i];

		// Reuse the memory associated with command recording.
		// We can only reset when the associated command lists have finished execution on the GPU.
		ReturnIfFailed(cmdListAlloc->Reset());
	}

	return GameResultOk;
}

GameResult DxRenderer::ClearViews() {
	ID3D12CommandAllocator* cmdListAlloc = mCurrFrameResource->mCmdListAllocs[0].Get();
	ID3D12GraphicsCommandList* cmdList = mCommandLists[0].Get();

	ReturnIfFailed(cmdList->Reset(cmdListAlloc, nullptr));

	// Clear the back buffer and depth buffer.
	cmdList->ClearDepthStencilView(mShadowMap.Dsv(),
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	cmdList->ClearDepthStencilView(
		DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	{
		// Clear the screen normal map and depth buffer.
		auto diffuseMapRtv = mGBuffer.GetDiffuseMapRtv();
		auto normalMapRtv = mGBuffer.GetNormalMapRtv();
		auto specMapRtv = mGBuffer.GetSpecularMapRtv();

		float clearValue[] = { 0.0f, 0.0f, 0.0f, 1.0f };
		cmdList->ClearRenderTargetView(diffuseMapRtv, clearValue, 0, nullptr);

		clearValue[1] = 0.0f;
		clearValue[2] = 1.0f;
		clearValue[3] = 0.0f;
		cmdList->ClearRenderTargetView(normalMapRtv, clearValue, 0, nullptr);

		clearValue[2] = 0.0f;
		cmdList->ClearRenderTargetView(specMapRtv, clearValue, 0, nullptr);
	}

	// Done recording commands.
	ReturnIfFailed(cmdList->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { cmdList };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	return GameResultOk;
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

	ID3D12GraphicsCommandList* cmdList = mCommandLists[0].Get();

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

	return GameResultOk;
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

	ID3D12GraphicsCommandList* cmdList = mCommandLists[0].Get();

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

	return GameResultOk;
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
		
	ID3D12GraphicsCommandList* cmdList = mCommandLists[0].Get();

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

	return GameResultOk;
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

	return GameResultOk;
}

GameResult DxRenderer::AddTextures(const std::unordered_map<std::string, MaterialIn>& inMaterials) {
	CheckGameResult(FlushCommandQueue());

	ID3D12GraphicsCommandList* cmdList = mCommandLists[0].Get();
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

	mAlphaSrvHeapIndices[""] = -1;
	for (const auto& matList : inMaterials) {
		const auto& material = matList.second;

		if (material.AlphaMapFileName.empty())
			continue;

		auto iter = mTextures.find(material.AlphaMapFileName);
		if (iter != mTextures.end())
			continue;

		auto texMap = std::make_unique<Texture>();
		texMap->Name = material.AlphaMapFileName;

		std::wstringstream wsstream;
		wsstream << TextureFileNamePrefix << material.AlphaMapFileName.c_str();
		texMap->Filename = wsstream.str();

		HRESULT status = CreateDDSTextureFromFile12(
			md3dDevice.Get(),
			cmdList,
			texMap->Filename.c_str(),
			texMap->Resource,
			texMap->UploadHeap
		);

		if (FAILED(status)) {
			mAlphaSrvHeapIndices[material.AlphaMapFileName] = -1;
		}
		else {
			mAlphaSrvHeapIndices[material.AlphaMapFileName] = mDescHeapIdx.mCurrSrvHeapIndex++;
			mTextures[texMap->Name] = std::move(texMap);
		}
	}

	// Execute the initialization commands.
	ReturnIfFailed(cmdList->Close());
	ID3D12CommandList* cmdsLists[] = { cmdList };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	CheckGameResult(FlushCommandQueue());

	return GameResultOk;
}

GameResult DxRenderer::AddDescriptors(const std::unordered_map<std::string, MaterialIn>& inMaterials) {
	CheckGameResult(FlushCommandQueue());

	ID3D12GraphicsCommandList* cmdList = mCommandLists[0].Get();

	// Reset the command list to prep for initialization commands.
	ReturnIfFailed(cmdList->Reset(mCommandAllocators[0].Get(), nullptr));

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

		auto nestedIter = std::find(
			mBuiltDiffuseTexDescriptors.cbegin(), mBuiltDiffuseTexDescriptors.cend(), material.DiffuseMapFileName);
		if (nestedIter != mBuiltDiffuseTexDescriptors.cend())
			continue;

		auto texIter = mTextures.find(material.DiffuseMapFileName);
		if (texIter == mTextures.cend())
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

		auto nestedIter = std::find(
			mBuiltNormalTexDescriptors.cbegin(), mBuiltNormalTexDescriptors.cend(),	material.NormalMapFileName);
		if (nestedIter != mBuiltNormalTexDescriptors.cend())
			continue;

		auto texIter = mTextures.find(material.NormalMapFileName);
		if (texIter == mTextures.cend())
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

		auto nestedIter = std::find(
			mBuiltSpecularTexDescriptors.cbegin(), mBuiltSpecularTexDescriptors.cend(),	material.SpecularMapFileName);
		if (nestedIter != mBuiltSpecularTexDescriptors.cend())
			continue;

		auto texIter = mTextures.find(material.SpecularMapFileName);
		if (texIter == mTextures.cend())
			continue;

		auto tex = mTextures[material.SpecularMapFileName]->Resource;

		srvDesc.Format = tex->GetDesc().Format;
		srvDesc.Texture2D.MipLevels = tex->GetDesc().MipLevels;

		md3dDevice->CreateShaderResourceView(tex.Get(), &srvDesc, hDescriptor);

		hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);
		++mNumDescriptor;

		mBuiltSpecularTexDescriptors.push_back(material.SpecularMapFileName);
	}

	for (const auto& matList : inMaterials) {
		const auto& material = matList.second;

		if (material.AlphaMapFileName.empty())
			continue;

		auto nestedIter = std::find(
			mBuiltAlphaTexDescriptors.cbegin(), mBuiltAlphaTexDescriptors.cend(), material.AlphaMapFileName);
		if (nestedIter != mBuiltAlphaTexDescriptors.cend())
			continue;

		auto texIter = mTextures.find(material.AlphaMapFileName);
		if (texIter == mTextures.cend())
			continue;

		auto tex = mTextures[material.AlphaMapFileName]->Resource;

		srvDesc.Format = tex->GetDesc().Format;
		srvDesc.Texture2D.MipLevels = tex->GetDesc().MipLevels;

		md3dDevice->CreateShaderResourceView(tex.Get(), &srvDesc, hDescriptor);

		hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);
		++mNumDescriptor;

		mBuiltAlphaTexDescriptors.push_back(material.AlphaMapFileName);
	}

	// Execute the initialization commands.
	ReturnIfFailed(cmdList->Close());
	ID3D12CommandList* cmdsLists[] = { cmdList };

	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	CheckGameResult(FlushCommandQueue());

	return GameResultOk;
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
	return GameResultOk;
}

GameResult DxRenderer::UpdateObjectCBsAndInstanceBuffers(const GameTimer& gt, UINT inTid) {
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
	
	SyncHost(mSpinlockBarrier);

	if (inTid == 0) {
		UINT visibleObjectCount = 0;

		for (UINT i = 0; i < mNumThreads; ++i)
			visibleObjectCount += mNumInstances[i];

		AddOutputText(
			"TEXT_VOC",
			L"voc: " + std::to_wstring(visibleObjectCount),
			10.0f,
			10.0f,
			16.0f
		);
	}

	return GameResultOk;
}

GameResult DxRenderer::UpdateMaterialBuffers(const GameTimer& gt, UINT inTid) {
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
			matData.mAlphaMapIndex = mat->AlphaSrvHeapIndex;

			currMaterialBuffer.CopyData(mat->MatCBIndex, matData);

			// Next FrameResource need to be updated too.
			mat->NumFramesDirty--;
		}
	}

	return GameResultOk;
}

GameResult DxRenderer::UpdateShadowTransform(const GameTimer& gt, UINT inTid) {
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

	return GameResultOk;
}

GameResult DxRenderer::UpdateMainPassCB(const GameTimer& gt, UINT inTid) {
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

	return GameResultOk;
}

GameResult DxRenderer::UpdateShadowPassCB(const GameTimer& gt, UINT inTid) {
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

	return GameResultOk;
}

GameResult DxRenderer::UpdatePostPassCB(const GameTimer& gt, UINT inTid) {
	PostPassConstants postPassCB;

	postPassCB.mInvView = mMainPassCB.mInvView;
	postPassCB.mProj = mMainPassCB.mProj;
	postPassCB.mInvProj = mMainPassCB.mInvProj;
	postPassCB.mEyePosW = mMainPassCB.mEyePosW;
	postPassCB.mCubeMapCenter = 0.0f;
	postPassCB.mCubeMapExtents = 128.0f;

	auto& currPostPassCB = mCurrFrameResource->mPostPassCB;
	currPostPassCB.CopyData(0, postPassCB);

	return GameResultOk;
}

GameResult DxRenderer::UpdateSsaoCB(const GameTimer& gt, UINT inTid) {
	if (!mMainCamera)
		ReturnGameResult(S_FALSE, L"Main camera does not exist");

	SsaoConstants ssaoCB;

	XMMATRIX P = mMainCamera->GetProj();

	// Transform NDC space [-1,+1]^2 to texture space [0,1]^2
	XMMATRIX T(
		0.5f,  0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f,  0.0f, 1.0f, 0.0f,
		0.5f,  0.5f, 0.0f, 1.0f
	);

	ssaoCB.mView = mMainPassCB.mView;
	ssaoCB.mProj = mMainPassCB.mProj;
	ssaoCB.mInvProj = mMainPassCB.mInvProj;
	XMStoreFloat4x4(&ssaoCB.mProjTex, XMMatrixTranspose(P * T));

	mSsao.GetOffsetVectors(ssaoCB.mOffsetVectors);

	ssaoCB.mBlurWeights[0] = mBlurWeights5[0];
	ssaoCB.mBlurWeights[1] = mBlurWeights5[1];
	ssaoCB.mBlurWeights[2] = mBlurWeights5[2];

	ssaoCB.mInvRenderTargetSize = XMFLOAT2(1.0f / mSsao.GetSsaoMapWidth(), 1.0f / mSsao.GetSsaoMapHeight());

	//ssaoCB.mBlurRadius = 5;

	// Coordinates given in view space.
	ssaoCB.mOcclusionRadius = 0.5f;
	ssaoCB.mOcclusionFadeStart = 0.2f;
	ssaoCB.mOcclusionFadeEnd = 2.0f;
	ssaoCB.mSurfaceEpsilon = 0.05f;

	auto& currSsaoCB = mCurrFrameResource->mSsaoCB;
	currSsaoCB.CopyData(0, ssaoCB);

	return GameResultOk;
}

GameResult DxRenderer::UpdateSsrCB(const GameTimer& gt, UINT inTid) {
	SsrConstants ssrCB;

	ssrCB.mProj = mMainPassCB.mProj;
	ssrCB.mInvProj = mMainPassCB.mInvProj;
	ssrCB.mViewProj = mMainPassCB.mViewProj;
	
	ssrCB.mBlurWeights[0] = mBlurWeights9[0];
	ssrCB.mBlurWeights[1] = mBlurWeights9[1];
	ssrCB.mBlurWeights[2] = mBlurWeights9[2];
	ssrCB.mBlurWeights[3] = mBlurWeights9[3];
	ssrCB.mBlurWeights[4] = mBlurWeights9[4];

	ssrCB.mInvRenderTargetSize = XMFLOAT2(1.0f / mSsr.GetSsrMapWidth(), 1.0f / mSsr.GetSsrMapHeight());

	ssrCB.mSsrDistance = mSsr.GetSsrDistance();
	ssrCB.mMaxFadeDistance = mSsr.GetMaxFadeDistance();
	ssrCB.mMinFadeDistance = mSsr.GetMinFadeDistance();
	ssrCB.mEdgeFadeLength = mSsr.GetEdgeFadeLength();

	ssrCB.mBlurRadius = 9;

	auto& currSsrCB = mCurrFrameResource->mSsrCB;
	currSsrCB.CopyData(0, ssrCB);

	return GameResultOk;
}

GameResult DxRenderer::UpdateBloomCB(const GameTimer& gt, UINT inTid) {
	BloomConstants bloomCB;

	bloomCB.mBlurWeights[0] = mBlurWeights17[0];
	bloomCB.mBlurWeights[1] = mBlurWeights17[1];
	bloomCB.mBlurWeights[2] = mBlurWeights17[2];
	bloomCB.mBlurWeights[3] = mBlurWeights17[3];
	bloomCB.mBlurWeights[4] = mBlurWeights17[4];
	bloomCB.mBlurWeights[5] = mBlurWeights17[5];
	bloomCB.mBlurWeights[6] = mBlurWeights17[6];
	bloomCB.mBlurWeights[7] = mBlurWeights17[7];
	bloomCB.mBlurWeights[8] = mBlurWeights17[8];

	bloomCB.mInvRenderTargetSize = XMFLOAT2(1.0f / mBloom.GetBloomMapWidth(), 1.0f / mBloom.GetBloomMapHeight());

	bloomCB.mBlurRadius = 17;

	auto& currBloomCB = mCurrFrameResource->mBloomCB;
	currBloomCB.CopyData(0, bloomCB);

	return GameResultOk;
}

/// Update functions

GameResult DxRenderer::LoadBasicTextures() {
	std::vector<std::string> texNames = {
		"defaultDiffuseMap",
		"defaultNormalMap",
		"skyCubeMap",
		"blurSkyCubeMap"
	};

	std::vector<std::wstring> texFileNames = {
		TextureFileNamePrefix + L"white1x1.dds",
		TextureFileNamePrefix + L"default_nmap.dds",
		TextureFileNamePrefix + L"skycube.dds",
		TextureFileNamePrefix + L"blurskycube.dds"
	};

	ID3D12GraphicsCommandList* cmdList = mCommandLists[0].Get();

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

	return GameResultOk;
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

void DxRenderer::BuildDescriptorHeapIndices(UINT inOffset) {
	mDescHeapIdx.mCubeMapIndex				= inOffset;;
	mDescHeapIdx.mBlurCubeMapIndex			= mDescHeapIdx.mCubeMapIndex			+ 1;
	mDescHeapIdx.mMainPassMapIndex1			= mDescHeapIdx.mBlurCubeMapIndex		+ 1;
	mDescHeapIdx.mMainPassMapIndex2			= mDescHeapIdx.mMainPassMapIndex1		+ 1;
	mDescHeapIdx.mDiffuseMapIndex			= mDescHeapIdx.mMainPassMapIndex2		+ 1;
	mDescHeapIdx.mNormalMapIndex			= mDescHeapIdx.mDiffuseMapIndex			+ 1;
	mDescHeapIdx.mDepthMapIndex				= mDescHeapIdx.mNormalMapIndex			+ 1;
	mDescHeapIdx.mSpecularMapIndex			= mDescHeapIdx.mDepthMapIndex			+ 1;
	mDescHeapIdx.mShadowMapIndex			= mDescHeapIdx.mSpecularMapIndex		+ 1;
	mDescHeapIdx.mSsaoAmbientMapIndex		= mDescHeapIdx.mShadowMapIndex			+ 1;
	mDescHeapIdx.mSsrMapIndex				= mDescHeapIdx.mSsaoAmbientMapIndex		+ 1;
	mDescHeapIdx.mBloomMapIndex				= mDescHeapIdx.mSsrMapIndex				+ 1;
	mDescHeapIdx.mBloomBlurMapIndex			= mDescHeapIdx.mBloomMapIndex			+ 1;
	mDescHeapIdx.mAnimationsMapIndex		= mDescHeapIdx.mBloomBlurMapIndex		+ 1;
	mDescHeapIdx.mSsaoAdditionalMapIndex	= mDescHeapIdx.mAnimationsMapIndex		+ 1;
	mDescHeapIdx.mSsrAdditionalMapIndex		= mDescHeapIdx.mSsaoAdditionalMapIndex	+ 2;	
	mDescHeapIdx.mBloomAdditionalMapIndex	= mDescHeapIdx.mSsrAdditionalMapIndex	+ 1;
	mDescHeapIdx.mDefaultFontIndex			= mDescHeapIdx.mBloomAdditionalMapIndex + 1;
	mDescHeapIdx.mCurrSrvHeapIndex			= mDescHeapIdx.mDefaultFontIndex		+ 1;

	mNumDescriptor = mDescHeapIdx.mCurrSrvHeapIndex;
}

void DxRenderer::BuildDescriptorsForEachHelperClass() {
	UINT rtvIndex = SwapChainBufferCount;
	mGBuffer.BuildDescriptors(
		mDepthStencilBuffer.Get(),
		GetCpuSrv(mDescHeapIdx.mDiffuseMapIndex),
		GetGpuSrv(mDescHeapIdx.mDiffuseMapIndex),
		GetRtv(rtvIndex),
		mCbvSrvUavDescriptorSize,
		mRtvDescriptorSize
	);

	mShadowMap.BuildDescriptors(
		GetCpuSrv(mDescHeapIdx.mShadowMapIndex),
		GetGpuSrv(mDescHeapIdx.mShadowMapIndex),
		GetDsv(1)
	);

	rtvIndex += GBuffer::NumRenderTargets;
	mMainPass.BuildDescriptors(
		GetCpuSrv(mDescHeapIdx.mMainPassMapIndex1),
		GetGpuSrv(mDescHeapIdx.mMainPassMapIndex1),
		GetRtv(rtvIndex),
		mCbvSrvUavDescriptorSize,
		mRtvDescriptorSize
	);

	rtvIndex += MainPass::NumRenderTargets;
	mSsao.BuildDescriptors(
		GetGpuSrv(mDescHeapIdx.mNormalMapIndex),
		GetCpuSrv(mDescHeapIdx.mSsaoAmbientMapIndex),
		GetGpuSrv(mDescHeapIdx.mSsaoAmbientMapIndex),
		GetCpuSrv(mDescHeapIdx.mSsaoAdditionalMapIndex),
		GetGpuSrv(mDescHeapIdx.mSsaoAdditionalMapIndex),
		GetRtv(rtvIndex),
		mCbvSrvUavDescriptorSize,
		mRtvDescriptorSize
	);

	mAnimsMap.BuildDescriptors(
		GetCpuSrv(mDescHeapIdx.mAnimationsMapIndex),
		GetGpuSrv(mDescHeapIdx.mAnimationsMapIndex)
	);

	rtvIndex += Ssao::NumRenderTargets;
	mSsr.BuildDescriptors(
		GetGpuSrv(mDescHeapIdx.mMainPassMapIndex1),
		GetGpuSrv(mDescHeapIdx.mNormalMapIndex),
		GetCpuSrv(mDescHeapIdx.mSsrMapIndex),
		GetGpuSrv(mDescHeapIdx.mSsrMapIndex),
		GetCpuSrv(mDescHeapIdx.mSsrAdditionalMapIndex),
		GetGpuSrv(mDescHeapIdx.mSsrAdditionalMapIndex),
		GetRtv(rtvIndex),
		mCbvSrvUavDescriptorSize,
		mRtvDescriptorSize
	);

	rtvIndex += Ssr::NumRenderTargets;
	mBloom.BuildDescriptors(
		GetCpuSrv(mDescHeapIdx.mBloomMapIndex),
		GetGpuSrv(mDescHeapIdx.mBloomMapIndex),
		GetCpuSrv(mDescHeapIdx.mBloomAdditionalMapIndex),
		GetGpuSrv(mDescHeapIdx.mBloomAdditionalMapIndex),
		GetGpuSrv(mDescHeapIdx.mMainPassMapIndex1),
		GetRtv(rtvIndex),
		mCbvSrvUavDescriptorSize,
		mRtvDescriptorSize
	);
}

GameResult DxRenderer::BuildDescriptorHeaps() {
	//
	// Create the SRV heap.
	//
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 128; // default: 14, gbuffer: ?
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ReturnIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mCbvSrvUavDescriptorHeap)));

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

	BuildDescriptorHeapIndices(static_cast<UINT>(tex2DList.size()));
	BuildDescriptorsForEachHelperClass();

	return GameResultOk;
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

	std::vector<Vertex> vertices(totalVertexCount);

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

	std::vector<std::uint16_t> indices;
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
	
	ID3D12GraphicsCommandList* cmdList = mCommandLists[0].Get();

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

	return GameResultOk;
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
	mMaterialRefs[defaultMat->Name] = defaultMat.get();
	mMaterials.push_back(std::move(defaultMat));

	auto brightMat = std::make_unique<Material>();
	brightMat->Name = "bright";
	brightMat->MatCBIndex = mNumMatCB++;
	brightMat->DiffuseSrvHeapIndex = 0;
	brightMat->NormalSrvHeapIndex = 1;
	brightMat->SpecularSrvHeapIndex = -1;
	brightMat->DispSrvHeapIndex = -1;
	brightMat->DiffuseAlbedo = XMFLOAT4(0.9f, 0.9f, 0.9f, 1.0f);
	brightMat->FresnelR0 = XMFLOAT3(0.8f, 0.8f, 0.8f);
	brightMat->Roughness = 0.1f;
	mMaterialRefs[brightMat->Name] = brightMat.get();
	mMaterials.push_back(std::move(brightMat));

	auto skyMat = std::make_unique<Material>();
	skyMat->Name = "sky";
	skyMat->MatCBIndex = mNumMatCB++;
	skyMat->DiffuseSrvHeapIndex = mDescHeapIdx.mCubeMapIndex;
	skyMat->NormalSrvHeapIndex = 1;
	skyMat->SpecularSrvHeapIndex = -1;
	skyMat->DispSrvHeapIndex = -1;
	skyMat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	skyMat->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	skyMat->Roughness = 1.0f;
	mMaterialRefs[skyMat->Name] = skyMat.get();
	mMaterials.push_back(std::move(skyMat));

	return GameResultOk;
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
	boxRitem->mMat = mMaterialRefs["bright"];
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

	return GameResultOk;
}

GameResult DxRenderer::BuildPSOs() {
	CheckGameResult(mPsoManager.BuildSkeletonPso(
		mRSManager.GetBasicRootSignature(),
		mShaderManager.GetSkeletonVShader(),
		mShaderManager.GetSkeletonGShader(),
		mShaderManager.GetSkeletonPShader(),
		mBackBufferFormat
	));
	CheckGameResult(mPsoManager.BuildShadowPso(
		mRSManager.GetBasicRootSignature(),
		mShaderManager.GetShadowsVShader(),
		mShaderManager.GetShadowsPShader(),
		mDepthStencilFormat
	));
	CheckGameResult(mPsoManager.BuildSkinnedShadowPso(
		mRSManager.GetBasicRootSignature(),
		mShaderManager.GetSkinnedShadowsVShader(),
		mShaderManager.GetShadowsPShader(),
		mDepthStencilFormat
	));
	CheckGameResult(mPsoManager.BuildDebugPso(
		mRSManager.GetBasicRootSignature(),
		mShaderManager.GetDebugVShader(),
		mShaderManager.GetDebugPShader(),
		mBackBufferFormat
	));
	CheckGameResult(mPsoManager.BuildSsaoPso(
		mRSManager.GetSsaoRootSignature(),
		mShaderManager.GetSsaoVShader(),
		mShaderManager.GetSsaoPShader(),
		mSsao.GetAmbientMapFormat()
	));
	CheckGameResult(mPsoManager.BuildSsaoBlurPso(
		mRSManager.GetSsaoRootSignature(),
		mShaderManager.GetSsaoBlurVShader(),
		mShaderManager.GetSsaoBlurPShader(),
		mSsao.GetAmbientMapFormat()
	));
	CheckGameResult(mPsoManager.BuildSsrPso(
		mRSManager.GetSsrRootSignature(),
		mShaderManager.GetSsrVShader(),
		mShaderManager.GetSsrPShader(),
		mSsr.GetAmbientMapFormat()
	));
	CheckGameResult(mPsoManager.BuildSsrBlurPso(
		mRSManager.GetSsrRootSignature(),
		mShaderManager.GetSsrBlurVShader(),
		mShaderManager.GetSsrBlurPShader(),
		mSsr.GetAmbientMapFormat()
	));
	CheckGameResult(mPsoManager.BuildSkyPso(
		mRSManager.GetBasicRootSignature(),
		mShaderManager.GetSkyVShader(),
		mShaderManager.GetSkyPShader(),
		mBackBufferFormat,
		mDepthStencilFormat
	));
	CheckGameResult(mPsoManager.BuildGBufferPso(
		mRSManager.GetBasicRootSignature(),
		mShaderManager.GetGBufferVShader(),
		mShaderManager.GetGBufferPShader(),
		mGBuffer.GetDiffuseMapFormat(),
		mGBuffer.GetNormalMapFormat(),
		mGBuffer.GetSpecularMapFormat(),
		mDepthStencilFormat
	));
	CheckGameResult(mPsoManager.BuildSkinnedGBufferPso(
		mRSManager.GetBasicRootSignature(),
		mShaderManager.GetSkinnedGBufferVShader(),
		mShaderManager.GetGBufferPShader(),
		mGBuffer.GetDiffuseMapFormat(),
		mGBuffer.GetNormalMapFormat(),
		mGBuffer.GetSpecularMapFormat(),
		mDepthStencilFormat
	));
	CheckGameResult(mPsoManager.BuildAlphaBlendingGBufferPso(
		mRSManager.GetBasicRootSignature(),
		mShaderManager.GetGBufferVShader(),
		mShaderManager.GetGBufferPShader(),
		mGBuffer.GetDiffuseMapFormat(),
		mGBuffer.GetNormalMapFormat(),
		mGBuffer.GetSpecularMapFormat(),
		mDepthStencilFormat
	));
	CheckGameResult(mPsoManager.BuildMainPassPso(
		mRSManager.GetBasicRootSignature(),
		mShaderManager.GetMainPassVShader(),
		mShaderManager.GetMainPassPShader(),
		mMainPass.GetMainMpassMapFormat(),
		mMainPass.GetMainMpassMapFormat()
	));
	CheckGameResult(mPsoManager.BuildPostPassPso(
		mRSManager.GetPostPassRootSignature(),
		mShaderManager.GetPostPassVShader(),
		mShaderManager.GetPostPassPShader(),
		mBackBufferFormat
	));
	CheckGameResult(mPsoManager.BuildBloomPso(
		mRSManager.GetBloomRootSignature(),
		mShaderManager.GetBloomVShader(),
		mShaderManager.GetBloomPShader(),
		mBackBufferFormat
	));
	CheckGameResult(mPsoManager.BuildBloomBlurPso(
		mRSManager.GetBloomRootSignature(),
		mShaderManager.GetBloomBlurVShader(),
		mShaderManager.GetBloomBlurPShader(),
		mBackBufferFormat
	));
	return GameResultOk;
}

GameResult DxRenderer::BuildFrameResources() {
	for (UINT i = 0; i < gNumFrameResources; ++i) {
		mFrameResources.push_back(std::make_unique<FrameResource>(
			md3dDevice.Get(), 2, 256, MaxInstanceCount, 256));

		CheckGameResult(mFrameResources.back()->Initialize(mNumThreads));
	}

	return GameResultOk;
}

void DxRenderer::DrawRenderItems(
		ID3D12GraphicsCommandList*	outCmdList, 
		RenderItem* const*			inRitems, 
		size_t						inNum) {
	UINT objCBByteSize = D3D12Util::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	auto objectCB = mCurrFrameResource->mObjectCB.Resource();

	// For each render item...
	for (size_t i = 0; i < inNum; ++i) {
		auto ri = inRitems[i];

		outCmdList->IASetVertexBuffers(0, 1, &ri->mGeo->VertexBufferView());
		outCmdList->IASetIndexBuffer(&ri->mGeo->IndexBufferView());
		outCmdList->IASetPrimitiveTopology(ri->mPrimitiveType);

		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->mObjCBIndex * objCBByteSize;
		outCmdList->SetGraphicsRootConstantBufferView(mRSManager.GetObjectCBIndex(), objCBAddress);

		outCmdList->DrawIndexedInstanced(ri->mIndexCount, ri->mNumInstancesToDraw,
			ri->mStartIndexLocation, ri->mBaseVertexLocation, 0);
	}
}

void DxRenderer::DrawRenderItems(
		ID3D12GraphicsCommandList*	outCmdList,
		RenderItem* const*			inRitems,
		size_t						inBegin,
		size_t						inEnd) {
	UINT objCBByteSize = D3D12Util::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	auto objectCB = mCurrFrameResource->mObjectCB.Resource();
	
	// For each render item...
	for (size_t i = inBegin; i < inEnd; ++i) {
		auto ri = inRitems[i];

		outCmdList->IASetVertexBuffers(0, 1, &ri->mGeo->VertexBufferView());
		outCmdList->IASetIndexBuffer(&ri->mGeo->IndexBufferView());
		outCmdList->IASetPrimitiveTopology(ri->mPrimitiveType);
		
		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->mObjCBIndex * objCBByteSize;
		outCmdList->SetGraphicsRootConstantBufferView(mRSManager.GetObjectCBIndex(), objCBAddress);
		
		outCmdList->DrawIndexedInstanced(ri->mIndexCount, ri->mNumInstancesToDraw,
			ri->mStartIndexLocation, ri->mBaseVertexLocation, 0);
	}
}

void DxRenderer::BindViews(ID3D12GraphicsCommandList* outCmdList, bool bShadowPass) {
	auto passCB = mCurrFrameResource->mPassCB.Resource();
	if (bShadowPass) {
		// Bind the pass constant buffer for the shadow map pass.
		UINT passCBByteSize = D3D12Util::CalcConstantBufferByteSize(sizeof(PassConstants));
		D3D12_GPU_VIRTUAL_ADDRESS passCBAddress = passCB->GetGPUVirtualAddress() + 1 * passCBByteSize;
		outCmdList->SetGraphicsRootConstantBufferView(mRSManager.GetPassCBIndex(), passCBAddress);
	}
	else {
		// Bind the constant buffer for this pass.
		outCmdList->SetGraphicsRootConstantBufferView(mRSManager.GetPassCBIndex(), passCB->GetGPUVirtualAddress());
	}

	// Bind all the materials used in this scene. For structured buffers,
	// we can bypass the heap and set as a root descriptor.
	auto matBuffer = mCurrFrameResource->mMaterialBuffer.Resource();
	outCmdList->SetGraphicsRootShaderResourceView(mRSManager.GetMatBufferIndex(), matBuffer->GetGPUVirtualAddress());

	auto instIdxBuffer = mCurrFrameResource->mInstanceIdxBuffer.Resource();
	outCmdList->SetGraphicsRootShaderResourceView(mRSManager.GetInstIdxBufferIndex(), instIdxBuffer->GetGPUVirtualAddress());

	auto instDataBuffer = mCurrFrameResource->mInstanceDataBuffer.Resource();
	outCmdList->SetGraphicsRootShaderResourceView(mRSManager.GetInstBufferIndex(), instDataBuffer->GetGPUVirtualAddress());
}

void DxRenderer::BindDescriptorTables(ID3D12GraphicsCommandList* outCmdList, bool bNullMiscTex) {
	if (!bNullMiscTex) {
		// Bind the sky cube map.  For our demos, we just use one "world" cube map representing the environment
		// from far away, so all objects will use the same cube map and we only need to set it once per-frame.  
		// If we wanted to use "local" cube maps, we would have to change them per-object, or dynamically
		// index into an array of cube maps.
		CD3DX12_GPU_DESCRIPTOR_HANDLE miscTexDescriptor(mCbvSrvUavDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		miscTexDescriptor.Offset(mDescHeapIdx.mCubeMapIndex, mCbvSrvUavDescriptorSize);
		outCmdList->SetGraphicsRootDescriptorTable(mRSManager.GetMiscTextureMapIndex(), miscTexDescriptor);
	}

	// Bind all the textures used in this scene.
	// Observe that we only have to specify the first descriptor in the table.  
	// The root signature knows how many descriptors are expected in the table.
	outCmdList->SetGraphicsRootDescriptorTable(
		mRSManager.GetTextureMapIndex(),
		mCbvSrvUavDescriptorHeap->GetGPUDescriptorHandleForHeapStart()
	);

	// Bind the texture for extracing animations data.
	outCmdList->SetGraphicsRootDescriptorTable(
		mRSManager.GetAnimationsMapIndex(),
		mAnimsMap.GetAnimationsMapSrv()
	);
}

void DxRenderer::BindRootConstants(ID3D12GraphicsCommandList* outCmdList) {
	outCmdList->SetGraphicsRoot32BitConstants(
		mRSManager.GetConstSettingsIndex(),
		1,
		&MaxInstanceCount,
		0
	);

	outCmdList->SetGraphicsRoot32BitConstants(
		mRSManager.GetConstSettingsIndex(),
		1,
		&mEffectEnabled,
		1
	);
}

GameResult DxRenderer::DrawSceneToShadowMap(UINT inTid) {
	ID3D12CommandAllocator* cmdListAlloc = mCurrFrameResource->mCmdListAllocs[inTid].Get();
	ID3D12GraphicsCommandList* cmdList = mCommandLists[inTid].Get();

	ReturnIfFailed(cmdList->Reset(cmdListAlloc, mPsoManager.GetShadowPsoPtr()));
	cmdList->SetGraphicsRootSignature(mRSManager.GetBasicRootSignature());

	if (inTid == 0) {
		// Change to DEPTH_WRITE.
		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mShadowMap.Resource(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE));
	}

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvSrvUavDescriptorHeap.Get() };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	BindViews(cmdList, true);
	BindDescriptorTables(cmdList, true);
	BindRootConstants(cmdList);

	cmdList->RSSetViewports(1, &mShadowMap.Viewport());
	cmdList->RSSetScissorRects(1, &mShadowMap.ScissorRect());

	// Specify the buffers we are going to render to.
	cmdList->OMSetRenderTargets(0, nullptr, false, &mShadowMap.Dsv());

	//
	// Draw shador for opaque. 
	//
#ifdef MT_World
	const auto& opaque = mRitemLayer[RenderLayer::Opaque];

	UINT numRitems = static_cast<UINT>(opaque.size());
	UINT eachNumRitems = numRitems / mNumThreads;
	UINT remaining = numRitems % mNumThreads;

	UINT begin = inTid * eachNumRitems + (inTid < remaining ? inTid : remaining);
	UINT end = begin + eachNumRitems + (inTid < remaining ? 1 : 0);

	DrawRenderItems(cmdList, opaque.data(), begin, end);
#else
	DrawRenderItems(cmdList, mRitemLayer[RenderLayer::Opaque]);
#endif // MT_World

	cmdList->SetPipelineState(mPsoManager.GetSkinnedShadowPsoPtr());
	//
	// Draw shadow for skinned opaque.
	//
#ifdef MT_World
	const auto& skinnedOpaque = mRitemLayer[RenderLayer::SkinnedOpaque];

	numRitems = static_cast<UINT>(skinnedOpaque.size());
	eachNumRitems = numRitems / mNumThreads;
	remaining = numRitems % mNumThreads;

	begin = inTid * eachNumRitems + (inTid < remaining ? inTid : remaining);
	end = begin + eachNumRitems + (inTid < remaining ? 1 : 0);

	DrawRenderItems(cmdList, skinnedOpaque.data(), begin, end);
#else
	DrawRenderItems(cmdList, mRitemLayer[RenderLayer::SkinnedOpaque]);
#endif

	if (inTid == mNumThreads - 1) {
		// Change back to PIXEL_SHADER_RESOURCE so we can read the texture in a shader.
		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mShadowMap.Resource(),
			D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
	}

	// Done recording commands.
	ReturnIfFailed(cmdList->Close());

	SyncHost(mSpinlockBarrier);

	if (inTid == 0) {
		std::vector<ID3D12CommandList*> cmdsLists;
		for (UINT i = 0; i < mNumThreads; ++i)
			cmdsLists.push_back(mCommandLists[i].Get());

		mCommandQueue->ExecuteCommandLists(static_cast<UINT>(cmdsLists.size()), cmdsLists.data());
	}

	return GameResultOk;
}

GameResult DxRenderer::DrawSceneToGBuffer(UINT inTid) {
	auto diffuseMap = mGBuffer.GetDiffuseMap();
	auto normalMap = mGBuffer.GetNormalMap();
	auto specMap = mGBuffer.GetSpecularMap();

	ID3D12CommandAllocator* cmdListAlloc = mCurrFrameResource->mCmdListAllocs[inTid].Get();
	ID3D12GraphicsCommandList* cmdList = mCommandLists[inTid].Get();

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	ReturnIfFailed(cmdList->Reset(cmdListAlloc, mPsoManager.GetGBufferPsoPtr()));
	cmdList->SetGraphicsRootSignature(mRSManager.GetBasicRootSignature());

	if (inTid == 0) {
		// Change to RENDER_TARGET.
		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
			diffuseMap,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET));
		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
			normalMap,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET));
		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
			specMap,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET));
	}

	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvSrvUavDescriptorHeap.Get() };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	BindViews(cmdList, false);
	BindDescriptorTables(cmdList, true);
	BindRootConstants(cmdList);

	cmdList->RSSetViewports(1, &mScreenViewport);
	cmdList->RSSetScissorRects(1, &mScissorRect);

	// Specify the buffers we are going to render to.
	cmdList->OMSetRenderTargets(GBuffer::NumRenderTargets, &mGBuffer.GetDiffuseMapRtv(), true, &DepthStencilView());

	//
	// Draw gbuffer for opaque.
	//
#ifdef MT_World
	const auto& opaque = mRitemLayer[RenderLayer::Opaque];

	UINT numRitems = static_cast<UINT>(opaque.size());
	UINT eachNumRitems = numRitems / mNumThreads;
	UINT remaining = numRitems % mNumThreads;

	UINT begin = inTid * eachNumRitems + (inTid < remaining ? inTid : remaining);
	UINT end = begin + eachNumRitems + (inTid < remaining ? 1 : 0);

	DrawRenderItems(cmdList, opaque.data(), begin, end);
#else
	DrawRenderItems(cmdList, mRitemLayer[RenderLayer::Opaque]);
#endif

	cmdList->SetPipelineState(mPsoManager.GetSkinnedGBufferPsoPtr());
	//
	// Draw gbuffer for skinned opaque.
	//
#ifdef MT_World
	const auto& skinnedOpaque = mRitemLayer[RenderLayer::SkinnedOpaque];

	numRitems = static_cast<UINT>(skinnedOpaque.size());
	eachNumRitems = numRitems / mNumThreads;
	remaining = numRitems % mNumThreads;

	begin = inTid * eachNumRitems + (inTid < remaining ? inTid : remaining);
	end = begin + eachNumRitems + (inTid < remaining ? 1 : 0);

	DrawRenderItems(cmdList, skinnedOpaque.data(), begin, end);
#else
	DrawRenderItems(cmdList, mRitemLayer[RenderLayer::SkinnedOpaque]);
#endif

	if (inTid == mNumThreads - 1) {
		// Change back to PIXEL_SHADER_RESOURCE so we can read the texture in a shader.
		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
			specMap,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
			normalMap,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
			diffuseMap,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
	}

	// Done recording commands.
	ReturnIfFailed(cmdList->Close());

	SyncHost(mSpinlockBarrier);

	if (inTid == 0) {
		std::vector<ID3D12CommandList*> cmdsLists;
		for (UINT i = 0; i < mNumThreads; ++i)
			cmdsLists.push_back(mCommandLists[i].Get());

		mCommandQueue->ExecuteCommandLists(static_cast<UINT>(cmdsLists.size()), cmdsLists.data());
	}

	return GameResultOk;
}

GameResult DxRenderer::DrawPreRenderingPass(ID3D12GraphicsCommandList* outCmdList, ID3D12CommandAllocator* inCmdListAlloc) {
	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	ReturnIfFailed(outCmdList->Reset(inCmdListAlloc, nullptr));

	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvSrvUavDescriptorHeap.Get() };
	outCmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	outCmdList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			mDepthStencilBuffer.Get(),
			D3D12_RESOURCE_STATE_DEPTH_READ,
			D3D12_RESOURCE_STATE_DEPTH_WRITE
		)
	);

	// Compute SSAO.
	if (mEffectEnabled & EffectEnabled::ESsao) {
		outCmdList->SetGraphicsRootSignature(mRSManager.GetSsaoRootSignature());
		mSsao.ComputeSsao(outCmdList, mCurrFrameResource, 3);
	}

	// Done recording commands.
	ReturnIfFailed(outCmdList->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { outCmdList };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	return GameResultOk;
}

GameResult DxRenderer::DrawMainRenderingPass(ID3D12GraphicsCommandList* outCmdList, ID3D12CommandAllocator* inCmdListAlloc) {
	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	ReturnIfFailed(outCmdList->Reset(inCmdListAlloc, mPsoManager.GetMainPassPsoPtr()));
	// Rebind state whenever graphics root signature changes.
	outCmdList->SetGraphicsRootSignature(mRSManager.GetBasicRootSignature());

	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvSrvUavDescriptorHeap.Get() };
	outCmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	outCmdList->RSSetViewports(1, &mScreenViewport);
	outCmdList->RSSetScissorRects(1, &mScissorRect);

	// Clear the back buffer and depth buffer.
	outCmdList->ClearRenderTargetView(mMainPass.GetMainPassMapView1(), Colors::Black, 0, nullptr);
	outCmdList->ClearRenderTargetView(mMainPass.GetMainPassMapView2(), Colors::Black, 0, nullptr);

	// Indicate a state transition on the resource usage.
	const D3D12_RESOURCE_BARRIER resourceBarriers[] = {
		CD3DX12_RESOURCE_BARRIER::Transition(
			mMainPass.GetMainPassMap1(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		),
		CD3DX12_RESOURCE_BARRIER::Transition(
			mMainPass.GetMainPassMap2(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		)
	};
	outCmdList->ResourceBarrier(
		2,
		resourceBarriers
	);

	outCmdList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			mDepthStencilBuffer.Get(),
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			D3D12_RESOURCE_STATE_DEPTH_READ
		)
	);

	// Specify the buffers we are going to render to.
	outCmdList->OMSetRenderTargets(MainPass::NumRenderTargets, &mMainPass.GetMainPassMapView1(), true, &DepthStencilView());	

	BindViews(outCmdList, false);
	BindDescriptorTables(outCmdList, false);
	BindRootConstants(outCmdList);

	// Draw fullscreen quad.
	outCmdList->IASetVertexBuffers(0, 0, nullptr);
	outCmdList->IASetIndexBuffer(nullptr);
	outCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	outCmdList->DrawInstanced(6, 1, 0, 0);

	// Read to stick notes.
	//outCmdList->SetPipelineState(mPSOs["sky"].Get());
	//auto ritems = mRitemLayer[RenderLayer::Sky];
	//DrawRenderItems(outCmdList, ritems.data(), ritems.size());

	// Done recording commands.
	ReturnIfFailed(outCmdList->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { outCmdList };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	return GameResultOk;
}

GameResult DxRenderer::DrawPostRenderingPass(ID3D12GraphicsCommandList* outCmdList, ID3D12CommandAllocator* inCmdListAlloc) {
	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	ReturnIfFailed(outCmdList->Reset(inCmdListAlloc, nullptr));

	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvSrvUavDescriptorHeap.Get() };
	outCmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	const D3D12_RESOURCE_BARRIER resourceBarriers[] = {
		CD3DX12_RESOURCE_BARRIER::Transition(
			mMainPass.GetMainPassMap1(),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
		),
		CD3DX12_RESOURCE_BARRIER::Transition(
			mMainPass.GetMainPassMap2(),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
		)
	};
	outCmdList->ResourceBarrier(
		2,
		resourceBarriers
	);

	// Compute SSR.
	if (mEffectEnabled & EffectEnabled::ESsr) {
		outCmdList->SetGraphicsRootSignature(mRSManager.GetSsrRootSignature());
		mSsr.ComputeSsr(outCmdList, mCurrFrameResource, 3);
	}

	outCmdList->SetGraphicsRootSignature(mRSManager.GetPostPassRootSignature());
	outCmdList->SetPipelineState(mPsoManager.GetPostPassPsoPtr());

	auto postPassCBAddress = mCurrFrameResource->mPostPassCB.Resource()->GetGPUVirtualAddress();
	outCmdList->SetGraphicsRootConstantBufferView(0, postPassCBAddress);

	outCmdList->SetGraphicsRoot32BitConstants(1, 1, &mEffectEnabled, 0);

	outCmdList->SetGraphicsRootDescriptorTable(2, GetGpuSrv(mDescHeapIdx.mCubeMapIndex));

	outCmdList->RSSetViewports(1, &mScreenViewport);
	outCmdList->RSSetScissorRects(1, &mScissorRect);

	// Clear the back buffer and depth buffer.
	outCmdList->ClearRenderTargetView(CurrentBackBufferView(), Colors::Black, 0, nullptr);

	// Specify the buffers we are going to render to.
	outCmdList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	outCmdList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			CurrentBackBuffer(),
			D3D12_RESOURCE_STATE_PRESENT,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		)
	);

	// Draw fullscreen quad.
	outCmdList->IASetVertexBuffers(0, 0, nullptr);
	outCmdList->IASetIndexBuffer(nullptr);
	outCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	outCmdList->DrawInstanced(6, 5, 0, 0);

	// Compute Bloom
	if (mEffectEnabled & EffectEnabled::EBloom) {
		outCmdList->SetGraphicsRootSignature(mRSManager.GetBloomRootSignature());
		mBloom.ComputeBloom(outCmdList, mCurrFrameResource, 3);
	}

	// Done recording commands.
	ReturnIfFailed(outCmdList->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { outCmdList };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	return GameResultOk;
}

GameResult DxRenderer::DrawDebugRenderingPass(ID3D12GraphicsCommandList* outCmdList, ID3D12CommandAllocator* inCmdListAlloc) {
	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	ReturnIfFailed(outCmdList->Reset(inCmdListAlloc, mPsoManager.GetSkeletonPsoPtr()));
	outCmdList->SetGraphicsRootSignature(mRSManager.GetBasicRootSignature());

	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvSrvUavDescriptorHeap.Get() };
	outCmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	outCmdList->RSSetViewports(1, &mScreenViewport);
	outCmdList->RSSetScissorRects(1, &mScissorRect);

	// Specify the buffers we are going to render to.
	outCmdList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	BindViews(outCmdList, false);
	BindDescriptorTables(outCmdList, false);
	BindRootConstants(outCmdList);

	if (bDrawDebugSkeletonsEnabled) {
		// Draw skeletons lines for debugging.
		auto ritems = mRitemLayer[RenderLayer::Skeleton];
		DrawRenderItems(outCmdList, ritems.data(), ritems.size());
	}

	if (bDrawDeubgWindowsEnabled) {
		// Draw quad render items for debugging.
		outCmdList->SetPipelineState(mPsoManager.GetDebugPsoPtr());

		// Draw fullscreen quad.
		outCmdList->IASetVertexBuffers(0, 0, nullptr);
		outCmdList->IASetIndexBuffer(nullptr);
		outCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		outCmdList->DrawInstanced(6, 5, 0, 0);
	}

	// Done recording commands.
	ReturnIfFailed(outCmdList->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { outCmdList };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	return GameResultOk;
}

GameResult DxRenderer::DrawSceneToRenderTarget() {	
	ID3D12CommandAllocator* cmdListAlloc = mCurrFrameResource->mCmdListAllocs[0].Get();
	ID3D12GraphicsCommandList* cmdList = mCommandLists[0].Get();

	CheckGameResult(DrawPreRenderingPass(cmdList, cmdListAlloc));
	CheckGameResult(DrawMainRenderingPass(cmdList, cmdListAlloc));
	CheckGameResult(DrawPostRenderingPass(cmdList, cmdListAlloc));
	CheckGameResult(DrawDebugRenderingPass(cmdList, cmdListAlloc));

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	ReturnIfFailed(cmdList->Reset(cmdListAlloc, nullptr));
	cmdList->SetGraphicsRootSignature(nullptr);

	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvSrvUavDescriptorHeap.Get() };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	cmdList->RSSetViewports(1, &mScreenViewport);
	cmdList->RSSetScissorRects(1, &mScissorRect);

	// Specify the buffers we are going to render to.
	cmdList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	//
	// Draw texts.
	//
	DrawTexts();

	// Indicate a state transition on the resource usage.
	cmdList->ResourceBarrier(
		1, 
		&CD3DX12_RESOURCE_BARRIER::Transition(
			CurrentBackBuffer(),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PRESENT
		)
	);

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

	return GameResultOk;
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