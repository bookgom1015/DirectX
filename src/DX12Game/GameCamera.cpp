#include "DX12Game/GameCamera.h"

using namespace DirectX;
using namespace DirectX::PackedVector;

GameCamera::GameCamera() {
	mPosition = { 0.0f, 0.0f, 0.0f };
	mRight = { 1.0f, 0.0f, 0.0f };
	mUp = { 0.0f, 1.0f, 0.0f };
	mLook = { 0.0f, 0.0f, 1.0f };

	mNearZ = 0.0f;
	mFarZ = 0.0f;
	mAspect = 0.0f;
	mFovY = 0.0f;
	mNearWindowHeight = 0.0f;
	mFarWindowHeight = 0.0f;

	bool mViewDirty = true;

	mView = MathHelper::Identity4x4();
	mProj = MathHelper::Identity4x4();

	bInheritPitch = false;
	bInheritYaw = false;
	bInheritRoll = false;

	SetLens(0.5f * MathHelper::Pi, 1.0f, 1.0f, 1000.0f);
}

void GameCamera::Pitch(float inAngle) {
	// Rotate up and look vector about the right vector.

	XMMATRIX R = XMMatrixRotationAxis(XMLoadFloat3(&mRight), inAngle);

	XMStoreFloat3(&mUp, XMVector3TransformNormal(XMLoadFloat3(&mUp), R));
	XMStoreFloat3(&mLook, XMVector3TransformNormal(XMLoadFloat3(&mLook), R));

	mViewDirty = true;
}

void GameCamera::RotateY(float inAngle) {
	// Rotate the basis vectors about the world y-axis.

	XMMATRIX R = XMMatrixRotationY(inAngle);

	XMStoreFloat3(&mRight, XMVector3TransformNormal(XMLoadFloat3(&mRight), R));
	XMStoreFloat3(&mUp, XMVector3TransformNormal(XMLoadFloat3(&mUp), R));
	XMStoreFloat3(&mLook, XMVector3TransformNormal(XMLoadFloat3(&mLook), R));

	mViewDirty = true;
}

void GameCamera::Roll(float inAngle) {
	// Rotate up and right vector about the look vector.

	XMMATRIX R = XMMatrixRotationAxis(XMLoadFloat3(&mLook), inAngle);

	XMStoreFloat3(&mUp, XMVector3TransformNormal(XMLoadFloat3(&mUp), R));
	XMStoreFloat3(&mRight, XMVector3TransformNormal(XMLoadFloat3(&mRight), R));

	mViewDirty = true;
}

void GameCamera::UpdateViewMatrix() {
	if (mViewDirty) {
		XMVECTOR R = XMLoadFloat3(&mRight);
		XMVECTOR U = XMLoadFloat3(&mUp);
		XMVECTOR L = XMLoadFloat3(&mLook);
		XMVECTOR P = XMLoadFloat3(&mPosition);

		// Keep camera's axes orthogonal to each other and of unit length.
		L = XMVector3Normalize(L);
		U = XMVector3Normalize(XMVector3Cross(L, R));

		// U, L already ortho-normal, so no need to normalize cross product.
		R = XMVector3Cross(U, L);

		// Fill in the view matrix entries.
		float x = -XMVectorGetX(XMVector3Dot(P, R));
		float y = -XMVectorGetX(XMVector3Dot(P, U));
		float z = -XMVectorGetX(XMVector3Dot(P, L));

		XMStoreFloat3(&mRight, R);
		XMStoreFloat3(&mUp, U);
		XMStoreFloat3(&mLook, L);

		mView(0, 0) = mRight.x;
		mView(1, 0) = mRight.y;
		mView(2, 0) = mRight.z;
		mView(3, 0) = x;

		mView(0, 1) = mUp.x;
		mView(1, 1) = mUp.y;
		mView(2, 1) = mUp.z;
		mView(3, 1) = y;

		mView(0, 2) = mLook.x;
		mView(1, 2) = mLook.y;
		mView(2, 2) = mLook.z;
		mView(3, 2) = z;

		mView(0, 3) = 0.0f;
		mView(1, 3) = 0.0f;
		mView(2, 3) = 0.0f;
		mView(3, 3) = 1.0f;

		mViewDirty = false;
	}
}

void GameCamera::LookAt(const FXMVECTOR& inPos, const FXMVECTOR& inTarget, const FXMVECTOR& inWorldUp) {
	XMVECTOR L = XMVector3Normalize(XMVectorSubtract(inTarget, inPos));
	XMVECTOR R = XMVector3Normalize(XMVector3Cross(inWorldUp, L));
	XMVECTOR U = XMVector3Cross(L, R);

	XMStoreFloat3(&mPosition, inPos);
	XMStoreFloat3(&mLook, L);
	XMStoreFloat3(&mRight, R);
	XMStoreFloat3(&mUp, U);

	mViewDirty = true;
}

