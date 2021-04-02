#pragma once

#include "Component.h"

class Actor;
class GameCamera;

class CameraComponent : Component {
public:
	CameraComponent(Actor* inActor, int inUpdateOrder = 200);
	virtual ~CameraComponent();

public:
	// Update this component by delta time.
	virtual void Update(const GameTimer& gt) override;
	// Called when world transform changes.
	virtual void OnUpdateWorldTransform() override;

	// Rotate the camera.
	void Pitch(float inAngle);
	void RotateY(float inAngle);
	void Roll(float inAngle);

	// After modifying camera position/orientation, call to rebuild the view matrix.
	void UpdateViewMatrix();

	// Define camera space via LookAt parameters.
	void LookAt(const DirectX::FXMVECTOR& inPos, const DirectX::FXMVECTOR& inTarget, const DirectX::FXMVECTOR& inWorldUp);
	void LookAt(const DirectX::XMFLOAT3& inPos, const DirectX::XMFLOAT3& inTarget, const DirectX::XMFLOAT3& inUp);

	// Get/Set world camera position.
	DirectX::XMVECTOR GetPosition() const;
	DirectX::XMFLOAT3 GetPosition3f() const;
	void SetPosition(float inX, float inY, float inZ);
	void SetPosition(const DirectX::XMVECTOR& inV);
	void SetPosition(const DirectX::XMFLOAT3& inV);

	// Get camera basis vectors.
	DirectX::XMVECTOR GetRight() const;
	DirectX::XMFLOAT3 GetRight3f() const;
	DirectX::XMVECTOR GetUp() const;
	DirectX::XMFLOAT3 GetUp3f() const;
	DirectX::XMVECTOR GetLook() const;
	DirectX::XMFLOAT3 GetLook3f() const;

	// Get frustum properties.
	float GetNearZ() const;
	float GetFarZ() const;
	float GetAspect() const;
	float GetFovY() const;
	float GetFovX() const;

	// Get near and far plane dimensions in view space coordinates.
	float GetNearWindowWidth() const;
	float GetNearWindowHeight() const;
	float GetFarWindowWidth() const;
	float GetFarWindowHeight() const;

	// Get View/Proj matrices.
	DirectX::XMMATRIX GetView() const;
	DirectX::XMMATRIX GetProj() const;

	DirectX::XMFLOAT4X4 GetView4x4f() const;
	DirectX::XMFLOAT4X4 GetProj4x4f() const;

	GameCamera* GetCamera() const;

	void SetInheritPitch(bool inState);
	void SetInheritYaw(bool inState);
	void SetInheritRoll(bool inState);

	// Set frustum.
	void SetLens(float inFovY, float inAspect, float inZnear, float inZfar);

private:
	std::unique_ptr<GameCamera> mCamera = nullptr;
};