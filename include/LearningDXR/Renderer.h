#pragma once

#include "LearningDXR/LowRenderer.h"
#include "DX12Game/GameUploadBuffer.h"
#include "DX12Game/ShaderManager.h"

struct Vertex {
	DirectX::XMFLOAT3 Pos;
	DirectX::XMFLOAT3 Normal;
	DirectX::XMFLOAT2 TexC;
	DirectX::XMFLOAT3 Tangent;
};

struct ObjectConstants {
	DirectX::XMFLOAT4X4 World;
	DirectX::XMFLOAT4X4 TexTransform;
};

struct PassConstants {
	DirectX::XMFLOAT4X4 View;
	DirectX::XMFLOAT4X4 InvView;
	DirectX::XMFLOAT4X4 Proj;
	DirectX::XMFLOAT4X4 InvProj;
	DirectX::XMFLOAT4X4 ViewProj;
	DirectX::XMFLOAT4X4 InvViewProj;
	DirectX::XMFLOAT3 EyePosW;
	float CBPerObjectPad1;
	DirectX::XMFLOAT4 AmbientLight;
	Light Lights[MaxLights];
};

struct FrameResource {
public:
	FrameResource(
		ID3D12Device* inDevice, 
		UINT inPassCount,
		UINT inObjectCount, 
		UINT inMaterialCount);
	virtual ~FrameResource() = default;

private:
	FrameResource(const FrameResource& src) = delete;
	FrameResource(FrameResource&& src) = delete;
	FrameResource& operator=(const FrameResource& rhs) = delete;
	FrameResource& operator=(FrameResource&& rhs) = delete;

public:
	GameResult Initialize();

public:
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;

	GameUploadBuffer<PassConstants> PassCB;
	GameUploadBuffer<ObjectConstants> ObjectCB;
	GameUploadBuffer<MaterialConstants> MaterialCB;

	UINT64 Fence = 0;

	ID3D12Device* Device;
	UINT PassCount;
	UINT ObjectCount;
	UINT MaterialCount;
};

struct RenderItem {
	RenderItem() = default;

	// World matrix of the shape that describes the object's local space
	// relative to the world space, which defines the position, orientation,
	// and scale of the object in thw world.
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();

	DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	// Dirty flag indicating the object data has changed and we need to update the constant buffer.
	// Because we have an object cbuffer for each FrameResource, we have to apply the
	// update to each FrameResource. Thus, when we modify object data we should set
	// NumFrameDirty = gNumFrameResources so that each frame resource gets the update.
	int NumFramesDirty = gNumFrameResources;

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	UINT ObjCBIndex = -1;

	Material* Mat = nullptr;
	MeshGeometry* Geo = nullptr;

	// Primitive topology.
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// DrawIndexedInstanced parameters.
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;
};

class Renderer : public LowRenderer {
public:
	Renderer();
	virtual ~Renderer();

private:
	Renderer(const Renderer& inRef) = delete;
	Renderer(Renderer&& inRVal) = delete;
	Renderer& operator=(const Renderer& inRef) = delete;
	Renderer& operator=(Renderer&& inRVal) = delete;

public:
	virtual GameResult Initialize(HWND hMainWnd, UINT inClientWidth = 800, UINT inClientHeight = 600) override;
	virtual void CleanUp() override;

	GameResult Update(const GameTimer& gt);
	GameResult Draw();

	virtual GameResult OnResize(UINT inClientWidth, UINT inClientHeight) override;

	bool GetRenderType() const;
	void SetRenderType(bool inValue);

protected:
	virtual GameResult CreateRtvAndDsvDescriptorHeaps() override;

	GameResult BuildGeometries();

	// Raterization
	GameResult CompileShader();
	GameResult BuildRootSignature();
	GameResult BuildResources();
	GameResult BuildDescriptorHeaps();
	GameResult BuildDescriptors();
	GameResult BuildPSOs();
	GameResult BuildMaterials();
	GameResult BuildFrameResources();
	GameResult BuildRenderItems();

	// Raytracing

	GameResult UpdateCamera(const GameTimer& gt);
	GameResult UpdateObjectCB(const GameTimer& gt);
	GameResult UpdatePassCB(const GameTimer& gt);
	GameResult UpdateMaterialCB(const GameTimer& gt);

	GameResult Rasterize();
	GameResult Raytrace();
	GameResult DrawRenderItems();

private:
	bool bIsCleanedUp = false;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, D3D12_RAYTRACING_GEOMETRY_DESC> mGeoDescs;

	Microsoft::WRL::ComPtr<ID3D12Resource> mBlasScratch;
	Microsoft::WRL::ComPtr<ID3D12Resource> mBlasResult;

	Microsoft::WRL::ComPtr<ID3D12Resource> mTlasInstanceDesc;
	Microsoft::WRL::ComPtr<ID3D12Resource> mTlasScratch;
	Microsoft::WRL::ComPtr<ID3D12Resource> mTlasResult;

	ShaderManager mShaderManager;
	DxcShaderManager mDxcShaderManager;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mGlobalRootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mLocalRootSignature = nullptr;

	RaytracingProgram mRayGenShader;
	RaytracingProgram mMissShader;
	RaytracingProgram mClosestHitShader;

	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPSOs;
	Microsoft::WRL::ComPtr<ID3D12StateObject> mRtPso;
	Microsoft::WRL::ComPtr<ID3D12StateObjectProperties> mRtPsoInfo;

	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;

	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	DirectX::XMFLOAT3 mEyePos = { -5.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT4X4 mView = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 mProj = MathHelper::Identity4x4();

	PassConstants mMainPassCB;

	float mTheta = 1.5f * DirectX::XM_PI;
	float mPhi = 0.2f * DirectX::XM_PI;
	float mRadius = 15.0f;

	bool bRaytracing = false;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mCbvSrvUavDescriptorHeaps = nullptr;
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> mMainRenderTargets;
};