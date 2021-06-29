#pragma once

#include "DX12Game/GameCore.h"
#include "DX12Game/InputSystem.h"

class Component;

class Actor {
public:
	enum ActorState {
		EActive,
		EPaused,
		EDead
	};

public:
	Actor();
	virtual ~Actor();

	Actor(const Actor& src) = default;
	Actor(Actor&& src) = default;

	Actor& operator=(const Actor& rhs) = default;
	Actor& operator=(Actor&& rhs) = default;

public:
	void Update(const GameTimer& gt);
	void UpdateComponents(const GameTimer& gt);
	void ProcessInput(const InputState& input);
	virtual bool OnLoadingData();
	virtual void OnUnloadingData();

	void AddComponent(Component* inComponent);
	void RemoveComponent(Component* inComponent);

	void ComputeWorldTransform();

protected:
	virtual void UpdateActor(const GameTimer& gt);
	virtual void ProcessActorInput(const InputState& input);

public:
	void AddFunction(std::shared_ptr<std::function<void(const GameTimer&, Actor*)>> inFunction);
	void RemoveFunction(std::shared_ptr<std::function<void(const GameTimer&, Actor*)>> inFunction);

	ActorState GetState() const;
	void SetState(ActorState inState);

	DirectX::XMMATRIX GetWorldTransform() const;
	const DirectX::XMFLOAT4X4& GetWorldTransform4x4f() const;
	
	DirectX::XMVECTOR GetScale() const;
	const DirectX::XMFLOAT3& GetScale3f() const;
	void SetScale(float inX, float inY, float inZ);
	void SetScale(const DirectX::XMFLOAT3& inScale);
	void SetScale(const DirectX::XMVECTOR& inScale);

	DirectX::XMVECTOR GetQuaternion() const;
	const DirectX::XMFLOAT4& GetQuaternion4f() const;
	void SetQuaternion(const DirectX::XMFLOAT4& inQuat);
	void SetQuaternion(const DirectX::XMVECTOR& inQuat);

	DirectX::XMVECTOR GetPosition() const;
	const DirectX::XMFLOAT3& GetPosition3f() const;
	void SetPosition(float inX, float inY, float inZ);
	void SetPosition(const DirectX::XMFLOAT3& inPos);
	void SetPosition(const DirectX::XMVECTOR& inPos);

	bool GetIsDirty() const;
	void SetActorClean();

	UINT GetOwnerThreadID() const;
	void SetOwnerThreadID(UINT inId);

private:
	ActorState mState = ActorState::EActive;

	DirectX::XMFLOAT4X4 mWorldTransform = MathHelper::Identity4x4();
	DirectX::XMFLOAT3 mScale = { 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT4 mQuaternion = { 0.0f, 0.0f, 0.0f, 1.0f };
	DirectX::XMFLOAT3 mPosition = { 0.0f, 0.0f, 0.0f };
	bool bRecomputeWorldTransform = true;

	GVector<Component*> mComponents;

	GVector<std::shared_ptr<std::function<void(const GameTimer&, Actor*)>>> mFunctions;

	bool mIsDirty = false;

	UINT mOwnerId;
};