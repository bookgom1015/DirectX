//***************************************************************************************
// BlurApp.cpp by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

#include "common/d3dApp.h"
#include "common/MathHelper.h"
#include "common/UploadBuffer.h"
#include "common/GeometryGenerator.h"
#include "FrameResource.h"
#include "Waves.h"
#include "BlurFilter.h"
#include "GpuWaves.h"
#include "SobelFilter.h"
#include "RenderTarget.h"

//#define Ex1
//#define Ex2 
//#define Ex3 // Bind resource to ConsumeStructuredBuffer appropriately
//#define Ex4
#define Ex5
//#define Ex6

const int gNumFrameResources = 3;

// Lightweight structure stores parameters to draw a shape.  This will
// vary from app-to-app.
struct RenderItem {
	RenderItem() = default;

	// World matrix of the shape that describes the object's local space
	// relative to the world space, which defines the position, orientation,
	// and scale of the object in the world.
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();

	DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

#if defined(Ex5) || defined(Ex6)
	// Used for GPU waves render items.
	DirectX::XMFLOAT2 DisplacementMapTexelSize = { 1.0f, 1.0f };
	float GridSpatialStep = 1.0f;
#endif

	// Dirty flag indicating the object data has changed and we need to update the constant buffer.
	// Because we have an object cbuffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify obect data we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int NumFramesDirty = gNumFrameResources;

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	UINT ObjCBIndex = -1;

	Material* Mat = nullptr;
	MeshGeometry* Geo = nullptr;

	// Primitive topology.
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// DrawIndexedInstanced parameters.
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;
};

enum class RenderLayer : int {
	Opaque = 0,
	Transparent,
	AlphaTested,
	GpuWaves,
	Count
};

class BlurApp : public D3DApp {
public:
	BlurApp(HINSTANCE hInstance);
	virtual ~BlurApp();

private:
	BlurApp(const BlurApp& src) = delete;
	BlurApp(BlurApp&& src) = delete;
	BlurApp& operator=(const BlurApp& rhs) = delete;
	BlurApp& operator=(BlurApp&& rhs) = delete;

public:
	virtual bool Initialize() override;

private:
	virtual void OnResize() override;
	virtual void Update(const GameTimer& gt) override;
	virtual void Draw(const GameTimer& gt) override;
	virtual void BeginLoop() override;
	virtual void BeginDestroy() override;

	virtual void OnMouseDown(WPARAM btnState, int x, int y) override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y) override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y) override;

	void OnKeyboardInput(const GameTimer& gt);
	void UpdateCamera(const GameTimer& gt);
	void AnimateMaterials(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMaterialCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);
	void UpdateWaves(const GameTimer& gt);

	void LoadTextures();
	void BuildRootSignature();
	void BuildPostProcessRootSignature();
	void BuildDescriptorHeaps();
	void BuildShadersAndInputLayout();
	void BuildLandGeometry();
	void BuildWavesGeometry();
	void BuildBoxGeometry();
	void BuildPSOs();
	void BuildFrameResources();
	void BuildMaterials();
	void BuildRenderItems();
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

	float GetHillsHeight(float x, float z) const;
	DirectX::XMFLOAT3 GetHillsNormal(float x, float z) const;

	void BuildVectorArrays();
	void BuildWavesRootSignature();
	void UpdateWavesGPU(const GameTimer& gt);
	void CreateRtvAndDsvDescriptorHeaps() override;
	void DrawFullscreenQuad(ID3D12GraphicsCommandList* cmdList);
	void BuildSkullGeometry();

private:
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	UINT mCbvSrvUavDescriptorSize = 0;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mPostProcessRootSignature = nullptr;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mCbvSrvUavDescriptorHeap = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPSOs;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	RenderItem* mWavesRitem = nullptr;

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	// Render items divided by PSO.
	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];

#if defined(Ex5) || defined(Ex6)
	std::unique_ptr<GpuWaves> mWaves;
#else
	std::unique_ptr<Waves> mWaves;
#endif

	std::unique_ptr<BlurFilter> mBlurFilter;

	PassConstants mMainPassCB;

	DirectX::XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT4X4 mView = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 mProj = MathHelper::Identity4x4();

	float mTheta = 1.5f * DirectX::XM_PI;
	float mPhi = 0.2f * DirectX::XM_PI;
	float mRadius = 15.0f;

	float mSunTheta = 1.25f * DirectX::XM_PI;
	float mSunPhi = DirectX::XM_PIDIV4;

	POINT mLastMousePos;

#if defined(Ex1) || defined(Ex2) || defined(Ex3)
	Microsoft::WRL::ComPtr<ID3D12Resource> mSrv = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> mUav = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> mUploadBuffer = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> mReadbackBuffer = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> mConsumeUav = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> mAppendUav = nullptr;
#elif defined(Ex5)
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mWavesRootSignature = nullptr;
#elif defined(Ex6)
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mWavesRootSignature = nullptr;

	std::unique_ptr<RenderTarget> mOffscreenRT = nullptr;

	std::unique_ptr<SobelFilter> mSobelFilter = nullptr;
#endif
};