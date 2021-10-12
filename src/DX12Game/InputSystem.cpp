#include "DX12Game/InputSystem.h"

using namespace DirectX;
using namespace DirectX::PackedVector;

namespace {
	bool GetKeyButtonValue(int inKeyCode) {
		SHORT status = GetAsyncKeyState(inKeyCode);

		if (status & 0x8000 || status & 0x8001)
			return true;
		else
			return false;
	}

	ButtonState GetKeyButtonState(int inKeyCode) {
		SHORT status = GetAsyncKeyState(inKeyCode);

		if (status & 0x0000)
			return ButtonState::ENone;
		else if (status & 0x8000)
			return ButtonState::EPressed;
		else if (status & 0x0001)
			return ButtonState::EReleased;
		else
			return ButtonState::EHeld;
	}
}

bool KeyboardState::GetKeyValue(int inKeyCode) const {
	return GetKeyButtonValue(inKeyCode);
}

ButtonState KeyboardState::GetKeyState(int inKeyCode) const {
	return GetKeyButtonState(inKeyCode);
}

void MouseState::WheelUp() {
	mScrollWheelAccum += 1.0f;
}

void MouseState::WheelDown() {
	mScrollWheelAccum -= 1.0f;
}

XMFLOAT2 MouseState::GetPosition() const {
	return mMousePos;
}

float MouseState::GetScrollWheel() const {
	return mScrollWheel;
}

XMFLOAT2 MouseState::GetMouseCenter() const {
	return mMouseCenter;
}

bool MouseState::IsRelative() const {
	return mIsRelative;
}

bool MouseState::IsIgnored() const {
	return mIsIgnored;
}

bool MouseState::GetButtonValue(int inButton) const {
	return GetKeyButtonValue(inButton);
}

ButtonState MouseState::GetButtonState(int inButton) const {
	return GetKeyButtonState(inButton);
}

InputSystem::InputSystem() {}

InputSystem::~InputSystem() {
	if (!bIsCleaned)
		CleanUp();
}

bool InputSystem::Initialize(HWND hMainWnd) {
	mhMainWnd = hMainWnd;

	ShowCursor(false);

	mState.Mouse.mMousePos = XMFLOAT2(0.0f, 0.0f);
	mState.Mouse.mScrollWheel = 0.0f;
	mState.Mouse.mScrollWheelAccum = 0.0f;
	mState.Mouse.mMouseCenter = XMFLOAT2(0.0f, 0.0f);
	mState.Mouse.mIsRelative = false;
	mState.Mouse.mIsIgnored = true;

	return true;
}

void InputSystem::CleanUp() {
	ShowCursor(true);

	bIsCleaned = true;
}

void InputSystem::PrepareForUpdate() {
	RECT wndRect;
	GetWindowRect(mhMainWnd, &wndRect);

	float xPos = (wndRect.left + wndRect.right) * 0.5f;
	float yPos = (wndRect.top + wndRect.bottom) * 0.5f;

	mState.Mouse.mMouseCenter = XMFLOAT2(xPos, yPos);

	mState.Mouse.mIsRelative = false;

	mState.Mouse.mScrollWheel = mState.Mouse.mScrollWheelAccum;
	mState.Mouse.mScrollWheelAccum = 0.0f;
}

void InputSystem::Update() {
	if (mState.Mouse.mIsRelative) {
		POINT cursorPos;
		GetCursorPos(&cursorPos);

		int x = static_cast<int>(mState.Mouse.GetMouseCenter().x);
		int y = static_cast<int>(mState.Mouse.GetMouseCenter().y);
		SetCursorPos(x, y);

		if (mState.Mouse.mIsIgnored) {
			mState.Mouse.mMousePos.x = 0.0f;
			mState.Mouse.mMousePos.y = 0.0f;
			mState.Mouse.mIsIgnored = false;
		}
		else {
			mState.Mouse.mMousePos.x = static_cast<float>(cursorPos.x - x);
			mState.Mouse.mMousePos.y = static_cast<float>(cursorPos.y - y);
		}
	}
	else {
		POINT cursorPos;
		GetCursorPos(&cursorPos);
		mState.Mouse.mMousePos.x = static_cast<float>(cursorPos.x);
		mState.Mouse.mMousePos.y = static_cast<float>(cursorPos.y);
	}
}

void InputSystem::IgnoreMouseInput() {
	mState.Mouse.mIsIgnored = true;
}

void InputSystem::OnWheelUp() {
	mState.Mouse.WheelUp();
}

void InputSystem::OnWheelDown() {
	mState.Mouse.WheelDown();
}

const InputState& InputSystem::GetState() const {
	return mState;
}

void InputSystem::SetRelativeMouseMode(bool inValue) {
	mState.Mouse.mIsRelative = inValue;
}