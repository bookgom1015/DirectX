#pragma once

#include "Actor.h"

class MeshComponent;
class CameraComponent;

class FpsActor : public Actor {
public:
	FpsActor();
	virtual ~FpsActor();

public:
	virtual void UpdateActor(const GameTimer& gt) override;
	virtual void ProcessActorInput(const InputState& input) override;

private:
	std::unique_ptr<CameraComponent> mCameraComp = nullptr;

	float mForwardSpeed;
	float mStrafeSpeed;

	float mYAngularSpeed;
	
	float mPhi;
	float mPhiRotationSpeed;

	const float mMaxPhi;
	const float mMinPhi;

	const float mCameraPosY;
};