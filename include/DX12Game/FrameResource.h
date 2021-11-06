#pragma once

#include "DX12Game/GameUploadBuffer.h"

enum EInstanceRenderState : UINT {
	EID_Visible		= 1,
	EID_DrawAlways	= 1 << 1
};

struct ObjectConstants {
public:
	UINT mObjectIndex;
	UINT mObjectPad0;
	UINT mObjectPad1;
	UINT mObjectPad2;

public:
	ObjectConstants(UINT inObjectIndex = 0);
};

struct InstanceIdxData {
public:
	UINT mInstanceIdx;
	UINT mInstIdxPad0;
	UINT mInstIdxPad1;
	UINT mInstIdxPad2;

public:
	InstanceIdxData(UINT inIdx = 0);
};

struct InstanceData {
public:
	DirectX::XMFLOAT4X4 mWorld;
	DirectX::XMFLOAT4X4 mTexTransform;
	float mTimePos;
	UINT mMaterialIndex;
	int mAnimClipIndex;	
	UINT mRenderState;

public:
	InstanceData(
		const DirectX::XMFLOAT4X4& inWorld			= MathHelper::Identity4x4(),
		const DirectX::XMFLOAT4X4& inTexTransform	= MathHelper::Identity4x4(),
		float inTimePos								= 0.0f, 
		UINT inMaterialIndex						= 0, 
		int inAnimClipIndex							= -1, 
		EInstanceRenderState inRenderState			= EInstanceRenderState::EID_Visible
	);

public:
	static void SetRenderState(UINT& inRenderState, EInstanceRenderState inTargetState);
	static void UnsetRenderState(UINT& inRenderState, EInstanceRenderState inTargetState);

	static bool IsMatched(UINT inRenderState, EInstanceRenderState inTargetState);
	static bool IsUnmatched(UINT inRenderState, EInstanceRenderState inTargetState);

	bool CheckFrameDirty(UINT inIndex) const;
	void SetFramesDirty(UINT inNum);
	void UnsetFrameDirty(UINT inIndex);
};

struct PassConstants {
public:
	DirectX::XMFLOAT4X4 mView;
	DirectX::XMFLOAT4X4 mInvView;
	DirectX::XMFLOAT4X4 mProj;
	DirectX::XMFLOAT4X4 mInvProj;
	DirectX::XMFLOAT4X4 mViewProj;
	DirectX::XMFLOAT4X4 mInvViewProj;
	DirectX::XMFLOAT4X4 mViewProjTex;
	DirectX::XMFLOAT4X4 mShadowTransform;
	DirectX::XMFLOAT3 mEyePosW;
	float mCBPerObjectPad1;
	DirectX::XMFLOAT2 mRenderTargetSize;
	DirectX::XMFLOAT2 mInvRenderTargetSize;
	float mNearZ;
	float mFarZ;
	float mTotalTime;
	float mDeltaTime;

	DirectX::XMFLOAT4 mAmbientLight;

	// Indices [0, NUM_DIR_LIGHTS) are directional lights;
	// indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS) are point lights;
	// indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)
	// are spot lights for a maximum of MaxLights per object.
	Light mLights[MaxLights];

public:
	PassConstants();
};

struct SsaoConstants {
	DirectX::XMFLOAT4X4 mView;
	DirectX::XMFLOAT4X4 mProj;
	DirectX::XMFLOAT4X4 mInvProj;
	DirectX::XMFLOAT4X4 mProjTex;
	DirectX::XMFLOAT4   mOffsetVectors[14];
	// For SsaoBlur.hlsl
	DirectX::XMFLOAT4 mBlurWeights[3];
	DirectX::XMFLOAT2 mInvRenderTargetSize;
	// Coordinates given in view space.
	float mOcclusionRadius;
	float mOcclusionFadeStart;
	float mOcclusionFadeEnd;
	float mSurfaceEpsilon;

public:
	SsaoConstants();
};

struct SsrConstants {
	DirectX::XMFLOAT4X4	mProj;
	DirectX::XMFLOAT4X4	mInvProj;
	DirectX::XMFLOAT4X4	mViewProj;
	// For SsrBlur.hlsl
	DirectX::XMFLOAT4 mBlurWeights[3];
	DirectX::XMFLOAT2 mInvRenderTargetSize;
};

