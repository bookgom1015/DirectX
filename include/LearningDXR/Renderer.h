#pragma once

const int gNumFrameResources = 3;

#include "LearningDXR/LowRenderer.h"
#include "LearningDXR/GameTimer.h"
#include "common/MathHelper.h"

#include <unordered_map>

const std::wstring ShaderFilePathW = L".\\..\\..\\..\\..\\Assets\\Shaders\\";
const std::string ShaderFilePath = ".\\..\\..\\..\\..\\Assets\\Shaders\\";

class Camera;
class ShaderManager;

struct MeshGeometry;
struct Material;
struct FrameResource;
struct DXRObjectCB;
struct PassConstants;
struct DebugPassConstants;
struct AccelerationStructureBuffer;

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

enum class ERenderTypes {
	EOpaque = 0,
	EGizmo,
	Count
};

enum class ERasterRootSignatureParams {
	EObjectCB = 0,
	EPassCB,
	EMatCB,
	Count
};

enum class EDebugRootSignatureParams {
	EPassCB = 0,
	Count
};

enum class EGlobalRootSignatureParams {
	EOutput = 0,
	EAccelerationStructure,
	EPassCB,
	EGeometryBuffers,
	Count
};

enum class ELocalRootSignatureParams {
	EObjectCB = 0,
	Count
};

enum class EDXRDescriptors {
	EOutputUAV		= 0,	// 3 (gNumFrameResources)
	EGeomtrySRVs	= 3,	// 2 (vertices and indices)
	Count
};


class Renderer : public LowRenderer {	
public:
	Renderer();
	virtual ~Renderer();

private:
	Renderer(const Renderer& ref) = delete;
	Renderer(Renderer&& rval) = delete;
	Renderer& operator=(const Renderer& ref) = delete;
	Renderer& operator=(Renderer&& rval) = delete;

public:
	virtual bool Initialize(HWND hMainWnd, UINT width, UINT height) override;
	virtual void CleanUp() override;

	bool Update(const GameTimer& gt);
	bool Draw();

	virtual bool OnResize(UINT width, UINT height) override;

	__forceinline constexpr bool GetRenderType() const;
	void SetRenderType(bool bRaytrace);

	void SetCamera(Camera* pCam);

	__forceinline constexpr bool Initialized() const;

protected:
	virtual bool CreateRtvAndDsvDescriptorHeaps() override;

	void BuildDebugViewport();

	// Shared
	bool BuildFrameResources();
	bool BuildGeometries();
	bool BuildMaterials();

	// Raterization
	bool CompileShaders();
	bool BuildRootSignatures();
	bool BuildResources();
	bool BuildDescriptorHeaps();
	bool BuildDescriptors();
	bool BuildPSOs();
	bool BuildRenderItems();

	// Raytracing
	bool CompileDXRShaders();
	bool BuildDXRRootSignatures();
	bool BuildDXRResources();
	bool BuildDXRDescriptorHeaps();
	bool BuildDXRDescriptors();
	bool BuildBLAS();
	bool BuildTLAS();
	bool BuildDXRPSOs();
	bool BuildDXRObjectCB();
	bool BuildShaderTables();

	bool UpdateObjectCB(const GameTimer& gt);
	bool UpdatePassCB(const GameTimer& gt);
	bool UpdateMaterialCB(const GameTimer& gt);
	bool UpdateDebugPassCB(const GameTimer& gt);

	bool Rasterize();
	bool Raytrace();
	bool DrawDebugLayer();

	bool DrawRenderItems(const std::vector<RenderItem*>& ritems);

private:
	bool bIsCleanedUp;
	bool bInitialized;
	bool bRaytracing;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;

	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource;
	int mCurrFrameResourceIndex;

	std::unique_ptr<ShaderManager> mShaderManager;

	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12RootSignature>> mRootSignatures;

	Camera* mCamera;

	//
	// Rasterization
	//
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPSOs;

	std::vector<std::unique_ptr<RenderItem>> mAllRitems;
	std::unordered_map<ERenderTypes, std::vector<RenderItem*>> mRitems;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDescriptorHeap;
	
	std::unique_ptr<PassConstants> mMainPassCB;
	std::unique_ptr <DebugPassConstants> mDebugPassCB;

	D3D12_VIEWPORT mDebugViewport;
	D3D12_RECT mDebugScissorRect;

	//
	// Raytracing
	//
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> mDXROutputs;
	
	std::unique_ptr<AccelerationStructureBuffer> mBLAS;
	std::unique_ptr<AccelerationStructureBuffer> mTLAS;
	
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDXRDescriptorHeap;

	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12StateObject>> mDXRPSOs;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12StateObjectProperties>> mDXRPSOProps;

	std::unique_ptr<DXRObjectCB> mDXRObjectCB;

	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12Resource>> mShaderTables;
};

#include "LearningDXR/Renderer.inl"