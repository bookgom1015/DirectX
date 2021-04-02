#pragma once

#include "DX12Game/Actor.h"
#include "DX12Game/CameraComponent.h"

class SkeletalMeshComponent;

class TpsActor : public Actor {
public:
	TpsActor();
	virtual ~TpsActor();

public:
	//* Any actor-specific update code (overridable)
	virtual void UpdateActor(const GameTimer& gt) override;
	//* Any actor-specific input code (overridable)
	virtual void ActorInput(const InputState& input) override;
	//* Called when the application loads data
	virtual bool OnLoadingData() override;

private:
	std::unique_ptr<CameraComponent> mCameraComp = nullptr;

	int mForwardSpeed = 0;
	int mStrafeSpeed = 0;

	const float mWalkingSpeed = 2.0f;

	float mAzimuth = -DirectX::XM_PIDIV2;
	float mYAngularSpeed = 0.0f;

	float mElevation = 0.0f;
	float mPitchRotationSpeed = 0.0f;

	const float mMaxElevation = 0.85f;
	const float mMinElevation = -0.85f;

	const float mCameraDistance = 4.0f;
	const float mCameraPosY = 2.0f;
	const float mCameraTargetPosY = 1.8f;

	const float mLocalOffset = 0.3f;

	SkeletalMeshComponent* mSkeletalMeshComponent;
};