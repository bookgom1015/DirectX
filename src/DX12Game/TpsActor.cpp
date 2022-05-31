#include "DX12Game/TpsActor.h"
#include "DX12Game/GameWorld.h"
#include "DX12Game/Renderer.h"
#include "DX12Game/SkeletalMeshComponent.h"
#include "DX12Game/CameraComponent.h"

using namespace DirectX;
using namespace DirectX::PackedVector;

TpsActor::TpsActor()
	: Actor(),
	mWalkingSpeed(2.0f),
	mRunningSpeed(8.0f),
	mMaxElevation(0.85f),
	mMinElevation(-0.85f),	
	mCameraMaxDistance(5.0f),
	mCameraCurrDistance(3.0f),
	mCameraMinDistance(1.0f),
	mCameraPosY(2.0f),
	mCameraTargetPosY(1.8f),	
	mLocalOffset(0.3f) {

	mCameraComp = std::make_unique<CameraComponent>(this);

	GameWorld::GetWorld()->GetRenderer()->SetMainCamerea(mCameraComp->GetCamera());

	mSkeletalMeshComponent = new SkeletalMeshComponent(this);

	mForwardSpeed = 0;
	mStrafeSpeed = 0;

	mAzimuth = -DirectX::XM_PIDIV2;
	mYAngularSpeed = 0.0f;

	mElevation = 0.0f;
	mPitchRotationSpeed = 0.0f;
}

void TpsActor::UpdateActor(const GameTimer& gt) {
	//----------------------------------------------------------------------------------------------------------------
	// Get camera relative position.
	//----------------------------------------------------------------------------------------------------------------
	mAzimuth += mYAngularSpeed;

	mElevation += mPitchRotationSpeed;
	if (mElevation > mMaxElevation)
		mElevation = mMaxElevation;
	else if (mElevation < mMinElevation)
		mElevation = mMinElevation;

	float sinA = MathHelper::Sin(mAzimuth);
	float cosA = MathHelper::Cos(mAzimuth);
	float sinE = MathHelper::Sin(mElevation);
	float cosE = MathHelper::Cos(mElevation);

	float t = mCameraCurrDistance * cosE;
	float offsetX = mLocalOffset * -sinA;
	float offsetZ = mLocalOffset * cosA;
	float x = t * cosA + offsetX;
	float y = mCameraCurrDistance * sinE;
	float z = t * sinA + offsetZ;

	//----------------------------------------------------------------------------------------------------------------
	// Translate this actor.
	//----------------------------------------------------------------------------------------------------------------
	auto look3f = mCameraComp->GetLook3f();
	look3f.y = 0.0f;
	auto lookVec = XMVector4Normalize(XMLoadFloat3(&look3f));

	const XMVECTOR& forward = lookVec * static_cast<float>(mForwardSpeed);
	const XMVECTOR& right = mCameraComp->GetRight() * static_cast<float>(mStrafeSpeed);

	XMVECTOR disp = XMVector4Normalize(XMVectorAdd(forward, right));
	const XMVECTOR& pos = XMVectorAdd(GetPosition(), disp * mCurrSpeed * gt.DeltaTime());

	SetPosition(pos);

	XMVECTOR cameraDisp = XMVectorSet(x, y + mCameraPosY, z, 0.0f);
	XMVECTOR cameraPos = XMVectorAdd(pos, cameraDisp);
	mCameraComp->SetPosition(cameraPos);

	XMVECTOR cameraTarget = XMVectorAdd(pos, XMVectorSet(offsetX, mCameraTargetPosY, offsetZ, 0.0f));
	mCameraComp->LookAt(cameraPos, cameraTarget, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));

	//----------------------------------------------------------------------------------------------------------------
	// Rotate mesh to look moving forward direction.
	//----------------------------------------------------------------------------------------------------------------
	if (mForwardSpeed != 0 || mStrafeSpeed != 0) {
		XMVECTOR direction = XMVectorAdd(forward, right);
		XMVECTOR normalizedDirection = XMVector4Normalize(direction);
		XMVECTOR dot = XMVector4Dot(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), normalizedDirection);
		float theta = MathHelper::ACos(dot.m128_f32[0]);

		XMVECTOR cross = XMVector3Cross(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), normalizedDirection);
		if (cross.m128_f32[1] <= 0.0f)
			theta *= -1.0f;

		mSkeletalMeshComponent->SetQuaternion(XMQuaternionRotationAxis(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), theta));
	}
}

void TpsActor::ProcessActorInput(const InputState& input) {
	mForwardSpeed = 0;
	mStrafeSpeed = 0;

	if (input.Keyboard.GetKeyValue('W'))
		mForwardSpeed += 1;
	if (input.Keyboard.GetKeyValue('S'))
		mForwardSpeed -= 1;
	if (input.Keyboard.GetKeyValue('A'))
		mStrafeSpeed -= 1;
	if (input.Keyboard.GetKeyValue('D'))
		mStrafeSpeed += 1;

	float scroll = input.Mouse.GetScrollWheel();
	if (scroll > 0.0f) {
		mCameraCurrDistance -= 0.1f;
		if (mCameraCurrDistance < mCameraMinDistance)
			mCameraCurrDistance = mCameraMinDistance;
	}
	else if (scroll < 0.0f) {
		mCameraCurrDistance += 0.1f;
		if (mCameraCurrDistance > mCameraMaxDistance)
			mCameraCurrDistance = mCameraMaxDistance;
	}
	
	mCurrSpeed = input.Keyboard.GetKeyValue(VK_LSHIFT) ? mRunningSpeed : mWalkingSpeed;

	// Make each pixel correspond to a quarter of a degree.
	mYAngularSpeed = XMConvertToRadians(input.Mouse.GetPosition().x * -0.25f);
	mPitchRotationSpeed = XMConvertToRadians(input.Mouse.GetPosition().y * 0.25f);

	if (mForwardSpeed != 0 || mStrafeSpeed != 0)
		mSkeletalMeshComponent->SetClipName("Walk");
	else
		mSkeletalMeshComponent->SetClipName("Idle");
}

GameResult TpsActor::OnLoadingData() {
	CheckGameResult(mSkeletalMeshComponent->LoadMesh("leoni", "leoni.fbx"));

	mSkeletalMeshComponent->SetSkeleletonVisible(true);

	return true;
}