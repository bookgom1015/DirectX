#pragma once

#include "DX12Game/InputSystem.h"

class Actor;

class Component {
public:
	Component(Actor* inOwnerActor, int inUpdateOrder = 100);
	virtual ~Component() = default;

	Component(const Component& src) = default;
	Component(Component&& src) = default;

	Component& operator=(const Component& rhs) = default;
	Component& operator=(Component&& rhs) = default;

public:
	virtual void Update(const GameTimer& gt);
	virtual void ProcessInput(const InputState& input);
	virtual void OnUpdateWorldTransform();

	Actor* GetOwner() const;

	int GetUpdateOrder() const;

	DirectX::XMVECTOR GetScale() const;
	DirectX::XMFLOAT3 GetScale3f() const;
	void SetScale(DirectX::XMFLOAT3 inScale);
	void SetScale(DirectX::XMVECTOR inScale);

	DirectX::XMVECTOR GetQuaternion() const;
	DirectX::XMFLOAT4 GetQuaternion4f() const;
	void SetQuaternion(DirectX::XMFLOAT4 inQuaternion);
	void SetQuaternion(DirectX::XMVECTOR inQuaternion);

	DirectX::XMVECTOR GetPosition() const;
	DirectX::XMFLOAT3 GetPosition3f() const;
	void SetPosition(DirectX::XMFLOAT3 inPosition);
	void SetPosition(DirectX::XMVECTOR inPosition);

protected:
	Actor* mOwner;
	int mUpdateOrder;

	DirectX::XMFLOAT3 mScale = { 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT4 mQuaternion = { 0.0f, 0.0f, 0.0f, 1.0f };
	DirectX::XMFLOAT3 mPosition = { 0.0f, 0.0f, 0.0f };
};