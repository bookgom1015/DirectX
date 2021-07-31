#pragma once

#include "DX12Game/DxLowRenderer.h"

#include <DescriptorHeap.h>
#include <GraphicsMemory.h>
#include <SpriteBatch.h>
#include <SpriteFont.h>

#include "DX12Game/Renderer.h"
#include "DX12Game/AnimationsMap.h"
#include "DX12Game/FrameResource.h"
#include "DX12Game/GameCamera.h"
#include "DX12Game/DxLowRenderer.h"
#include "DX12Game/ShadowMap.h"
#include "DX12Game/Ssao.h"
#include "DX12Game/GBuffer.h"

class Mesh;
class Animation;

class DxRenderer : public DxLowRenderer, public Renderer {
private:
	enum RenderLayer : int {
		Opaque = 0,
		SkinnedOpaque,
		Skeleton,
		Debug,
		Sky,
		Count
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

		DirectX::BoundingBox mAABB;
		DirectX::BoundingOrientedBox mOBB;
		DirectX::BoundingSphere mSphere;

		GVector<InstanceData> mInstances;

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

	struct RootParameterIndices {
		UINT mObjectCBIndex;
		UINT mPassCBIndex;
		UINT mInstIdxBufferIndex;
		UINT mInstBufferIndex;
		UINT mMatBufferIndex;
		UINT mMiscTextureMapIndex;
		UINT mTextureMapIndex;
		UINT mConstSettingsIndex;
		UINT mAnimationsMapIndex;
	};

	struct DescriptorHeapIndices {
		UINT mSkyTexHeapIndex;
		UINT mBlurSkyTexHeapIndex;
#ifdef DeferredRendering
		UINT mDiffuseMapHeapIndex;
		UINT mNormalMapHeapIndex;
		UINT mDepthMapHeapIndex;
#endif
		UINT mShadowMapHeapIndex;
		UINT mSsaoHeapIndexStart;
		UINT mSsaoAmbientMapIndex;
		UINT mAnimationsMapIndex;
		UINT mNullCubeSrvIndex;
		UINT mNullBlurCubeSrvIndex;
		UINT mNullTexSrvIndex1;
		UINT mNullTexSrvIndex2;
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
	virtual GameResult Initialize(HWND hMainWnd, UINT inClientWidth, UINT inClientHeight) override;
	virtual void CleanUp() override;
	virtual GameResult Update(const GameTimer& gt) override;
	virtual GameResult Draw(const GameTimer& gt) override;
	virtual GameResult OnResize(UINT inClientWidth, UINT inClientHeight) override;

	virtual void UpdateWorldTransform(const std::string& inRenderItemName,
		const DirectX::XMMATRIX& inTransform, bool inIsSkeletal = false) override;
	virtual void UpdateInstanceAnimationData(const std::string& inRenderItemName,
		UINT inAnimClipIdx, float inTimePos, bool inIsSkeletal = false) override;

	virtual void SetVisible(const std::string& inRenderItemName, bool inState) override;
	virtual void SetSkeletonVisible(const std::string& inRenderItemName, bool inState) override;

	virtual GameResult AddGeometry(const Mesh* inMesh) override;
	virtual void AddRenderItem(std::string& ioRenderItemName, const Mesh* inMesh) override;
	virtual GameResult AddMaterials(const GUnorderedMap<std::string, MaterialIn>& inMaterials) override;

	virtual UINT AddAnimations(const std::string& inClipName, const Animation& inAnim) override;
	virtual GameResult UpdateAnimationsMap() override;

protected:
	virtual GameResult CreateRtvAndDsvDescriptorHeaps() override;

private:
	void DrawTexts();
	void AddRenderItem(const std::string& inRenderItemName, const Mesh* inMesh, bool inIsNested);
	GameResult LoadDataFromMesh(const Mesh* inMesh, MeshGeometry* outGeo, DirectX::BoundingBox& inBound);
	GameResult LoadDataFromSkeletalMesh(const Mesh* inMesh, MeshGeometry* outGeo, DirectX::BoundingBox& inBound);

	GameResult AddSkeletonGeometry(const Mesh* inMesh);
	GameResult AddSkeletonRenderItem(const std::string& inRenderItemName, const Mesh* inMesh, bool inIsNested);

