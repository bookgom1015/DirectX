#include "DX12Game/SkeletalMeshComponent.h"
#include "DX12Game/GameWorld.h"
#include "DX12Game/Renderer.h"
#include "DX12Game/Actor.h"
#include "DX12Game/Mesh.h"

using namespace DirectX;

SkeletalMeshComponent::SkeletalMeshComponent(Actor* inOwnerActor)
	: MeshComponent(inOwnerActor, 100) {
	mIsSkeletal = true;
	mBoneTransforms.resize(gNumBones);
	for (size_t i = 0, size = mBoneTransforms.size(); i < size; ++i)
		mBoneTransforms[i] = MathHelper::Identity4x4();
	
	mLastTotalTime = 0.0f;
	mClipIsChanged = false;
}

void SkeletalMeshComponent::OnUpdateWorldTransform() {
	MeshComponent::OnUpdateWorldTransform();
}

void SkeletalMeshComponent::Update(const GameTimer& gt) {
	if (mClipIsChanged) {
		mLastTotalTime = gt.TotalTime();
		mClipIsChanged = false;
	}

	float timePos = mMesh->GetSkinnedData().GetTimePosition(mClipName, gt.TotalTime() - mLastTotalTime);
	mRenderer->UpdateInstanceAnimationData(mMeshName, mMesh->GetClipIndex(mClipName), timePos, true);
}

GameResult SkeletalMeshComponent::LoadMesh(const std::string& inMeshName, const std::string& inFileName) {
	CheckGameResult(GameWorld::GetWorld()->AddMesh(inFileName, mMesh, true, false));

	mMeshName = inMeshName;
	mRenderer->AddRenderItem(mMeshName, mMesh);

	return GameResultOk;
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