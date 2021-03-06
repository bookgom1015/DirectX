#pragma once

#include "DX12Game/DxLowRenderer.h"

#ifndef UsingVulkan

#include <DescriptorHeap.h>
#include <GraphicsMemory.h>
#include <SpriteBatch.h>
#include <SpriteFont.h>

#include "DX12Game/Renderer.h"
#include "DX12Game/PsoManager.h"
#include "DX12Game/ShaderManager.h"
#include "DX12Game/AnimationsMap.h"
#include "DX12Game/FrameResource.h"
#include "DX12Game/GameCamera.h"
#include "DX12Game/DxLowRenderer.h"
#include "DX12Game/RootSignatureManager.h"
#include "DX12Game/ShadowMap.h"
#include "DX12Game/Ssao.h"
#include "DX12Game/GBuffer.h"
#include "DX12Game/Ssr.h"
#include "DX12Game/MainPass.h"
#include "DX12Game/Bloom.h"

//class Mesh;
//class Animation;

class DxRenderer : public DxLowRenderer, public Renderer {
private:
	using UpdateFunc = std::function<GameResult(DxRenderer&, const GameTimer&, UINT)>;
	using RenderPassFunc = std::vector<std::vector<std::function<GameResult(DxRenderer&, ID3D12GraphicsCommandList*, ID3D12CommandAllocator*)>>>;

private:
	enum RenderLayers : int {
		EOpaque = 0,
		ESkinnedOpaque,
		EOpaqueAlphaBlend,
		ESkinnedOpaqueAlphaBlend,
		EOpaqueSsr,
		EScreen,
		ESkeleton,
		EDebug,
		ESky,
		Count
	};

	enum BoundTypes : int {
		EAABB = 0,
		EOBB,
		ESphere
	};

	enum StencilMasks : UINT {
		ESsr = 1 << 0
	};

	// Lightweight structure stores parameters to draw a shape.  This will
	//  vary from app-to-app.
	struct RenderItem {
	public:
		// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
		int mObjCBIndex = -1;

		Material* mMat = nullptr;
		MeshGeometry* mGeo = nullptr;

		// Primitive topology.
		D3D12_PRIMITIVE_TOPOLOGY mPrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

		BoundTypes mBoundType;
		struct BoundingStruct {
			DirectX::BoundingBox mAABB;
			DirectX::BoundingOrientedBox mOBB;
			DirectX::BoundingSphere mSphere;
		} mBoundingUnion;

		std::vector<Game::InstanceData> mInstances;

		// DrawIndexedInstanced parameters.
		UINT mIndexCount = 0;
		UINT mStartIndexLocation = 0;
		UINT mBaseVertexLocation = 0;

		UINT mNumInstancesToDraw = 0;

	public:
		RenderItem() = default;

	private:
		RenderItem(const RenderItem& src) = delete;
		RenderItem(RenderItem&& src) = delete;
		RenderItem& operator=(const RenderItem& rhs) = delete;
		RenderItem& operator=(RenderItem&& rhs) = delete;
	};

	struct DescriptorHeapIndices {
		UINT mCubeMapIndex;
		UINT mBlurCubeMapIndex;
		UINT mMainPassMapIndex1;
		UINT mMainPassMapIndex2;
		UINT mDiffuseMapIndex;
		UINT mNormalMapIndex;
		UINT mDepthMapIndex;
		UINT mSpecularMapIndex;
		UINT mShadowMapIndex;
		UINT mSsaoAmbientMapIndex;
		UINT mSsrMapIndex;
		UINT mBloomMapIndex;
		UINT mBloomBlurMapIndex;
		UINT mAnimationsMapIndex;
		UINT mSsaoAdditionalMapIndex;
		UINT mSsrAdditionalMapIndex;
		UINT mBloomAdditionalMapIndex;
		UINT mDefaultFontIndex;
		UINT mCurrSrvHeapIndex;
	};

	struct LightingVariables {
		float mLightNearZ = 0.0f;
		float mLightFarZ = 0.0f;

		DirectX::XMFLOAT3 mLightPosW;
		DirectX::XMFLOAT4X4 mLightView = MathHelper::Identity4x4();
		DirectX::XMFLOAT4X4 mLightProj = MathHelper::Identity4x4();
		DirectX::XMFLOAT4X4 mShadowTransform = MathHelper::Identity4x4();

		float mLightRotationAngle = 0.0f;
		DirectX::XMFLOAT3 mBaseLightStrengths[3] = {
			DirectX::XMFLOAT3(0.7f, 0.6670f, 0.6423f),
			DirectX::XMFLOAT3(0.4f, 0.3811f, 0.367f),
			DirectX::XMFLOAT3(0.1f, 0.0952f, 0.0917f)
		};

