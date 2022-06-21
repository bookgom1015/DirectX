#include "DX12Game/CameraComponent.h"
#include "DX12Game/Actor.h"
#include "DX12Game/GameCamera.h"

using namespace DirectX;

CameraComponent::CameraComponent(Actor* inActor, int inUpdateOrder)
	: Component(inActor, inUpdateOrder) {
	mCamera = std::make_unique<GameCamera>();

	SetLens(XM_PIDIV4, 1.0f, 0.1f, 1000.0f);
	SetPosition(GetOwner()->GetPosition3f());
}

void CameraComponent::Update(const GameTimer& gt) {
	mCamera->SetRotationUsingQuaternion(GetOwner()->GetQuaternion());
}

void CameraComponent::OnUpdateWorldTransform() {
	UpdateViewMatrix();
}

void CameraComponent::Pitch(float inAngle) {
	mCamera->Pitch(inAngle);
}

void CameraComponent::RotateY(float inAngle) {
	mCamera->RotateY(inAngle);
}

void CameraComponent::Roll(float inAngle) {
	mCamera->Roll(inAngle);
}

void CameraComponent::UpdateViewMatrix() {
	mCamera->UpdateViewMatrix();
}

void CameraComponent::LookAt(const FXMVECTOR& inPos, const FXMVECTOR& inTarget, const FXMVECTOR& inWorldUp) {
	mCamera->LookAt(inPos, inTarget, inWorldUp);
}

void CameraComponent::LookAt(const XMFLOAT3& inPos, const XMFLOAT3& inTarget, const XMFLOAT3& inUp) {
	mCamera->LookAt(inPos, inTarget, inUp);
}

XMVECTOR CameraComponent::GetPosition() const {
	return mCamera->GetPosition();
}

XMFLOAT3 CameraComponent::GetPosition3f() const {
	return mCamera->GetPosition3f();
}

void CameraComponent::SetPosition(float inX, float inY, float inZ) {
	mCamera->SetPosition(inX, inY, inZ);
}

void CameraComponent::SetPosition(const XMVECTOR& inV) {
	mCamera->SetPosition(inV);
}

void CameraComponent::SetPosition(const XMFLOAT3& inV) {
	mCamera->SetPosition(inV);
}

XMVECTOR CameraComponent::GetRight() const {
	return mCamera->GetRight();
}

XMFLOAT3 CameraComponent::GetRight3f() const {
	return mCamera->GetRight3f();
}

XMVECTOR CameraComponent::GetUp() const {
	return mCamera->GetUp();
}

XMFLOAT3 CameraComponent::GetUp3f() const {
	return mCamera->GetUp3f();
}

XMVECTOR CameraComponent::GetLook() const {
	return mCamera->GetLook();
}

XMFLOAT3 CameraComponent::GetLook3f() const {
	return mCamera->GetLook3f();
}

float CameraComponent::GetNearZ() const {
	return mCamera->GetNearZ();
}

float CameraComponent::GetFarZ() const {
	return mCamera->GetFarZ();
}

float CameraComponent::GetAspect() const {
	return mCamera->GetAspect();
}

float CameraComponent::GetFovY() const {
	return mCamera->GetFovY();
}

float CameraComponent::GetFovX() const {
	return mCamera->GetFovX();
}

float CameraComponent::GetNearWindowWidth() const {
	return mCamera->GetNearWindowWidth();
}

float CameraComponent::GetNearWindowHeight() const {
	return mCamera->GetNearWindowHeight();
}

float CameraComponent::GetFarWindowWidth() const {
	return mCamera->GetFarWindowWidth();
}

float CameraComponent::GetFarWindowHeight() const {
	return mCamera->GetFarWindowHeight();
}

XMMATRIX CameraComponent::GetView() const {
	return mCamera->GetView();
}

XMMATRIX CameraComponent::GetProj() const {
	return mCamera->GetProj();
}

XMFLOAT4X4 CameraComponent::GetView4x4f() const {
	return mCamera->GetView4x4f();
}

XMFLOAT4X4 CameraComponent::GetProj4x4f() const {
	return mCamera->GetProj4x4f();
}

GameCamera* CameraComponent::GetCamera() const {
	return mCamera.get();
}

void CameraComponent::SetInheritPitch(bool inState) {
	mCamera->SetInheritPitch(inState);
}

void CameraComponent::SetInheritYaw(bool inState) {
	mCamera->SetInheritYaw(inState);
}

void CameraComponent::SetInheritRoll(bool inState) {
	mCamera->SetInheritRoll(inState);
}

void CameraComponent::SetLens(float inFovY, float inAspect, float inZnear, float inZfar) {
	mCamera->SetLens(inFovY, inAspect, inZnear, inZfar);
}