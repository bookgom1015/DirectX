#pragma once

class GameCamera {
public:
	GameCamera();
	virtual ~GameCamera() = default;

public:
	void Pitch(float inAngle);
	void RotateY(float inAngle);
	void Roll(float inAngle);

	///
	// After modifying camera position/orientation, call to rebuild the view matrix.
	///
	void UpdateViewMatrix();

	///
	// Define camera space via LookAt parameters.
	///
	void LookAt(const DirectX::FXMVECTOR& inPos, const DirectX::FXMVECTOR& inTarget, const DirectX::FXMVECTOR& inWorldUp);
	void LookAt(const DirectX::XMFLOAT3& inPos, const DirectX::XMFLOAT3& inTarget, const DirectX::XMFLOAT3& inUp);

	DirectX::XMVECTOR GetPosition() const;
	DirectX::XMFLOAT3 GetPosition3f() const;
	void SetPosition(float inX, float inY, float inZ);
	void SetPosition(const DirectX::XMVECTOR& inV);
	void SetPosition(const DirectX::XMFLOAT3& inV);

	DirectX::XMVECTOR GetRight() const;
	DirectX::XMFLOAT3 GetRight3f() const;
	DirectX::XMVECTOR GetUp() const;
	DirectX::XMFLOAT3 GetUp3f() const;
	DirectX::XMVECTOR GetLook() const;
	DirectX::XMFLOAT3 GetLook3f() const;

	float GetNearZ() const;
	float GetFarZ() const;
	float GetAspect() const;
	float GetFovY() const;
	float GetFovX() const;

	float GetNearWindowWidth() const;
	float GetNearWindowHeight() const;
	float GetFarWindowWidth() const;
	float GetFarWindowHeight() const;

	DirectX::XMMATRIX GetView() const;
	DirectX::XMMATRIX GetProj() const;

	DirectX::XMFLOAT4X4 GetView4x4f() const;
	DirectX::XMFLOAT4X4 GetProj4x4f() const;

	void SetInheritPitch(bool inState);
	void SetInheritYaw(bool inState);
	void SetInheritRoll(bool inState);

	void SetLens(float inFovY, float inAspect, float inZnear, float inZfar);

	void SetRotationUsingQuaternion(const DirectX::XMVECTOR& inQuat);

private:
	// Camera coordinate system with coordinates relative to world space.
	DirectX::XMFLOAT3 mPosition;
	DirectX::XMFLOAT3 mRight;
	DirectX::XMFLOAT3 mUp;
	DirectX::XMFLOAT3 mLook;

	// Cache frustum properties.
	float mNearZ;
	float mFarZ;
	float mAspect;
	float mFovY;
	float mNearWindowHeight;
	float mFarWindowHeight;

	bool mViewDirty;

	// Cache View/Proj matrices.
	DirectX::XMFLOAT4X4 mView;
	DirectX::XMFLOAT4X4 mProj;

	bool bInheritPitch;
	bool bInheritYaw;
	bool bInheritRoll;
};