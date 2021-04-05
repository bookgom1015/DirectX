#pragma once

#include "DX12Game/LowRenderer.h"
#include "DX12Game/FrameResource.h"
#include "DX12Game/ShadowMap.h"
#include "DX12Game/Ssao.h"
#include "DX12Game/GameCamera.h"

// Lightweight structure stores parameters to draw a shape.  This will
// vary from app-to-app.
struct RenderItem {
	RenderItem() = default;
	RenderItem(const RenderItem& rhs) = delete;

	// World matrix of the shape that describes the object's local space
	// relative to the world space, which defines the position, orientation,
	// and scale of the object in the world.
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();

	DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	// Dirty flag indicating the object data has changed and we need to update the constant buffer.
	// Because we have an object cbuffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify obect data we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int NumFramesDirty = gNumFrameResources;

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	int ObjCBIndex = -1;

	Material* Mat = nullptr;
	MeshGeometry* Geo = nullptr;

	// Primitive topology.
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	DirectX::BoundingBox AABB;
	DirectX::BoundingOrientedBox OBB;
	DirectX::BoundingSphere Sphere;

	// DrawIndexedInstanced parameters.
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;

	// Only applicable to skinned render-items.
	int SkinnedCBIndex = -1;

	bool Visibility = true;
	bool IsCulled = false;
};

enum class RenderLayer : int {
	Opaque = 0,
	SkinnedOpaque,
	Skeleton,
	Debug,
	Sky,
	Count
};

class Mesh;

class Renderer : public LowRenderer {
public:
	Renderer();
	virtual ~Renderer();

private:
	//
	//* Renderer doesn't allow substitution and replication.
	//

	Renderer(const Renderer& src) = delete;
	Renderer(Renderer&& src) = delete;
	Renderer& operator=(const Renderer& rhs) = delete;
	Renderer& operator=(Renderer&& rhs) = delete;

public:
	//* Intializes LowRenderer(parent class) and builds miscellaneous.
	virtual bool Initialize(HWND hMainWnd, UINT inWidth, UINT inHeight) override;
	//* Updates frame resources, constant buffers and others is need to be updated.
	virtual void Update(const GameTimer& gt) override;
	//* Builds and executes draw commands list.
	virtual void Draw(const GameTimer& gt) override;

	//* Called When window is resized.
	virtual void OnResize(UINT inClientWidth, UINT inClientHeight) override;

	//* Updates world transform for the actor.
	void UpdateWorldTransform(const std::string& inRenderItemName, 
								const DirectX::XMMATRIX& inTransform, bool inIsSkeletal = false);
	//* Updates pose matrix data at the time for the actor.
	void UpdateSkinnedTransforms(const std::string& inRenderItemName, const std::vector<DirectX::XMFLOAT4X4>& inTransforms);

	//* Set visibility status for the render item.
	void SetVisible(const std::string& inRenderItemName, bool inStatus);
	//* Set visibility status for the render item's skeleton.
	void SetSkeletonVisible(const std::string& inRenderItemName, bool inStatus);

	//* Builds the geometry in the mesh.
	//* After that, Call the function AddMaterials.
	void AddGeometry(const Mesh* inMesh);
	//* Builds render item.
	//* Also, registers the instance name. if it was overlapped, append a suffix.
	void AddRenderItem(std::string& ioRenderItemName, const DirectX::XMMATRIX& inTransform, const Mesh* inMesh);
	//* First, loads diffuse, normal and specular textures.
	//* Second, builds descriptor heaps.
	//* Finally, builds materials.
	void AddMaterials(const std::unordered_map<std::string, MaterialIn>& inMaterials);

	virtual ID3D12Device* GetDevice() const override;
	virtual ID3D12GraphicsCommandList* GetCommandList() const override;

	GameCamera* GetMainCamera() const;
	void SetMainCamerea(GameCamera* inCamera);

