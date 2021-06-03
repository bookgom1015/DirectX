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
		RenderItem() = default;
		RenderItem(const RenderItem& rhs) = delete;

		// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
		int ObjCBIndex = -1;

		Material* Mat = nullptr;
		MeshGeometry* Geo = nullptr;

		// Primitive topology.
		D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

		DirectX::BoundingBox AABB;
		DirectX::BoundingOrientedBox OBB;
		DirectX::BoundingSphere Sphere;

		std::vector<InstanceData> Instances;

		// DrawIndexedInstanced parameters.
		UINT IndexCount = 0;
		UINT StartIndexLocation = 0;
		UINT BaseVertexLocation = 0;

		UINT NumInstancesToDraw = 0;
	};

	struct RootParameterIndices {
		UINT ObjectCBIndex;
		UINT PassCBIndex;
		UINT InstBufferIndex;
		UINT MatBufferIndex;
		UINT MiscTextureMapIndex;
		UINT TextureMapIndex;
		UINT AnimationsMapIndex;
	};

	struct DescriptorHeapIndices {
		UINT SkyTexHeapIndex;
		UINT BlurSkyTexHeapIndex;
		UINT ShadowMapHeapIndex;
		UINT SsaoHeapIndexStart;
		UINT SsaoAmbientMapIndex;
		UINT AnimationsMapIndex;
		UINT NullCubeSrvIndex;
		UINT NullBlurCubeSrvIndex;
		UINT NullTexSrvIndex1;
		UINT NullTexSrvIndex2;
		UINT DefaultFontIndex;
		UINT CurrSrvHeapIndex;
	};

	struct LightingVariables {
		float LightNearZ = 0.0f;
		float LightFarZ = 0.0f;

		DirectX::XMFLOAT3 LightPosW;
		DirectX::XMFLOAT4X4 LightView = MathHelper::Identity4x4();
		DirectX::XMFLOAT4X4 LightProj = MathHelper::Identity4x4();
		DirectX::XMFLOAT4X4 ShadowTransform = MathHelper::Identity4x4();

		float LightRotationAngle = 0.0f;
		DirectX::XMFLOAT3 BaseLightStrengths[3] = {
			DirectX::XMFLOAT3(0.7f, 0.6670f, 0.6423f),
			DirectX::XMFLOAT3(0.4f, 0.3811f, 0.367f),
			DirectX::XMFLOAT3(0.1f, 0.0952f, 0.0917f)
		};

		DirectX::XMFLOAT3 BaseLightDirections[3] = {
			DirectX::XMFLOAT3(0.57735f, -0.57735f, 0.57735f),
			DirectX::XMFLOAT3(-0.57735f, -0.57735f, 0.57735f),
			DirectX::XMFLOAT3(0.0f, -0.707f, -0.707f)
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
	virtual bool Initialize(HWND hMainWnd, UINT inWidth, UINT inHeight) override;
	virtual void Update(const GameTimer& gt) override;
	virtual void Draw(const GameTimer& gt) override;
	virtual void OnResize(UINT inClientWidth, UINT inClientHeight) override;

	void UpdateWorldTransform(const std::string& inRenderItemName, 
		const DirectX::XMMATRIX& inTransform, bool inIsSkeletal = false);
	void UpdateInstanceAnimationData(const std::string& inRenderItemName, 
		UINT inAnimClipIdx, float inTimePos, bool inIsSkeletal = false);

	void SetVisible(const std::string& inRenderItemName, bool inState);
	void SetSkeletonVisible(const std::string& inRenderItemName, bool inState);

	void AddGeometry(const Mesh* inMesh);
	void AddRenderItem(std::string& ioRenderItemName, const Mesh* inMesh);
	void AddMaterials(const std::unordered_map<std::string, MaterialIn>& inMaterials);
	UINT AddAnimations(const std::string& inClipName, const Animation& inAnim);

	void UpdateAnimationsMap();

	void AddOutputText(const std::wstring& inText, float inX, float inY, float inScale, const std::string& inNameId);
	void RemoveOutputText(const std::string& inName);

	virtual ID3D12Device* GetDevice() const override;
	virtual ID3D12GraphicsCommandList* GetCommandList() const override;

	GameCamera* GetMainCamera() const;
	void SetMainCamerea(GameCamera* inCamera);

	virtual bool IsValid() const override;

protected:
	virtual void CreateRtvAndDsvDescriptorHeaps() override;

private:
	void DrawTexts();
	void AddRenderItem(const std::string& inRenderItemName, const Mesh* inMesh, bool inIsNested);
	void LoadDataFromMesh(const Mesh* inMesh, MeshGeometry* outGeo, DirectX::BoundingBox& inBound);
	void LoadDataFromSkeletalMesh(const Mesh* inMesh, MeshGeometry* outGeo, DirectX::BoundingBox& inBound);

	void AddSkeletonGeometry(const Mesh* inMesh);
	void AddSkeletonRenderItem(const std::string& inRenderItemName, const Mesh* inMesh, bool inIsNested);

	void AddTextures(const std::unordered_map<std::string, MaterialIn>& inMaterials);
	void AddDescriptors(const std::unordered_map<std::string, MaterialIn>& inMaterials);

	void AnimateMaterials(const GameTimer& gt);
	void UpdateObjectCBsAndInstanceBuffer(const GameTimer& gt);
	void UpdateMaterialBuffer(const GameTimer& gt);
	void UpdateShadowTransform(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);
	void UpdateShadowPassCB(const GameTimer& gt);
	void UpdateSsaoCB(const GameTimer& gt);

	void LoadBasicTextures();
	void BuildRootSignature();
	void BuildSsaoRootSignature();
	void BuildDescriptorHeaps();
	void BuildShadersAndInputLayout();
	void BuildBasicGeometry();
	void BuildBasicRenderItems();
	void BuildBasicMaterials();
	void BuildPSOs();
	void BuildFrameResources();

	void DrawRenderItems(ID3D12GraphicsCommandList* outCmdList, const std::vector<RenderItem*>& inRitems);

	//* Builds draw command list for rendering shadow map(depth buffer texture).
	void DrawSceneToShadowMap();
	//* Builds draw command list for SSAO.
	void DrawNormalsAndDepth();

	CD3DX12_CPU_DESCRIPTOR_HANDLE GetCpuSrv(int inIndex) const;
	CD3DX12_GPU_DESCRIPTOR_HANDLE GetGpuSrv(int inIndex) const;
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetDsv(int inIndex) const;
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetRtv(int inIndex) const;

private:
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mSsaoRootSignature = nullptr;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mCbvSrvUavDescriptorHeap = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPSOs;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mSkinnedInputLayout;

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	// Render items divided by PSO.
	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];

	std::unordered_map<std::string, UINT> mDiffuseSrvHeapIndices;
	std::unordered_map<std::string, UINT> mNormalSrvHeapIndices;
	std::unordered_map<std::string, UINT> mSpecularSrvHeapIndices;
	std::vector<std::string> mBuiltDiffuseTexDescriptors;
	std::vector<std::string> mBuiltNormalTexDescriptors;
	std::vector<std::string> mBuiltSpecularTexDescriptors;

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

	std::vector<const Mesh*> mNestedMeshes;
	std::unordered_map<std::string /* Render-item name */, std::vector<RenderItem*> /* Draw args */> mRefRitems;
	std::unordered_map<std::string /* Render-item name */, UINT /* Instance index */> mInstancesIndex;
	std::unordered_map<const Mesh*, std::vector<RenderItem*>> mMeshToRitem;
	std::unordered_map<const Mesh*, std::vector<RenderItem*>> mMeshToSkeletonRitem;

	std::unique_ptr<AnimationsMap> mAnimsMap;

	// Variables for drawing text on the screen.
	std::unique_ptr<DirectX::GraphicsMemory> mGraphicsMemory;
	std::unique_ptr<DirectX::SpriteFont> mDefaultFont;
	std::unique_ptr<DirectX::SpriteBatch> mSpriteBatch;
	std::unordered_map<std::string /* Name id */, 
		std::pair<std::wstring /* Output text */, DirectX::SimpleMath::Vector3>> mOutputTexts;
};