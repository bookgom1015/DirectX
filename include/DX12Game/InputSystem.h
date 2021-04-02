#pragma once

#include "DX12Game/GameCore.h"

// The different button states
enum class ButtonState {
	ENone,
	EPressed,
	EReleased,
	EHeld
};

// Helper for keyboard input
class KeyboardState {
public:
	// Friend so InputSystem can easily update it
	friend class InputSystem;

	// Get just the boolean true/false value of key
	bool GetKeyValue(int inKeyCode) const;
	// Get a state based on current and previous frame
	ButtonState GetKeyState(int inKeyCode) const;
};

// Helper for mouse input
class MouseState {
public:
	friend class InputSystem;

	// For mouse position
	const DirectX::XMFLOAT2& GetPosition() const;
	const DirectX::XMFLOAT2& GetScrollWheel() const;
	const DirectX::XMFLOAT2& GetMouseCenter() const;
	bool IsRelative() const;
	bool IsIgnored() const;

	// For buttons
	bool GetButtonValue(int inButton) const;
	ButtonState GetButtonState(int inButton) const;

private:
	// Store current mouse position
	DirectX::XMFLOAT2 mMousePos;
	// Motion of scroll wheel
	DirectX::XMFLOAT2 mScrollWheel;
	DirectX::XMFLOAT2 mMouseCenter;
	// Are we in relative mouse mode
	bool mIsRelative;
	bool mIsIgnored;
};

// Helper for controller input
class ControllerState {
public:
	friend class InputSystem;
};

// Wrapper that contains current state of input
struct InputState {
public:
	KeyboardState Keyboard;
	MouseState Mouse;
	ControllerState Controller;
};

class InputSystem final {
public:
	InputSystem();
	virtual ~InputSystem();

private:
	InputSystem(const InputSystem& src) = delete;
	InputSystem(InputSystem&& src) = delete;
	InputSystem& operator=(const InputSystem& rhs) = delete;
	InputSystem& operator=(InputSystem&& rhs) = delete;

public:
	bool Initialize(HWND hMainWnd);

	void PrepareForUpdate();
	void Update();

	void IgnoreMouseInput();

	const InputState& GetState() const;

	void SetRelativeMouseMode(bool inValue);

private:
	HWND mhMainWnd = nullptr; // main window handle

	InputState mState;
};