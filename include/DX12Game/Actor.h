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
	// Update function called from GameWorld (not overridable).
	void Update(const GameTimer& gt);
	// Updates all the components attached to the actor (not overridable).
	void UpdateComponents(const GameTimer& gt);
	// ProcessInput function called from Game (not overridable).
	void ProcessInput(const InputState& input);
	// Called when the application loads data
	virtual bool OnLoadingData();
	// Called when the application unloads data
	virtual void OnUnloadingData();

	// Add/remove components.
	void AddComponent(Component* inComponent);
	void RemoveComponent(Component* inComponent);

	void ComputeWorldTransform();

protected:
	//* Any actor-specific update code (overridable)
	virtual void UpdateActor(const GameTimer& gt);
	//* Any actor-specific input code (overridable)
	virtual void ActorInput(const InputState& input);

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
	// Actors's state.
	ActorState mState = ActorState::EActive;

	// Actor's Transform.
	DirectX::XMFLOAT4X4 mWorldTransform = MathHelper::Identity4x4();
	DirectX::XMFLOAT3 mScale = { 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT4 mQuaternion = { 0.0f, 0.0f, 0.0f, 1.0f };
	DirectX::XMFLOAT3 mPosition = { 0.0f, 0.0f, 0.0f };
	bool bRecomputeWorldTransform = true;

	std::vector<Component*> mComponents;

	std::vector<std::shared_ptr<std::function<void(const GameTimer&, Actor*)>>> mFunctions;

	bool mIsDirty = false;

	UINT mOwnerId;
};