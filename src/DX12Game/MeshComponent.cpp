#include "DX12Game/MeshComponent.h"
#include "DX12Game/GameWorld.h"
#include "DX12Game/Renderer.h"
#include "DX12Game/Actor.h"
#include "DX12Game/Mesh.h"

using namespace DirectX;
using namespace DirectX::PackedVector;

MeshComponent::MeshComponent(Actor* inOwnerActor, int inUpdateOrder)
	: Component(inOwnerActor, inUpdateOrder),
	  mIsSkeletal(false) {
	mRenderer = GameWorld::GetWorld()->GetRenderer();
}

void MeshComponent::OnUpdateWorldTransform() {
	if (mOwner->GetIsDirty()) {
		XMVECTOR actorScale = GetOwner()->GetScale();
		XMVECTOR actorQuat = GetOwner()->GetQuaternion();
		XMVECTOR actorPos = GetOwner()->GetPosition();

		XMVECTOR finalScale = XMVectorMultiply(actorScale, XMLoadFloat3(&mScale));
		XMVECTOR finalQuat = XMQuaternionMultiply(actorQuat, XMLoadFloat4(&mQuaternion));
		XMVECTOR finalPos = XMVectorAdd(actorPos, XMLoadFloat3(&mPosition));

		XMVECTOR RotationOrigin = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
		XMMATRIX finalTransform = XMMatrixAffineTransformation(finalScale, RotationOrigin, finalQuat, finalPos);

		mRenderer->UpdateWorldTransform(mMeshName, finalTransform, mIsSkeletal);
		mOwner->SetActorClean();
	}
}

bool MeshComponent::LoadMesh(const std::string& inMeshName, const std::string& inFileName) {
	return ProcessMeshLoading(inMeshName, inFileName, false, false);
}

bool MeshComponent::MTLoadMesh(const std::string& inMeshName, const std::string& inFileName) {
	return MTProcessMeshLoading(inMeshName, inFileName, false, false);
}

bool MeshComponent::ProcessMeshLoading(const std::string& inMeshName, const std::string& inFileName, 
										bool inIsSkeletal, bool inNeedToBeAligned) {
	mMesh = GameWorld::GetWorld()->AddMesh(inFileName, inIsSkeletal, inNeedToBeAligned);
	if (mMesh == nullptr)
		return false;

	mMeshName = inMeshName;
	mRenderer->AddRenderItem(mMeshName, GetOwner()->GetWorldTransform(), mMesh);

	return true;
}

bool MeshComponent::MTProcessMeshLoading(const std::string& inMeshName, const std::string& inFileName,
											bool inIsSkeletal, bool inNeedToBeAligned) {
	mMesh = GameWorld::GetWorld()->MTAddMesh(inFileName, inIsSkeletal, inNeedToBeAligned);
	if (mMesh == nullptr)
		return false;

	mMeshName = inMeshName;
	mRenderer->AddRenderItem(mMeshName, GetOwner()->GetWorldTransform(), mMesh);

	return true;
}

void MeshComponent::SetVisible(bool inStatus) {
	mRenderer->SetVisible(mMeshName, inStatus);
}