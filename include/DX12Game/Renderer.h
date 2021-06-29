#pragma once

#include <DescriptorHeap.h>
#include <GraphicsMemory.h>
#include <SimpleMath.h>
#include <SpriteBatch.h>
#include <SpriteFont.h>
#include "DX12Game/AnimationsMap.h"
#include "DX12Game/FrameResource.h"
#include "DX12Game/GameCamera.h"
#include "DX12Game/LowRenderer.h"
#include "DX12Game/ShadowMap.h"
#include "DX12Game/Ssao.h"

class Mesh;
class Animation;

class Renderer : public LowRenderer {
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
			DirectX::XMFLOAT3( 0.57735f, -0.57735f,	 0.57735f),
			DirectX::XMFLOAT3(-0.57735f, -0.57735f,	 0.57735f),
			DirectX::XMFLOAT3( 0.0f,	 -0.707f,	-0.707f)
		};
	};

public:
	Renderer();
	virtual ~Renderer();

private:
	///
	// Renderer doesn't allow substitution and replication.
	///
	Renderer(const Renderer& src) = delete;
	Renderer(Renderer&& src) = delete;
	Renderer& operator=(const Renderer& rhs) = delete;
	Renderer& operator=(Renderer&& rhs) = delete;

public:
	virtual DxResult Initialize(HWND hMainWnd, UINT inWidth, UINT inHeight) override;
	virtual DxResult Update(const GameTimer& gt) override;
	virtual DxResult Draw(const GameTimer& gt) override;
	virtual DxResult OnResize(UINT inClientWidth, UINT inClientHeight) override;

	void UpdateWorldTransform(const std::string& inRenderItemName, 
		const DirectX::XMMATRIX& inTransform, bool inIsSkeletal = false);
	void UpdateInstanceAnimationData(const std::string& inRenderItemName, 
		UINT inAnimClipIdx, float inTimePos, bool inIsSkeletal = false);

	void SetVisible(const std::string& inRenderItemName, bool inState);
	void SetSkeletonVisible(const std::string& inRenderItemName, bool inState);

	DxResult AddGeometry(const Mesh* inMesh);
	void AddRenderItem(std::string& ioRenderItemName, const Mesh* inMesh);
	DxResult AddMaterials(const GUnorderedMap<std::string, MaterialIn>& inMaterials);

	UINT AddAnimations(const std::string& inClipName, const Animation& inAnim);
	DxResult UpdateAnimationsMap();

	void AddOutputText(const std::string& inNameId, const std::wstring& inText,
		float inX = 0.0f, float inY = 0.0f, float inScale = 1.0f, float inLifeTime = -1.0f);
	void RemoveOutputText(const std::string& inName);

	virtual ID3D12Device* GetDevice() const override;
	virtual ID3D12GraphicsCommandList* GetCommandList() const override;

	GameCamera* GetMainCamera() const;
	void SetMainCamerea(GameCamera* inCamera);

	virtual bool IsValid() const override;

protected:
	virtual DxResult CreateRtvAndDsvDescriptorHeaps() override;

private:
	void DrawTexts();
	void AddRenderItem(const std::string& inRenderItemName, const Mesh* inMesh, bool inIsNested);
	DxResult LoadDataFromMesh(const Mesh* inMesh, MeshGeometry* outGeo, DirectX::BoundingBox& inBound);
	DxResult LoadDataFromSkeletalMesh(const Mesh* inMesh, MeshGeometry* outGeo, DirectX::BoundingBox& inBound);

	DxResult AddSkeletonGeometry(const Mesh* inMesh);
	void AddSkeletonRenderItem(const std::string& inRenderItemName, const Mesh* inMesh, bool inIsNested);

	DxResult AddTextures(const GUnorderedMap<std::string, MaterialIn>& inMaterials);
	DxResult AddDescriptors(const GUnorderedMap<std::string, MaterialIn>& inMaterials);

	void AnimateMaterials(const GameTimer& gt);
	void UpdateObjectCBsAndInstanceBuffer(const GameTimer& gt);
	void UpdateMaterialBuffer(const GameTimer& gt);
	void UpdateShadowTransform(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);
	void UpdateShadowPassCB(const GameTimer& gt);
	void UpdateSsaoCB(const GameTimer& gt);

	DxResult LoadBasicTextures();
	DxResult BuildRootSignature();
	DxResult BuildSsaoRootSignature();
	DxResult BuildDescriptorHeaps();
	void BuildShadersAndInputLayout();
	DxResult BuildBasicGeometry();
	void BuildBasicRenderItems();
	void BuildBasicMaterials();
	DxResult BuildPSOs();
	DxResult BuildFrameResources();

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

	std::unique_ptr<ShadowMap> mShadowMap;
	std::unique_ptr<Ssao> mSsao;

	DescriptorHeapIndices mDescHeapIdx;
	LightingVariables mLightingVars;
	RootParameterIndices mRootParams;

	GameCamera* mMainCamera = nullptr;
	DirectX::BoundingFrustum mCamFrustum;
	DirectX::BoundingSphere mSceneBounds;

	GVector<const Mesh*> mNestedMeshes;
	GUnorderedMap<std::string /* Render-item name */, GVector<RenderItem*> /* Draw args */> mRefRitems;
	GUnorderedMap<std::string /* Render-item name */, UINT /* Instance index */> mInstancesIndex;
	GUnorderedMap<const Mesh*, GVector<RenderItem*>> mMeshToRitem;
	GUnorderedMap<const Mesh*, GVector<RenderItem*>> mMeshToSkeletonRitem;

	std::unique_ptr<AnimationsMap> mAnimsMap;

	// Variables for drawing text on the screen.
	std::unique_ptr<DirectX::GraphicsMemory> mGraphicsMemory;
	std::unique_ptr<DirectX::SpriteFont> mDefaultFont;
	std::unique_ptr<DirectX::SpriteBatch> mSpriteBatch;
	GUnorderedMap<std::string /* Name id */, 
		std::pair<std::wstring /* Output text */, DirectX::SimpleMath::Vector4>> mOutputTexts;

	GVector<float> mConstantSettings;
};