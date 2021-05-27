#pragma once

const size_t gNumBones = 512;

#include "DX12Game/FrameResource.h"
#include "common/UploadBuffer.h"

struct ObjectConstants {
	UINT InstanceIndex = 0;
	UINT ObjectPad0;
	UINT ObjectPad1;
	UINT ObjectPad2;
};

enum EInstanceDataState : UINT {
	EID_Visible		= 0,
	EID_Invisible	= 1,
	EID_Culled		= 1 << 1,
	EID_DrawAlways	= 1 << 2
};

struct InstanceData {
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
	float TimePos = 0.0f;
	UINT MaterialIndex = 0;
	UINT AnimClipIndex = 0;
	UINT State = EInstanceDataState::EID_Visible;
};

struct PassConstants {
	DirectX::XMFLOAT4X4 View = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvView = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 Proj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvViewProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 ViewProjTex = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 ShadowTransform = MathHelper::Identity4x4();
	DirectX::XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
	float cbPerObjectPad1 = 0.0f;
	DirectX::XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
	DirectX::XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };
	float NearZ = 0.0f;
	float FarZ = 0.0f;
	float TotalTime = 0.0f;
	float DeltaTime = 0.0f;

	DirectX::XMFLOAT4 AmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };

	// Indices [0, NUM_DIR_LIGHTS) are directional lights;
	// indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS) are point lights;
	// indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)
	// are spot lights for a maximum of MaxLights per object.
	Light Lights[MaxLights];
};

struct SsaoConstants {
	DirectX::XMFLOAT4X4 Proj;
	DirectX::XMFLOAT4X4 InvProj;
	DirectX::XMFLOAT4X4 ProjTex;
	DirectX::XMFLOAT4   OffsetVectors[14];

	// For SsaoBlur.hlsl
	DirectX::XMFLOAT4 BlurWeights[3];

	DirectX::XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };

	// Coordinates given in view space.
	float OcclusionRadius = 0.5f;
	float OcclusionFadeStart = 0.2f;
	float OcclusionFadeEnd = 2.0f;
	float SurfaceEpsilon = 0.05f;
};

struct MaterialData {
	DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
	float Roughness = 64.0f;

	// Used in texture mapping.
	DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();

	UINT DiffuseMapIndex;
	UINT NormalMapIndex;
	INT SpecularMapIndex;
	UINT MaterialPad0;
};

struct Vertex {
	DirectX::XMFLOAT3 Pos = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 Normal = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT2 TexC = { 0.0f, 0.0f };
	DirectX::XMFLOAT3 TangentU = { 0.0f, 0.0f, 0.0f };

	Vertex() = default;
	Vertex(DirectX::XMFLOAT3 pos, DirectX::XMFLOAT3 normal, DirectX::XMFLOAT2 texC, DirectX::XMFLOAT3 tangent);

	friend bool operator==(const Vertex& lhs, const Vertex& rhs);
};

struct SkinnedVertex {
	DirectX::XMFLOAT3 Pos = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 Normal = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT2 TexC = { 0.0f, 0.0f };
	DirectX::XMFLOAT3 TangentU = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT4 BoneWeights0 = { 0.0f, 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT4 BoneWeights1 = { 0.0f, 0.0f, 0.0f, 0.0f };
	int BoneIndices0[4] = { -1 };
	int BoneIndices1[4] = { -1 };

	SkinnedVertex() = default;
	SkinnedVertex(DirectX::XMFLOAT3 pos, DirectX::XMFLOAT3 normal, DirectX::XMFLOAT2 texC, DirectX::XMFLOAT3 tangent);

	friend bool operator==(const SkinnedVertex& lhs, const SkinnedVertex& rhs);
};

// Stores the resources needed for the CPU to build the command lists
// for a frame.  
struct FrameResource {
public:    
    FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT maxInstanceCount, UINT materialCount);
    virtual ~FrameResource() = default;

private:
	FrameResource(const FrameResource& src) = delete;
	FrameResource(FrameResource&& src) = delete;
	FrameResource& operator=(const FrameResource& rhs) = delete;
	FrameResource& operator=(FrameResource&& rhs) = delete;

public:
    // We cannot reset the allocator until the GPU is done processing the commands.
    // So each frame needs their own allocator.
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;

    // We cannot update a cbuffer until the GPU is done processing the commands
    // that reference it.  So each frame needs their own cbuffers.
    std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
    std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;
	std::unique_ptr<UploadBuffer<SsaoConstants>> SsaoCB = nullptr;
	std::unique_ptr<UploadBuffer<MaterialData>> MaterialBuffer = nullptr;

	// NOTE: In this demo, we instance only one render-item, so we only have one structured buffer to 
	// store instancing data.  To make this more general (i.e., to support instancing multiple render-items), 
	// you would need to have a structured buffer for each render-item, and allocate each buffer with enough
	// room for the maximum number of instances you would ever draw.  
	// This sounds like a lot, but it is actually no more than the amount of per-object constant data we 
	// would need if we were not using instancing.  For example, if we were drawing 1000 objects without instancing,
	// we would create a constant buffer with enough room for a 1000 objects.  With instancing, we would just
	// create a structured buffer large enough to store the instance data for 1000 instances.  
	std::unique_ptr<UploadBuffer<InstanceData>> InstanceBuffer = nullptr;

    // Fence value to mark commands up to this fence point.  This lets us
    // check if these frame resources are still in use by the GPU.
    UINT64 Fence = 0;
};