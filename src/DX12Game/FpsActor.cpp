#include "DX12Game/FpsActor.h"
#include "DX12Game/GameWorld.h"
#include "DX12Game/Renderer.h"
#include "DX12Game/MeshComponent.h"
#include "DX12Game/CameraComponent.h"

using namespace DirectX;
using namespace DirectX::PackedVector;

FpsActor::FpsActor()
	: Actor() {
	mCameraComp = std::make_unique<CameraComponent>(this);
	mCameraComp->SetInheritYaw(true);

	GameWorld::GetWorld()->GetRenderer()->SetMainCamerea(mCameraComp->GetCamera());
}

FpsActor::~FpsActor() {}

void FpsActor::UpdateActor(const GameTimer& gt) {
	//--------------------------------------------------------
	// Rotate(axis Y) this actor.
	//--------------------------------------------------------
	XMVECTOR quat = XMQuaternionRotationAxis(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), mYAngularSpeed);
	SetQuaternion(XMQuaternionMultiply(GetQuaternion(), quat));

	//--------------------------------------------------------
	// Rotate(axis pitch) camera component for this actor.
	//--------------------------------------------------------
	mPhi += mPhiRotationSpeed;
	if (mPhi > mMaxPhi)	
		mPhi = mMaxPhi;
	else if (mPhi < mMinPhi) 
		mPhi = mMinPhi;

	mCameraComp->Pitch(mPhi);

	//--------------------------------------------------------
	// Translate this actor.
	//--------------------------------------------------------
	auto look3f = mCameraComp->GetLook3f();
	look3f.y = 0.0f;
	auto lookVec = XMVector4Normalize(XMLoadFloat3(&look3f));
	
	const XMVECTOR& forward = XMVectorScale(lookVec, mForwardSpeed * gt.DeltaTime());
	const XMVECTOR& right = XMVectorScale(mCameraComp->GetRight(), mStrafeSpeed * gt.DeltaTime());

	XMVECTOR disp = XMVectorAdd(forward, right);
	const XMVECTOR& pos = XMVectorAdd(GetPosition(), disp);

	SetPosition(pos);
	mCameraComp->SetPosition(XMVectorAdd(pos, XMVectorSet(0.0f, mCameraPosY, 0.0f, 0.0f)));
}

void FpsActor::ActorInput(const InputState& input) {
	mForwardSpeed = 0.0f;
	mStrafeSpeed = 0.0f;

	if (input.Keyboard.GetKeyValue('W')) 
		mForwardSpeed += 5.0f;
	if (input.Keyboard.GetKeyValue('S'))
		mForwardSpeed -= 5.0f;
	if (input.Keyboard.GetKeyValue('A'))
		mStrafeSpeed -= 5.0f;
	if (input.Keyboard.GetKeyValue('D'))
		mStrafeSpeed += 5.0f;

	// Make each pixel correspond to a quarter of a degree.
	mYAngularSpeed = XMConvertToRadians(input.Mouse.GetPosition().x * 0.25f);
	mPhiRotationSpeed = XMConvertToRadians(input.Mouse.GetPosition().y * 0.25f);
}