struct MaterialData {
	DirectX::XMFLOAT4 mDiffuseAlbedo;
	DirectX::XMFLOAT3 mFresnelR0;
	float mRoughness;

	// Used in texture mapping.
	DirectX::XMFLOAT4X4 mMatTransform;

	UINT mDiffuseMapIndex;
	UINT mNormalMapIndex;
	INT mSpecularMapIndex;
	INT mDispMapIndex;
	INT mAlphaMapIndex;
	UINT mMatPad0;
	UINT mMatPad1;
	UINT mMatPad2;

public:
	MaterialData();
};

struct Vertex {
public:
	DirectX::XMFLOAT3 mPos;
	DirectX::XMFLOAT3 mNormal;
	DirectX::XMFLOAT2 mTexC;
	DirectX::XMFLOAT3 mTangentU;

public:
	Vertex(
		DirectX::XMFLOAT3 inPos		= { 0.0f, 0.0f, 0.0f },
		DirectX::XMFLOAT3 inNormal	= { 0.0f, 0.0f, 0.0f },
		DirectX::XMFLOAT2 inTexC	= { 0.0f, 0.0f },
		DirectX::XMFLOAT3 inTangent	= { 0.0f, 0.0f, 0.0f }
	);

public:
	friend bool operator==(const Vertex& lhs, const Vertex& rhs);
};

struct SkinnedVertex {
public:
	DirectX::XMFLOAT3 mPos;
	DirectX::XMFLOAT3 mNormal;
	DirectX::XMFLOAT2 mTexC;
	DirectX::XMFLOAT3 mTangentU;
	DirectX::XMFLOAT4 mBoneWeights0;
	DirectX::XMFLOAT4 mBoneWeights1;
	int mBoneIndices0[4];
	int mBoneIndices1[4];

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
	GameResult Initialize(UINT inNumThreads = 1);

public:
    // We cannot reset the allocator until the GPU is done processing the commands.
    // So each frame needs their own allocator.
	std::vector<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>> mCmdListAllocs;

    // We cannot update a cbuffer until the GPU is done processing the commands
    // that reference it.  So each frame needs their own cbuffers.
	GameUploadBuffer<PassConstants> mPassCB;
	GameUploadBuffer<ObjectConstants> mObjectCB;
	GameUploadBuffer<SsaoConstants> mSsaoCB;
	GameUploadBuffer<SsrConstants> mSsrCB;
	GameUploadBuffer<MaterialData> mMaterialBuffer;
    //std::unique_ptr<UploadBuffer<PassConstants>> mPassCB = nullptr;
    //std::unique_ptr<UploadBuffer<ObjectConstants>> mObjectCB = nullptr;
	//std::unique_ptr<UploadBuffer<SsaoConstants>> mSsaoCB = nullptr;
	//std::unique_ptr<UploadBuffer<MaterialData>> mMaterialBuffer = nullptr;

	GameUploadBuffer<InstanceIdxData> mInstanceIdxBuffer;
	//std::unique_ptr<UploadBuffer<InstanceIdxData>> mInstanceIdxBuffer = nullptr;
	// NOTE: In this demo, we instance only one render-item, so we only have one structured buffer to 
	// store instancing data.  To make this more general (i.e., to support instancing multiple render-items), 
	// you would need to have a structured buffer for each render-item, and allocate each buffer with enough
	// room for the maximum number of instances you would ever draw.  
	// This sounds like a lot, but it is actually no more than the amount of per-object constant data we 
	// would need if we were not using instancing.  For example, if we were drawing 1000 objects without instancing,
	// we would create a constant buffer with enough room for a 1000 objects.  With instancing, we would just
	// create a structured buffer large enough to store the instance data for 1000 instances.  
	GameUploadBuffer<InstanceData> mInstanceDataBuffer;
	//std::unique_ptr<UploadBuffer<InstanceData>> mInstanceBuffer = nullptr;

    // Fence value to mark commands up to this fence point.  This lets us
    // check if these frame resources are still in use by the GPU.
    UINT64 mFence = 0;

	ID3D12Device* mDevice;
	UINT mPassCount;
	UINT mObjectCount;
	UINT mMaxInstanceCount;
	UINT mMaterialCount;
};