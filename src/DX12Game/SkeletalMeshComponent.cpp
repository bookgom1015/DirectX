#include "DX12Game/SkeletalMeshComponent.h"
#include "DX12Game/GameWorld.h"
#include "DX12Game/Renderer.h"
#include "DX12Game/Actor.h"
#include "DX12Game/Mesh.h"
#include "DX12Game/SkinnedData.h"

using namespace DirectX;

SkeletalMeshComponent::SkeletalMeshComponent(Actor* inOwnerActor)
	: MeshComponent(inOwnerActor, 100) {
	mIsSkeletal = true;
	mBoneTransforms.resize(gNumBones);
	for (size_t i = 0, size = mBoneTransforms.size(); i < size; ++i)
		mBoneTransforms[i] = MathHelper::Identity4x4();
}

void SkeletalMeshComponent::OnUpdateWorldTransform() {
	MeshComponent::OnUpdateWorldTransform();

	mRenderer->UpdateSkinnedTransforms(mMeshName, mBoneTransforms);
}

void SkeletalMeshComponent::Update(const GameTimer& gt) {
	if (mClipIsChanged) {
		mLastTotalTime = gt.TotalTime();
		mClipIsChanged = false;
	}

	mMesh->GetSkinnedData().GetFinalTransforms(mClipName, gt.TotalTime() - mLastTotalTime, mBoneTransforms);
}

bool SkeletalMeshComponent::LoadSkeletalMesh(const std::string& inMeshName, const std::string& inFileName, 
										bool inNeedToBeAligned) {
	return ProcessMeshLoading(inMeshName, inFileName, true, inNeedToBeAligned);
}

bool SkeletalMeshComponent::MTLoadSkeletalMesh(const std::string& inMeshName, const std::string& inFileName, 
										bool inNeedToBeAligned) {
	return MTProcessMeshLoading(inMeshName, inFileName, true, inNeedToBeAligned);
}

void SkeletalMeshComponent::SetClipName(const std::string& inClipName) {
	if (inClipName == mClipName)
		return;

	mClipName = inClipName;
	mClipIsChanged = true;
}

void SkeletalMeshComponent::SetVisible(bool inStatus) {
	mRenderer->SetVisible(mMeshName, inStatus);
	mRenderer->SetSkeletonVisible(mMeshName, inStatus);
}

void SkeletalMeshComponent::SetSkeleletonVisible(bool inStatus) {
	mRenderer->SetSkeletonVisible(mMeshName, inStatus);
}