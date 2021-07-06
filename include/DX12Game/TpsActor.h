#pragma once

#include "DX12Game/Actor.h"

class SkeletalMeshComponent;
class CameraComponent;

class TpsActor : public Actor {
public:
	TpsActor();
	virtual ~TpsActor() = default;

public:
	virtual void UpdateActor(const GameTimer& gt) override;
	virtual void ProcessActorInput(const InputState& input) override;
	virtual bool OnLoadingData() override;

private:
	std::unique_ptr<CameraComponent> mCameraComp = nullptr;
	SkeletalMeshComponent* mSkeletalMeshComponent;

	int mForwardSpeed;
	int mStrafeSpeed;

	const float mWalkingSpeed;
	const float mRunningSpeed;
	float mCurrSpeed;

	float mAzimuth;
	float mYAngularSpeed;

	float mElevation;
	float mPitchRotationSpeed;

	const float mMaxElevation;
	const float mMinElevation;

	const float mCameraMaxDistance;
	const float mCameraMinDistance;
	float mCameraCurrDistance;

	const float mCameraPosY;
	const float mCameraTargetPosY;

	const float mLocalOffset;
};