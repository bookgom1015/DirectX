#pragma once

#include "DX12Game/GameCore.h"
#include "DX12Game/InputSystem.h"

class Actor;

class Component {
public:
	Component(Actor* inOwnerActor, int inUpdateOrder = 100);
	virtual ~Component();

	Component(const Component& src) = default;
	Component(Component&& src) = default;

	Component& operator=(const Component& rhs) = default;
	Component& operator=(Component&& rhs) = default;

public:
	//* Update this component by delta time.
	virtual void Update(const GameTimer& gt);
	//* Process input for this component.
	virtual void ProcessInput(const InputState& input);
	//* Called when world transform changes.
	virtual void OnUpdateWorldTransform();

	Actor* GetOwner() const;

	int GetUpdateOrder() const;

	void SetScale(DirectX::XMFLOAT3 inScale);
	void SetScale(DirectX::XMVECTOR inScale);
	DirectX::XMVECTOR GetScale() const;
	DirectX::XMFLOAT3 GetScale3f() const;

	void SetQuaternion(DirectX::XMFLOAT4 inQuaternion);
	void SetQuaternion(DirectX::XMVECTOR inQuaternion);
	DirectX::XMVECTOR GetQuaternion() const;
	DirectX::XMFLOAT4 GetQuaternion4f() const;

	void SetPosition(DirectX::XMFLOAT3 inPosition);
	void SetPosition(DirectX::XMVECTOR inPosition);
	DirectX::XMVECTOR GetPosition() const;
	DirectX::XMFLOAT3 GetPosition3f() const;

protected:
	// Owning actor.
	Actor* mOwner;
	// Update order of component.
	int mUpdateOrder;

	DirectX::XMFLOAT3 mScale = { 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT4 mQuaternion = { 0.0f, 0.0f, 0.0f, 1.0f };
	DirectX::XMFLOAT3 mPosition = { 0.0f, 0.0f, 0.0f };
};