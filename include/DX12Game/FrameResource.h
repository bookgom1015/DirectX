#pragma once

const size_t gNumBones = 512;

#include "DX12Game/FrameResource.h"
#include "common/UploadBuffer.h"

enum EInstanceDataState : UINT {
	EID_Visible = 0,
	EID_Invisible = 1,
	EID_Culled = 1 << 1,
	EID_DrawAlways = 1 << 2
};

struct ObjectConstants {
public:
	UINT InstanceIndex;
	UINT ObjectPad0;
	UINT ObjectPad1;
	UINT ObjectPad2;

public:
	ObjectConstants();
};

struct InstanceData {
public:
	DirectX::XMFLOAT4X4 World;
	DirectX::XMFLOAT4X4 TexTransform;
	float TimePos;
	UINT MaterialIndex;
	UINT AnimClipIndex;
	UINT State;

public:
	InstanceData();
	InstanceData(const DirectX::XMFLOAT4X4& inWorld, const DirectX::XMFLOAT4X4& inTexTransform,
		float inTimePos, UINT inMaterialIndex, UINT inAnimClipIndex);
};

struct PassConstants {
public:
	DirectX::XMFLOAT4X4 View;
	DirectX::XMFLOAT4X4 InvView;
	DirectX::XMFLOAT4X4 Proj;
	DirectX::XMFLOAT4X4 InvProj;
	DirectX::XMFLOAT4X4 ViewProj;
	DirectX::XMFLOAT4X4 InvViewProj;
	DirectX::XMFLOAT4X4 ViewProjTex;
	DirectX::XMFLOAT4X4 ShadowTransform;
	DirectX::XMFLOAT3 EyePosW;
	float cbPerObjectPad1;
	DirectX::XMFLOAT2 RenderTargetSize;
	DirectX::XMFLOAT2 InvRenderTargetSize;
	float NearZ;
	float FarZ;
	float TotalTime;
	float DeltaTime;

	DirectX::XMFLOAT4 AmbientLight;

	// Indices [0, NUM_DIR_LIGHTS) are directional lights;
	// indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS) are point lights;
	// indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)
	// are spot lights for a maximum of MaxLights per object.
	Light Lights[MaxLights];

public:
	PassConstants();
};

struct SsaoConstants {
	DirectX::XMFLOAT4X4 Proj;
	DirectX::XMFLOAT4X4 InvProj;
	DirectX::XMFLOAT4X4 ProjTex;
	DirectX::XMFLOAT4   OffsetVectors[14];

	// For SsaoBlur.hlsl
	DirectX::XMFLOAT4 BlurWeights[3];

	DirectX::XMFLOAT2 InvRenderTargetSize;

	// Coordinates given in view space.
	float OcclusionRadius;
	float OcclusionFadeStart;
	float OcclusionFadeEnd;
	float SurfaceEpsilon;

public:
	SsaoConstants();
};

struct MaterialData {
	DirectX::XMFLOAT4 DiffuseAlbedo;
	DirectX::XMFLOAT3 FresnelR0;
	float Roughness;

	// Used in texture mapping.
	DirectX::XMFLOAT4X4 MatTransform;

	UINT DiffuseMapIndex;
	UINT NormalMapIndex;
	INT SpecularMapIndex;
	UINT MaterialPad0;

public:
	MaterialData();
};

struct Vertex {
public:
	DirectX::XMFLOAT3 Pos;
	DirectX::XMFLOAT3 Normal;
	DirectX::XMFLOAT2 TexC;
	DirectX::XMFLOAT3 TangentU;

public:
	Vertex();
	Vertex(DirectX::XMFLOAT3 inPos, DirectX::XMFLOAT3 inNormal, DirectX::XMFLOAT2 inTexC, DirectX::XMFLOAT3 inTangent);

public:
	friend bool operator==(const Vertex& lhs, const Vertex& rhs);
};

struct SkinnedVertex {
public:
	DirectX::XMFLOAT3 Pos;
	DirectX::XMFLOAT3 Normal;
	DirectX::XMFLOAT2 TexC;
	DirectX::XMFLOAT3 TangentU;
	DirectX::XMFLOAT4 BoneWeights0;
	DirectX::XMFLOAT4 BoneWeights1;
	int BoneIndices0[4];
	int BoneIndices1[4];

public:
	SkinnedVertex();
	SkinnedVertex(DirectX::XMFLOAT3 inPos, DirectX::XMFLOAT3 inNormal, 
		DirectX::XMFLOAT2 inTexC, DirectX::XMFLOAT3 inTangent);

public:
	friend bool operator==(const SkinnedVertex& lhs, const SkinnedVertex& rhs);
};

// Stores the resources needed for the CPU to build the command lists
// for a frame.  
struct FrameResource {
public:    
    FrameResource(ID3D12Device* inDevice, UINT inPassCount, 
		UINT inObjectCount, UINT inMaxInstanceCount, UINT inMaterialCount);
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