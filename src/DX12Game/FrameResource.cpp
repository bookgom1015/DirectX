#include "DX12Game/GameCore.h"
#include "DX12Game/FrameResource.h"

using namespace DirectX;

ObjectConstants::ObjectConstants(UINT inObjectIndex /* = 0 */) {
	mObjectIndex = inObjectIndex;
	mObjectPad0 = 0;
	mObjectPad1 = 0;
	mObjectPad2 = 0;
}

InstanceIdxData::InstanceIdxData(UINT inIdx /* = 0 */) {
	mInstanceIdx = inIdx;
	mInstIdxPad0 = 0;
	mInstIdxPad1 = 0;
	mInstIdxPad2 = 0;
}

InstanceData::InstanceData(
	const DirectX::XMFLOAT4X4& inWorld			/* = MathHelper::Identity4x4() */,
	const DirectX::XMFLOAT4X4& inTexTransform	/* = MathHelper::Identity4x4() */,
	float inTimePos								/* = 0.0f */,
	UINT inMaterialIndex						/* = 0 */, 
	int inAnimClipIndex							/* = -1 */,
	EInstanceRenderState inRenderState /* = EInstanceRenderState::EID_Visible */) {

	mWorld = inWorld;
	mTexTransform = inTexTransform;
	mTimePos = inTimePos;
	mMaterialIndex = inMaterialIndex;
	mAnimClipIndex = inAnimClipIndex;
	mRenderState = inRenderState;

	SetFramesDirty(gNumFrameResources);
}

void InstanceData::SetRenderState(UINT& inRenderState, EInstanceRenderState inTargetState) {
	inRenderState |= inTargetState;
}

void InstanceData::UnsetRenderState(UINT& inRenderState, EInstanceRenderState inTargetState) {
	inRenderState &= ~inTargetState;
}

bool InstanceData::IsMatched(UINT inRenderState, EInstanceRenderState inTargetState) {
	return (inRenderState & inTargetState) != 0;
}

bool InstanceData::IsUnmatched(UINT inRenderState, EInstanceRenderState inTargetState) {
	return (inRenderState & inTargetState) == 0;
}

namespace {
	const UINT BitShift = 16;
	const UINT DecreaseOne = (1 << BitShift);
}

bool InstanceData::CheckFrameDirty(UINT inIndex) const {
	return ((mRenderState >> BitShift) & (1 << inIndex)) != 0;
}

void InstanceData::SetFramesDirty(UINT inNum) {
	UINT num = 1;

	for (UINT i = 1; i < inNum; ++i)
		num |= 1 << i;

	mRenderState |= (num << BitShift);
}

void InstanceData::UnsetFrameDirty(UINT inIndex) {
	mRenderState &= ~((1 << inIndex) << BitShift);
}

PassConstants::PassConstants() {
	mView = MathHelper::Identity4x4();
	mInvView = MathHelper::Identity4x4();
	mProj = MathHelper::Identity4x4();
	mInvProj = MathHelper::Identity4x4();
	mViewProj = MathHelper::Identity4x4();
	mInvViewProj = MathHelper::Identity4x4();
	mViewProjTex = MathHelper::Identity4x4();
	mShadowTransform = MathHelper::Identity4x4();
	mEyePosW = { 0.0f, 0.0f, 0.0f };
	mCBPerObjectPad1 = 0.0f;
	mRenderTargetSize = { 0.0f, 0.0f };
	mInvRenderTargetSize = { 0.0f, 0.0f };
	mNearZ = 0.0f;
	mFarZ = 0.0f;
	mTotalTime = 0.0f;
	mDeltaTime = 0.0f;

	mAmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };
}

SsaoConstants::SsaoConstants() {
	mInvRenderTargetSize = { 0.0f, 0.0f };

	mOcclusionRadius = 0.5f;
	mOcclusionFadeStart = 0.2f;
	mOcclusionFadeEnd = 2.0f;
	mSurfaceEpsilon = 0.05f;
}

MaterialData::MaterialData() {
	mDiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	mFresnelR0 = { 0.01f, 0.01f, 0.01f };
	mRoughness = 64.0f;

	mMatTransform = MathHelper::Identity4x4();

	mDiffuseMapIndex = 0;
	mNormalMapIndex = 0;
	mSpecularMapIndex = -1;
	mDispMapIndex = -1;
}