	GameResult AddTextures(const GUnorderedMap<std::string, MaterialIn>& inMaterials);
	GameResult AddDescriptors(const GUnorderedMap<std::string, MaterialIn>& inMaterials);

	GameResult AnimateMaterials(const GameTimer& gt);
	GameResult UpdateObjectCBsAndInstanceBuffer(const GameTimer& gt);
	GameResult UpdateMaterialBuffer(const GameTimer& gt);
	GameResult UpdateShadowTransform(const GameTimer& gt);
	GameResult UpdateMainPassCB(const GameTimer& gt);
	GameResult UpdateShadowPassCB(const GameTimer& gt);
	GameResult UpdateSsaoCB(const GameTimer& gt);

	GameResult LoadBasicTextures();
	GameResult BuildRootSignature();
	GameResult BuildSsaoRootSignature();
	GameResult BuildDescriptorHeaps();
	GameResult BuildShadersAndInputLayout();
	GameResult BuildBasicGeometry();
	GameResult BuildBasicRenderItems();
	GameResult BuildBasicMaterials();
	GameResult BuildPSOs();
	GameResult BuildFrameResources();

	void DrawRenderItems(ID3D12GraphicsCommandList* outCmdList, const GVector<RenderItem*>& inRitems);

	//* Builds draw command list for rendering shadow map(depth buffer texture).
	void DrawSceneToShadowMap();
	//* Builds draw command list for SSAO.
	void DrawNormalsAndDepth();

	CD3DX12_CPU_DESCRIPTOR_HANDLE GetCpuSrv(int inIndex) const;
	CD3DX12_GPU_DESCRIPTOR_HANDLE GetGpuSrv(int inIndex) const;
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetDsv(int inIndex) const;
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetRtv(int inIndex) const;

private:
	bool bIsCleaned = false;

	GVector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mSsaoRootSignature = nullptr;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mCbvSrvUavDescriptorHeap = nullptr;

	GUnorderedMap<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	GUnorderedMap<std::string, std::unique_ptr<Material>> mMaterials;
	GUnorderedMap<std::string, std::unique_ptr<Texture>> mTextures;
	GUnorderedMap<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> mShaders;
	GUnorderedMap<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPSOs;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mSkinnedInputLayout;

	// List of all the render items.
	GVector<std::unique_ptr<RenderItem>> mAllRitems;

	// Render items divided by PSO.
	GVector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];

	GUnorderedMap<std::string, UINT> mDiffuseSrvHeapIndices;
	GUnorderedMap<std::string, UINT> mNormalSrvHeapIndices;
	GUnorderedMap<std::string, UINT> mSpecularSrvHeapIndices;
	GVector<std::string> mBuiltDiffuseTexDescriptors;
	GVector<std::string> mBuiltNormalTexDescriptors;
	GVector<std::string> mBuiltSpecularTexDescriptors;

	UINT mNumObjCB = 0;
	UINT mNumMatCB = 0;
	UINT mNumDescriptor = 0;

	CD3DX12_GPU_DESCRIPTOR_HANDLE mNullSrv;

	PassConstants mMainPassCB;		// Index 0 of pass cbuffer.
	PassConstants mShadowPassCB;	// Index 1 of pass cbuffer.

	ShadowMap mShadowMap;
	Ssao mSsao;
	AnimationsMap mAnimsMap;
	GBuffer mGBuffer;

	DescriptorHeapIndices mDescHeapIdx;
	LightingVariables mLightingVars;
	RootParameterIndices mRootParams;

	DirectX::BoundingFrustum mCamFrustum;
	DirectX::BoundingSphere mSceneBounds;

	GVector<const Mesh*> mNestedMeshes;
	GUnorderedMap<std::string /* Render-item name */, GVector<RenderItem*> /* Draw args */> mRefRitems;
	GUnorderedMap<std::string /* Render-item name */, UINT /* Instance index */> mInstancesIndex;
	GUnorderedMap<const Mesh*, GVector<RenderItem*>> mMeshToRitem;
	GUnorderedMap<const Mesh*, GVector<RenderItem*>> mMeshToSkeletonRitem;

	// Variables for drawing text on the screen.
	std::unique_ptr<DirectX::GraphicsMemory> mGraphicsMemory;
	std::unique_ptr<DirectX::SpriteFont> mDefaultFont;
	std::unique_ptr<DirectX::SpriteBatch> mSpriteBatch;

	GVector<float> mConstantSettings;
};