// ----------------------------------------------------------------
// From Game Programming in C++ by Sanjay Madhav
// Copyright (C) 2017 Sanjay Madhav. All rights reserved.
// 
// Released under the BSD License
// See LICENSE in root directory for full details.
// ----------------------------------------------------------------

#pragma once

#include "DX12Game/GameCore.h"

class AudioSystem;

class SoundEvent final {
public:
	SoundEvent();

protected:
	// Make this constructor protected and AudioSystem a friend
	// so that only AudioSystem can access this constructor.
	friend class AudioSystem;
	SoundEvent(AudioSystem* inSystem, unsigned int inId);

public:
	virtual ~SoundEvent() = default;

public:
	// Returns true if associated FMOD event still exists
	bool IsValid();

	// Restart event from begining
	void Restart();

	// Stop this event
	void Stop(bool inAllowFadeOut = true);

	// Setters
	void SetPaused(bool inPause);
	void SetVolume(float inVolume);
	void SetPitch(float inPitch);
	void SetParameter(const std::string& inParamName, float inValue);

	// Getters
	bool GetPaused() const;
	float GetVolume() const;
	float GetPitch() const;
	float GetParameter(const std::string& inParamName);

	// Positional
	bool Is3D() const;
	void Set3DAttributes(const DirectX::XMFLOAT4X4& inWorldTransform);

private:
	AudioSystem* mSystem;
	unsigned int mID;
};