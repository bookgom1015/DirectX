#include "DX12Game/Actor.h"
#include "DX12Game/GameWorld.h"
#include "DX12Game/InputSystem.h"
#include "DX12Game/Component.h"

using namespace DirectX;
using namespace DirectX::PackedVector;

Actor::Actor() {
	GameWorld::GetWorld()->AddActor(this);
}

Actor::~Actor() {
	while (!mComponents.empty())
		mComponents.pop_back();

	GameWorld::GetWorld()->RemoveActor(this);
}

void Actor::Update(const GameTimer& gt) {
	if (mState == ActorState::EActive) {
		ComputeWorldTransform();

		UpdateComponents(gt);
		UpdateActor(gt);

		ComputeWorldTransform();
	}
}

void Actor::UpdateComponents(const GameTimer& gt) {
	for (auto comp : mComponents)
		comp->Update(gt);
}

void Actor::ProcessInput(const InputState& input) {
	if (mState == ActorState::EActive) {
		for (auto comp : mComponents)
			comp->ProcessInput(input);

		ProcessActorInput(input);
	}
}

GameResult Actor::OnLoadingData() {
	return GameResultOk;
}

void Actor::OnUnloadingData() {}

void Actor::AddComponent(Component* inComponent) {
	int myOrder = inComponent->GetUpdateOrder();
	auto iter = mComponents.cbegin();
	for (; iter != mComponents.cend(); ++iter) {
		if (myOrder < (*iter)->GetUpdateOrder())
			break;
	}

	mComponents.insert(iter, inComponent);
}

void Actor::RemoveComponent(Component* inComponent) {
	auto iter = std::find(mComponents.cbegin(), mComponents.cend(), inComponent);
	if (iter != mComponents.cend())
		mComponents.erase(iter);
}

void Actor::ComputeWorldTransform() {
	if (bRecomputeWorldTransform) {
		bRecomputeWorldTransform = false;

		XMVECTOR S = XMLoadFloat3(&mScale);
		XMVECTOR P = XMLoadFloat3(&mPosition);
		XMVECTOR Q = XMLoadFloat4(&mQuaternion);

		XMVECTOR RotationOrigin = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
		XMStoreFloat4x4(&mWorldTransform, XMMatrixAffineTransformation(S, RotationOrigin, Q, P));
	}

	for (auto comp : mComponents)
		comp->OnUpdateWorldTransform();
}

void Actor::UpdateActor(const GameTimer& gt) {
	for (const auto& func : mFunctions)
		(*func)(gt, this);
}

void Actor::ProcessActorInput(const InputState& input) {}

void Actor::AddFunction(std::shared_ptr<std::function<void(const GameTimer&, Actor*)>> inFunction) {
	auto iter = std::find(mFunctions.begin(), mFunctions.end(), inFunction);
	if (iter == mFunctions.end())
		mFunctions.push_back(inFunction);
}

void Actor::RemoveFunction(std::shared_ptr<std::function<void(const GameTimer&, Actor*)>> inFunction) {
	auto iter = std::find(mFunctions.begin(), mFunctions.end(), inFunction);
	if (iter != mFunctions.end()) {
		std::iter_swap(iter, mFunctions.end() - 1);
		mFunctions.pop_back();
	}
}

Actor::ActorState Actor::GetState() const {
	return mState;
}

void Actor::SetState(Actor::ActorState inState) {
	mState = inState;
}

XMMATRIX Actor::GetWorldTransform() const {
	return XMLoadFloat4x4(&mWorldTransform);
}

const XMFLOAT4X4& Actor::GetWorldTransform4x4f() const {
	return mWorldTransform;
}

XMVECTOR Actor::GetScale() const {
	return XMLoadFloat3(&mScale);
}

const XMFLOAT3& Actor::GetScale3f() const {
	return mScale;
}

void Actor::SetScale(float inX, float inY, float inZ) {
	mScale = XMFLOAT3(inX, inY, inZ);

	bRecomputeWorldTransform = true;
	mIsDirty = true;
}

void Actor::SetScale(const XMFLOAT3& inScale) {
	mScale = inScale;

	bRecomputeWorldTransform = true;
	mIsDirty = true;
}

void Actor::SetScale(const XMVECTOR& inScale) {
	XMStoreFloat3(&mScale, inScale);

	bRecomputeWorldTransform = true;
	mIsDirty = true;
}

XMVECTOR Actor::GetQuaternion() const {
	return XMLoadFloat4(&mQuaternion);
}

const XMFLOAT4& Actor::GetQuaternion4f() const {
	return mQuaternion;
}

void Actor::SetQuaternion(const XMFLOAT4& inQuat) {
	mQuaternion = inQuat;

	bRecomputeWorldTransform = true;
	mIsDirty = true;
}

void Actor::SetQuaternion(const XMVECTOR& inQuat) {
	XMStoreFloat4(&mQuaternion, inQuat);

	bRecomputeWorldTransform = true;
	mIsDirty = true;
}

void Actor::SetPosition(float inX, float inY, float inZ) {
	mPosition = XMFLOAT3(inX, inY, inZ);

	bRecomputeWorldTransform = true;
	mIsDirty = true;
}

XMVECTOR Actor::GetPosition() const {
	return XMLoadFloat3(&mPosition);
}

const XMFLOAT3& Actor::GetPosition3f() const {
	return mPosition;
}

void Actor::SetPosition(const XMFLOAT3& inPos) {
	mPosition = inPos;

	bRecomputeWorldTransform = true;
	mIsDirty = true;
}

void Actor::SetPosition(const XMVECTOR& inPos) {
	XMStoreFloat3(&mPosition, inPos);

	bRecomputeWorldTransform = true;
	mIsDirty = true;
}

bool Actor::GetIsDirty() const {
	return mIsDirty;
}

void Actor::SetActorClean() {
	mIsDirty = false;
}

UINT Actor::GetOwnerThreadID() const {
	return mOwnerId;
}

void Actor::SetOwnerThreadID(UINT inId) {
	mOwnerId = inId;
}