#include "DX12Game/Component.h"
#include "DX12Game/Actor.h"

using namespace DirectX;
using namespace DirectX::PackedVector;

Component::Component(Actor* inOwnerActor, int inUpdateOrder)
	: mOwner(inOwnerActor),
	mUpdateOrder(inUpdateOrder) {
	mOwner->AddComponent(this);
}

Component::~Component() {}

void Component::Update(const GameTimer& gt) {}

void Component::ProcessInput(const InputState& input) {}

void Component::OnUpdateWorldTransform() {}

Actor* Component::GetOwner() const {
	return mOwner;
}

int Component::GetUpdateOrder() const {
	return mUpdateOrder;
}

void Component::SetScale(XMFLOAT3 inScale) {
	mScale = inScale;
}

void Component::SetScale(XMVECTOR inScale) {
	XMStoreFloat3(&mScale, inScale);
}

XMVECTOR Component::GetScale() const {
	return XMLoadFloat3(&mScale);
}

XMFLOAT3 Component::GetScale3f() const {
	return mScale;
}

void Component::SetQuaternion(XMFLOAT4 inQuaternion) {
	mQuaternion = inQuaternion;
}

void Component::SetQuaternion(XMVECTOR inQuaternion) {
	XMStoreFloat4(&mQuaternion, inQuaternion);
}

XMVECTOR Component::GetQuaternion() const {
	return XMLoadFloat4(&mQuaternion);
}

XMFLOAT4 Component::GetQuaternion4f() const {
	return mQuaternion;
}

void Component::SetPosition(XMFLOAT3 inPosition) {
	mPosition = inPosition;
}

void Component::SetPosition(XMVECTOR inPosition) {
	XMStoreFloat3(&mPosition, inPosition);
}

XMVECTOR Component::GetPosition() const {
	return XMLoadFloat3(&mPosition);
}

XMFLOAT3 Component::GetPosition3f() const {
	return mPosition;
}