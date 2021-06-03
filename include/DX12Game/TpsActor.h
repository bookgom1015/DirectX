#pragma once

#include "DX12Game/Actor.h"
#include "DX12Game/CameraComponent.h"

class SkeletalMeshComponent;

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

	float mAzimuth;
	float mYAngularSpeed;

	float mElevation;
	float mPitchRotationSpeed;

	const float mMaxElevation;
	const float mMinElevation;

	const float mCameraDistance;
	const float mCameraPosY;
	const float mCameraTargetPosY;

	const float mLocalOffset;
};