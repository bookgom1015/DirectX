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
}

void SkeletalMeshComponent::Update(const GameTimer& gt) {
	if (mClipIsChanged) {
		mLastTotalTime = gt.TotalTime();
		mClipIsChanged = false;
	}

	float timePose = mMesh->GetSkinnedData().GetTimePosition(mClipName, gt.TotalTime() - mLastTotalTime);
	mRenderer->UpdateInstanceAnimationData(mMeshName, mMesh->GetClipIndex(mClipName), timePose, true);
}

bool SkeletalMeshComponent::LoadSkeletalMesh(const std::string& inMeshName, const std::string& inFileName, 
										bool inNeedToBeAligned) {
	return ProcessLoadingMesh(inMeshName, inFileName, true, inNeedToBeAligned);
}

bool SkeletalMeshComponent::MTLoadSkeletalMesh(const std::string& inMeshName, const std::string& inFileName, 
										bool inNeedToBeAligned) {
	return MTProcessLoadingMesh(inMeshName, inFileName, true, inNeedToBeAligned);
}

void SkeletalMeshComponent::SetClipName(const std::string& inClipName) {
	if (inClipName == mClipName)
		return;

	mClipName = inClipName;
	mClipIsChanged = true;
}

void SkeletalMeshComponent::SetVisible(bool inState) {
	mRenderer->SetVisible(mMeshName, inState);
	mRenderer->SetSkeletonVisible(mMeshName, inState);
}

void SkeletalMeshComponent::SetSkeleletonVisible(bool inState) {
	mRenderer->SetSkeletonVisible(mMeshName, inState);
}