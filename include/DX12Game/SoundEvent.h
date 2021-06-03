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

	bool GetPaused() const;
	void SetPaused(bool inPause);

	float GetVolume() const;
	void SetVolume(float inVolume);

	float GetPitch() const;
	void SetPitch(float inPitch);

	float GetParameter(const std::string& inParamName);
	void SetParameter(const std::string& inParamName, float inValue);

	bool Is3D() const;
	void Set3DAttributes(const DirectX::XMFLOAT4X4& inWorldTransform);

private:
	AudioSystem* mSystem;
	unsigned int mID;
};