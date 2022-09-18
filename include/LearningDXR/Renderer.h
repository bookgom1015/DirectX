#pragma once

const int gNumFrameResources = 3;

#include "LearningDXR/LowRenderer.h"
#include "LearningDXR/GameTimer.h"
#include "LearningDXR/ShaderManager.h"
#include "LearningDXR/FrameResource.h"
#include "LearningDXR/Mesh.h"
#include "LearningDXR/RTXStructures.h"
#include "common/MathHelper.h"

const std::wstring ShaderFilePathW = L".\\..\\..\\..\\..\\Assets\\Shaders\\";
const std::string ShaderFilePath = ".\\..\\..\\..\\..\\Assets\\Shaders\\";

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
protected:
	enum class GlobalRootSignatureParams : UINT {
		EOutput = 0,
		EAccelerationStructure,
		EPassCB,
		EGeometryBuffers,
		Count
	};

	enum class LocalRootSignatureParams : UINT {
		EObjectCB = 0,
		Count
	};

	enum class DXRDescriptors : INT {
		EOutputUAV = 0,
		EGeomtrySRVs = 1
	};
	
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

	// Shared
	GameResult BuildFrameResources();
	GameResult BuildGeometries();
	GameResult BuildMaterials();

	// Raterization
	GameResult CompileShaders();
	GameResult BuildRootSignatures();
	GameResult BuildResources();
	GameResult BuildDescriptorHeaps();
	GameResult BuildDescriptors();
	GameResult BuildPSOs();
	GameResult BuildRenderItems();

	// Raytracing
	GameResult CompileDXRShaders();
	GameResult BuildDXRRootSignatures();
	GameResult BuildDXRResources();
	GameResult BuildDXRDescriptorHeaps();
	GameResult BuildDXRDescriptors();
	GameResult BuildBLAS();
	GameResult BuildTLAS();
	GameResult BuildDXRPSOs();
	GameResult BuildDXRObjectCB();
	GameResult BuildShaderTables();

	GameResult UpdateCamera(const GameTimer& gt);

	GameResult UpdateObjectCB(const GameTimer& gt);
	GameResult UpdatePassCB(const GameTimer& gt);
	GameResult UpdateMaterialCB(const GameTimer& gt);

	GameResult UpdateDXRPassCB(const GameTimer& gt);

	GameResult Rasterize();
	GameResult Raytrace();
	GameResult DrawRenderItems();

private:
	bool bIsCleanedUp = false;
	bool bRaytracing = false;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;

	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	ShaderManager mShaderManager;

	DirectX::XMFLOAT3 mEyePos = { -1.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT4X4 mView = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 mProj = MathHelper::Identity4x4();

	float mTheta = 1.5f * DirectX::XM_PI;
	float mPhi = 0.5f * DirectX::XM_PI;
	float mRadius = 15.0f;

	//
	// Rasterization
	//
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPSOs;

	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDescriptorHeap;
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> mMainRenderTargets;

	PassConstants mMainPassCB;

	//
	// Raytracing
	//
	Microsoft::WRL::ComPtr<ID3D12Resource> mDXROutput;
	
	AccelerationStructureBuffer mBLAS;
	AccelerationStructureBuffer mTLAS;
	
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mGlobalRootSignature;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mLocalRootSignature;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDXRDescriptorHeap;

	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12StateObject>> mDXRPSOs;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12StateObjectProperties>> mDXRPSOProps;

	DXRObjectCB mDXRObjectCB;
	DXRPassConstants mDXRPassCB;

	Microsoft::WRL::ComPtr<ID3D12Resource> mRayGenShaderTable;
	Microsoft::WRL::ComPtr<ID3D12Resource> mHitGroupShaderTable;
	Microsoft::WRL::ComPtr<ID3D12Resource> mMissShaderTable;
};