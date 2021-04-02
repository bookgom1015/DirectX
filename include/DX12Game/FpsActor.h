#pragma once

#include "Actor.h"

class MeshComponent;
class CameraComponent;

class FpsActor : public Actor {
public:
	FpsActor();
	virtual ~FpsActor();

public:
	// Any actor-specific update code (overridable)
	virtual void UpdateActor(const GameTimer& gt) override;
	// Any actor-specific input code (overridable)
	virtual void ActorInput(const InputState& input) override;

private:
	std::unique_ptr<CameraComponent> mCameraComp = nullptr;

	float mForwardSpeed = 0.0f;
	float mStrafeSpeed = 0.0f;

	float mYAngularSpeed = 0.0f;
	
	float mPhi = 0.0f;
	float mPhiRotationSpeed = 0.0f;

	const float mMaxPhi = DirectX::XM_PIDIV2 - 0.1f;
	const float mMinPhi = -DirectX::XM_PIDIV2 + 0.1f;

	const float mCameraPosY = 2.0f;
};