		DirectX::XMFLOAT3 mBaseLightDirections[3] = {
			DirectX::XMFLOAT3(0.57735f, -0.57735f,	 0.57735f),
			DirectX::XMFLOAT3(-0.57735f, -0.57735f,	 0.57735f),
			DirectX::XMFLOAT3(0.0f,	 -0.707f,	-0.707f)
		};
	};

public:
	DxRenderer();
	virtual ~DxRenderer();

private:
	///
	// Renderer doesn't allow substitution and replication.
	///
	DxRenderer(const DxRenderer& src) = delete;
	DxRenderer(DxRenderer&& src) = delete;
	DxRenderer& operator=(const DxRenderer& rhs) = delete;
	DxRenderer& operator=(DxRenderer&& rhs) = delete;

public:
	virtual GameResult Initialize(
		UINT inClientWidth,
		UINT inClientHeight,
		UINT inNumThreads = 1,
		HWND hMainWnd = NULL,
		CVBarrier* inCV = nullptr,
		SpinlockBarrier* inSpinlock = nullptr) override;

	virtual void CleanUp() override;
	virtual GameResult Update(const GameTimer& gt, UINT inTid = 0) override;
	virtual GameResult Draw(const GameTimer& gt, UINT inTid = 0) override;
	virtual GameResult OnResize(UINT inClientWidth, UINT inClientHeight) override;

	virtual GameResult GetDeviceRemovedReason() const override;

	virtual void UpdateWorldTransform(const std::string& inRenderItemName,
		const DirectX::XMMATRIX& inTransform, bool inIsSkeletal = false) override;
	virtual void UpdateInstanceAnimationData(const std::string& inRenderItemName,
		UINT inAnimClipIdx, float inTimePos, bool inIsSkeletal = false) override;

	virtual void SetVisible(const std::string& inRenderItemName, bool inState) override;
	virtual void SetSkeletonVisible(const std::string& inRenderItemName, bool inState) override;

	virtual GameResult AddGeometry(const Mesh* inMesh) override;
	virtual void AddRenderItem(std::string& ioRenderItemName, const Mesh* inMesh) override;
	virtual GameResult AddMaterials(const std::unordered_map<std::string, MaterialIn>& inMaterials) override;

	virtual UINT AddAnimations(const std::string& inClipName, const Game::Animation& inAnim) override;
	virtual GameResult UpdateAnimationsMap() override;

protected:
	virtual GameResult CreateRtvAndDsvDescriptorHeaps() override;

private:
	GameResult ResetFrameResourceCmdListAlloc();
	GameResult ClearViews();

	void DrawTexts();
	void AddRenderItem(const std::string& inRenderItemName, const Mesh* inMesh, bool inIsNested);
	GameResult LoadDataFromMesh(const Mesh* inMesh, MeshGeometry* outGeo, DirectX::BoundingBox& inBound);
	GameResult LoadDataFromSkeletalMesh(const Mesh* inMesh, MeshGeometry* outGeo, DirectX::BoundingBox& inBound);

	GameResult AddSkeletonGeometry(const Mesh* inMesh);
	GameResult AddSkeletonRenderItem(const std::string& inRenderItemName, const Mesh* inMesh, bool inIsNested);

	GameResult AddTextures(const std::unordered_map<std::string, MaterialIn>& inMaterials);
	GameResult AddDescriptors(const std::unordered_map<std::string, MaterialIn>& inMaterials);

	///
	// Update helper classes
	///
	bool IsContained(BoundTypes inType, const RenderItem::BoundingStruct& inBound,
		const DirectX::BoundingFrustum& inFrustum, UINT inTid = 0);
	UINT UpdateEachInstances(RenderItem* inRitem);
	/// Update helper classes

	///
	// Update functions
	///
	GameResult AnimateMaterials(const GameTimer& gt, UINT inTid = 0);
	GameResult UpdateObjectCBsAndInstanceBuffers(const GameTimer& gt, UINT inTid = 0);
	GameResult UpdateMaterialBuffers(const GameTimer& gt, UINT inTid = 0);
	GameResult UpdateShadowTransform(const GameTimer& gt, UINT inTid = 0);
	GameResult UpdateMainPassCB(const GameTimer& gt, UINT inTid = 0);
	GameResult UpdateShadowPassCB(const GameTimer& gt, UINT inTid = 0);
	GameResult UpdatePostPassCB(const GameTimer& gt, UINT inTid = 0);
	GameResult UpdateSsaoCB(const GameTimer& gt, UINT inTid = 0);
	GameResult UpdateSsrCB(const GameTimer& gt, UINT inTid = 0);
	GameResult UpdateBloomCB(const GameTimer& gt, UINT inTid = 0);
	/// Update functions

	GameResult LoadBasicTextures();

	void BuildDescriptorHeapIndices(UINT inOffset);
	void BuildDescriptorsForEachHelperClass();

	GameResult BuildDescriptorHeaps();
	GameResult BuildShaders();
	GameResult BuildBasicGeometry();
	GameResult BuildBasicRenderItems();
	GameResult BuildBasicMaterials();
	GameResult BuildPSOs();
	GameResult BuildFrameResources();

	void DrawRenderItems(ID3D12GraphicsCommandList* outCmdList, RenderItem*const* inRitems, size_t inNum);
	void DrawRenderItems(ID3D12GraphicsCommandList* outCmdList, RenderItem*const* inRitems, size_t inBegin, size_t inEnd);