void GameCamera::LookAt(const XMFLOAT3& inPos, const XMFLOAT3& inTarget, const XMFLOAT3& inUp) {
	XMVECTOR P = XMLoadFloat3(&inPos);
	XMVECTOR T = XMLoadFloat3(&inTarget);
	XMVECTOR U = XMLoadFloat3(&inUp);

	LookAt(P, T, U);
}

XMVECTOR GameCamera::GetPosition() const {
	return XMLoadFloat3(&mPosition);
}

XMFLOAT3 GameCamera::GetPosition3f() const {
	return mPosition;
}

void GameCamera::SetPosition(float inX, float inY, float inZ) {
	mPosition = XMFLOAT3(inX, inY, inZ);
	mViewDirty = true;
}

void GameCamera::SetPosition(const XMVECTOR& inV) {
	XMStoreFloat3(&mPosition, inV);
}

void GameCamera::SetPosition(const XMFLOAT3& inV) {
	mPosition = inV;
	mViewDirty = true;
}

XMVECTOR GameCamera::GetRight() const {
	return XMLoadFloat3(&mRight);
}

XMFLOAT3 GameCamera::GetRight3f() const {
	return mRight;
}

XMVECTOR GameCamera::GetUp() const {
	return XMLoadFloat3(&mUp);
}

XMFLOAT3 GameCamera::GetUp3f() const {
	return mUp;
}

XMVECTOR GameCamera::GetLook() const {
	return XMLoadFloat3(&mLook);
}

XMFLOAT3 GameCamera::GetLook3f() const {
	return mLook;
}

float GameCamera::GetNearZ() const {
	return mNearZ;
}

float GameCamera::GetFarZ() const {
	return mFarZ;
}

float GameCamera::GetAspect() const {
	return mAspect;
}

float GameCamera::GetFovY() const {
	return mFovY;
}

float GameCamera::GetFovX() const {
	float halfWidth = 0.5f*GetNearWindowWidth();
	return 2.0f * atanf(halfWidth / mNearZ);
}

float GameCamera::GetNearWindowWidth() const {
	return mAspect * mNearWindowHeight;
}

float GameCamera::GetNearWindowHeight() const {
	return mNearWindowHeight;
}

float GameCamera::GetFarWindowWidth() const {
	return mAspect * mFarWindowHeight;
}

float GameCamera::GetFarWindowHeight() const {
	return mFarWindowHeight;
}

XMMATRIX GameCamera::GetView() const {
	assert(!mViewDirty);
	return XMLoadFloat4x4(&mView);
}

XMMATRIX GameCamera::GetProj() const {
	return XMLoadFloat4x4(&mProj);
}

XMFLOAT4X4 GameCamera::GetView4x4f() const {
	assert(!mViewDirty);
	return mView;
}

XMFLOAT4X4 GameCamera::GetProj4x4f() const {
	return mProj;
}

void GameCamera::SetInheritPitch(bool inState) {
	bInheritPitch = inState;
}

void GameCamera::SetInheritYaw(bool inState) {
	bInheritYaw = inState;
}

void GameCamera::SetInheritRoll(bool inState) {
	bInheritRoll = inState;
}

void GameCamera::SetLens(float inFovY, float inAspect, float inZnear, float inZfar) {
	// cache properties
	mFovY = inFovY;
	mAspect = inAspect;
	mNearZ = inZnear;
	mFarZ = inZfar;

	mNearWindowHeight = 2.0f * mNearZ * tanf(0.5f * mFovY);
	mFarWindowHeight = 2.0f * mFarZ * tanf(0.5f * mFovY);

	XMMATRIX P = XMMatrixPerspectiveFovLH(mFovY, mAspect, mNearZ, mFarZ);
	XMStoreFloat4x4(&mProj, P);
}

void GameCamera::SetRotationUsingQuaternion(const XMVECTOR& inQuat) {
	XMMATRIX rotMat = XMMatrixRotationQuaternion(inQuat);

	XMVECTOR right = XMLoadFloat3(&mRight);
	if (bInheritYaw || bInheritRoll) 
		right = XMVector3Transform(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), rotMat);

	XMStoreFloat3(&mRight, right);

	XMVECTOR up = XMLoadFloat3(&mUp);
	if (bInheritPitch || bInheritRoll)
		up = XMVector3Transform(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), rotMat);

	XMStoreFloat3(&mUp, up);

	XMVECTOR look = XMLoadFloat3(&mLook);
	if (bInheritPitch || bInheritYaw)
		look = XMVector3Transform(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), rotMat);

	XMStoreFloat3(&mLook, look);

	mViewDirty = true;
}