Vertex::Vertex(
	XMFLOAT3 inPos		/* = { 0.0f, 0.0f, 0.0f } */,
	XMFLOAT3 inNormal	/* = { 0.0f, 0.0f, 0.0f } */, 
	XMFLOAT2 inTexC		/* = { 0.0f, 0.0f } */, 
	XMFLOAT3 inTangent	/* = { 0.0f, 0.0f, 0.0f } */) {

	mPos = inPos;
	mNormal = inNormal;
	mTexC = inTexC;
	mTangentU = inTangent;
}

bool operator==(const Vertex& lhs, const Vertex& rhs) {
	return MathHelper::IsEqual(lhs.mPos,		rhs.mPos)		&&
		   MathHelper::IsEqual(lhs.mNormal,		rhs.mNormal)	&&
		   MathHelper::IsEqual(lhs.mTexC,		rhs.mTexC)		&&
		   MathHelper::IsEqual(lhs.mTangentU,	rhs.mTangentU);
}

SkinnedVertex::SkinnedVertex() {
	mPos = { 0.0f, 0.0f, 0.0f };
	mNormal = { 0.0f, 0.0f, 0.0f };
	mTexC = { 0.0f, 0.0f };
	mTangentU = { 0.0f, 0.0f, 0.0f };
	mBoneWeights0 = { 0.0f, 0.0f, 0.0f, 0.0f };
	mBoneWeights1 = { 0.0f, 0.0f, 0.0f, 0.0f };
	for (auto& index : mBoneIndices0)
		index = -1;
	for (auto& index : mBoneIndices1)
		index = -1;
}

SkinnedVertex::SkinnedVertex(XMFLOAT3 inPos, XMFLOAT3 inNormal, XMFLOAT2 inTexC, XMFLOAT3 inTangent) {
	mPos = inPos;
	mNormal = inNormal;
	mTexC = inTexC;
	mTangentU = inTangent;
}

bool operator==(const SkinnedVertex& lhs, const SkinnedVertex& rhs) {
	bool bResult = MathHelper::IsEqual(lhs.mPos,			rhs.mPos)			&&
				   MathHelper::IsEqual(lhs.mNormal,			rhs.mNormal)		&&
				   MathHelper::IsEqual(lhs.mTexC,			rhs.mTexC)			&&
				   MathHelper::IsEqual(lhs.mTangentU,		rhs.mTangentU)		&&
				   MathHelper::IsEqual(lhs.mBoneWeights0,	rhs.mBoneWeights0)	&&
				   MathHelper::IsEqual(lhs.mBoneWeights1,	rhs.mBoneWeights1);
	if (!bResult)
		return false;

	for (size_t i = 0; i < 4; ++i) {
		if (lhs.mBoneIndices0[i] != rhs.mBoneIndices0[i] || lhs.mBoneIndices1[i] != rhs.mBoneIndices1[i])
			return false;
	}

	return true;
}

FrameResource::FrameResource(ID3D12Device* inDevice, 
	UINT inPassCount, UINT inObjectCount, UINT inMaxInstanceCount, UINT inMaterialCount) 
	: mPassCount(inPassCount), 
	  mObjectCount(inObjectCount), 
	  mMaxInstanceCount(inMaxInstanceCount), 
	  mMaterialCount(inMaterialCount) {

	mDevice = inDevice;
}

GameResult FrameResource::Initialize(UINT inNumThreads) {
	mCmdListAllocs.resize(inNumThreads);

	for (UINT i = 0; i < inNumThreads; ++i) {
		ReturnIfFailed(
			mDevice->CreateCommandAllocator(
				D3D12_COMMAND_LIST_TYPE_DIRECT,
				IID_PPV_ARGS(mCmdListAllocs[i].GetAddressOf())
			)
		);
	}

	mPassCB.Initialize(mDevice, mPassCount, true);
	mObjectCB.Initialize(mDevice, mObjectCount, true);
	mSsaoCB.Initialize(mDevice, 1, true);
	mMaterialBuffer.Initialize(mDevice, mMaterialCount, false);
	mInstanceIdxBuffer.Initialize(mDevice, mObjectCount * mMaxInstanceCount, false);
	mInstanceDataBuffer.Initialize(mDevice, mObjectCount * mMaxInstanceCount, false);

	return GameResult(S_OK);
}