	void BindViews(ID3D12GraphicsCommandList* outCmdList, bool bShadowPass);
	void BindDescriptorTables(ID3D12GraphicsCommandList* outCmdList, bool bNullMiscTex);
	void BindRootConstants(ID3D12GraphicsCommandList* outCmdList);

	GameResult DrawShadowMap(UINT inTid = 0);
	GameResult DrawGBuffer(UINT inTid = 0);
	GameResult DrawSceneToBackBuffer(UINT inTid = 0);

	GameResult DrawSsao(ID3D12GraphicsCommandList* outCmdList, ID3D12CommandAllocator* inCmdListAlloc);
	GameResult DrawMainPass(ID3D12GraphicsCommandList* outCmdList, ID3D12CommandAllocator* inCmdListAlloc);
	GameResult DrawSsr(ID3D12GraphicsCommandList* outCmdList, ID3D12CommandAllocator* inCmdListAlloc);
	GameResult DrawBloom(ID3D12GraphicsCommandList* outCmdList, ID3D12CommandAllocator* inCmdListAlloc);

	GameResult DrawBackBuffer(ID3D12GraphicsCommandList* outCmdList, ID3D12CommandAllocator* inCmdListAlloc);
	GameResult DrawDebug(ID3D12GraphicsCommandList* outCmdList, ID3D12CommandAllocator* inCmdListAlloc);

	CD3DX12_CPU_DESCRIPTOR_HANDLE GetCpuSrv(int inIndex) const;
	CD3DX12_GPU_DESCRIPTOR_HANDLE GetGpuSrv(int inIndex) const;
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetDsv(int inIndex) const;
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetRtv(int inIndex) const;

private:
	bool bIsCleaned = false;

	std::vector<std::unique_ptr<Game::FrameResource>> mFrameResources;
	Game::FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mCbvSrvUavDescriptorHeap = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;

	std::vector<std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, Material*> mMaterialRefs;

	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mSkinnedInputLayout;

	PsoManager mPsoManager;
	ShaderManager mShaderManager;

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	// Render items divided by PSO.
	std::vector<RenderItem*> mRitemLayer[RenderLayers::Count];

	std::unordered_map<std::string, UINT> mDiffuseSrvHeapIndices;
	std::unordered_map<std::string, UINT> mNormalSrvHeapIndices;
	std::unordered_map<std::string, INT> mSpecularSrvHeapIndices;
	std::unordered_map<std::string, INT> mAlphaSrvHeapIndices;
	std::vector<std::string> mBuiltDiffuseTexDescriptors;
	std::vector<std::string> mBuiltNormalTexDescriptors;
	std::vector<std::string> mBuiltSpecularTexDescriptors;
	std::vector<std::string> mBuiltAlphaTexDescriptors;

	UINT mNumObjCB = 0;
	UINT mNumMatCB = 0;
	UINT mNumDescriptor = 0;

	CD3DX12_GPU_DESCRIPTOR_HANDLE mNullSrv;

	Game::PassConstants mMainPassCB;		// Index 0 of pass cbuffer.
	Game::PassConstants mShadowPassCB;	// Index 1 of pass cbuffer.

	GBuffer mGBuffer;
	ShadowMap mShadowMap;
	Ssao mSsao;
	AnimationsMap mAnimsMap;
	Ssr mSsr;
	MainPass mMainPass;
	Bloom mBloom;

	RootSignatureManager mRSManager;

	DescriptorHeapIndices mDescHeapIdx;
	LightingVariables mLightingVars;

	DirectX::BoundingFrustum mCamFrustum;
	DirectX::BoundingSphere mSceneBounds;

	std::vector<const Mesh*> mNestedMeshes;
	std::unordered_map<std::string /* Render-item name */, std::vector<RenderItem*> /* Draw args */> mRefRitems;
	std::unordered_map<std::string /* Render-item name */, UINT /* Instance index */> mInstancesIndex;
	std::unordered_map<const Mesh*, std::vector<RenderItem*>> mMeshToRitem;
	std::unordered_map<const Mesh*, std::vector<RenderItem*>> mMeshToSkeletonRitem;

	// Variables for drawing text on the screen.
	std::unique_ptr<DirectX::GraphicsMemory> mGraphicsMemory;
	std::unique_ptr<DirectX::SpriteFont> mDefaultFont;
	std::unique_ptr<DirectX::SpriteBatch> mSpriteBatch;

	const UINT MaxInstanceCount = 128;
	std::array<float, 2> mRootConstants;

	std::vector<DirectX::XMFLOAT4> mBlurWeights5;
	std::vector<DirectX::XMFLOAT4> mBlurWeights9;
	std::vector<DirectX::XMFLOAT4> mBlurWeights17;

	CVBarrier* mCVBarrier;
	SpinlockBarrier* mSpinlockBarrier;

	std::vector<UINT> mNumInstances;
	std::vector<std::vector<UpdateFunc>> mEachUpdateFunctions;

	RenderPassFunc mPreRenderingPasses;
	RenderPassFunc mMainRenderingPasses;
	RenderPassFunc mPostRenderingPasses;
};

#endif // UsingVulkan