	virtual bool IsValid() const override;

protected:
	//* Create render target view and depth stencil view heaps.
	virtual void CreateRtvAndDsvDescriptorHeaps() override;

private:
	//* Extracts vertices and indices data from the mesh and builds geometry.
	void LoadDataFromMesh(const Mesh* inMesh, MeshGeometry* outGeo, DirectX::BoundingBox& inBound);
	//* Extracts skinned vertices and indices data from the mesh and builds geometry.
	void LoadDataFromSkeletalMesh(const Mesh* inMesh, MeshGeometry* outGeo, DirectX::BoundingBox& inBound);

	//* Builds the skeleton geometry(for debugging) that is composed lines(2-vertices).
	void AddSkeletonGeometry(const Mesh* inMesh);
	//* Builds the skeleton(for debugging) render item.
	void AddSkeletonRenderItem(const std::string& inRenderItemName, const DirectX::XMMATRIX& inTransform, const Mesh* inMesh);

	//* Loads textures(diffuse, normal, specular...) and creates DDXTexture.
	void AddTextures(const std::unordered_map<std::string, MaterialIn>& inMaterials);
	//* Builds descriptors for textures(diffuse, normal, specular...).
	void AddDescriptors(const std::unordered_map<std::string, MaterialIn>& inMaterials);

	void AnimateMaterials(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateSkinnedCBs(const GameTimer& gt);
	void UpdateMaterialBuffer(const GameTimer& gt);
	void UpdateShadowTransform(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);
	void UpdateShadowPassCB(const GameTimer& gt);
	void UpdateSsaoCB(const GameTimer& gt);

	void LoadStandardTextures();
	void BuildRootSignature();
	void BuildSsaoRootSignature();
	void BuildDescriptorHeaps();
	void BuildShadersAndInputLayout();
	void BuildStandardGeometry();
	void BuildStandardRenderItems();
	void BuildStandardMaterials();
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

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPSOs;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mSkinnedInputLayout;

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;
	std::unordered_map<std::string, std::vector<RenderItem*>> mRefAllRitems;

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
	UINT mNumSkinnedCB = 0;

	UINT mSkyTexHeapIndex = 0;
	UINT mBlurSkyTexHeapIndex = 0;
	UINT mShadowMapHeapIndex = 0;
	UINT mSsaoHeapIndexStart = 0;
	UINT mSsaoAmbientMapIndex = 0;

	UINT mNumDescriptor = 0;

	UINT mCurrSrvHeapIndex = 0;

	UINT mNullCubeSrvIndex = 0;
	UINT mNullBlurCubeSrvIndex = 0;
	UINT mNullTexSrvIndex1 = 0;
	UINT mNullTexSrvIndex2 = 0;

	CD3DX12_GPU_DESCRIPTOR_HANDLE mNullSrv;

	PassConstants mMainPassCB;		// Index 0 of pass cbuffer.
	PassConstants mShadowPassCB;	// Index 1 of pass cbuffer.

	std::unique_ptr<ShadowMap> mShadowMap;

	std::unique_ptr<Ssao> mSsao;

	DirectX::BoundingSphere mSceneBounds;

	std::unordered_map<int /* Skinned constant buffer index */, std::vector<DirectX::XMFLOAT4X4>> mSkinnedInstances;
	std::unordered_map<std::string /* Render item name */, int> mSkinnedIndices;

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
		DirectX::XMFLOAT3(0.57735f, -0.57735f, 0.57735f),
		DirectX::XMFLOAT3(-0.57735f, -0.57735f, 0.57735f),
		DirectX::XMFLOAT3(0.0f, -0.707f, -0.707f)
	};

	GameCamera* mMainCamera = nullptr;

	UINT mObjectCBIndex = 0;
	UINT mSkinedCBIndex = 0;
	UINT mPassCBIndex = 0;
	UINT mMatBufferIndex = 0;
	UINT mMiscTextureMapIndex = 0;
	UINT mTextureMapIndex = 0;

	DirectX::BoundingFrustum mCamFrustum;
};