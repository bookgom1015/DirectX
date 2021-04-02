#pragma once

#pragma comment(lib, "fmodstudioL64_vc.lib")
#pragma comment(lib, "fmodstudio64_vc.lib")
#pragma comment(lib, "fmod64_vc.lib")
#pragma comment(lib, "fmodL64_vc.lib")

// ----------------------------------------------------------------
// From Game Programming in C++ by Sanjay Madhav
// Copyright (C) 2017 Sanjay Madhav. All rights reserved.
// 
// Released under the BSD License
// See LICENSE in root directory for full details.
// ----------------------------------------------------------------

#pragma once

#include "DX12Game/SoundEvent.h"

// Forward declarations to avoid including FMOD header
namespace FMOD {
	class System;
	namespace Studio {
		class Bank;
		class EventDescription;
		class EventInstance;
		class System;
		class Bus;
	};
};

class AudioSystem final {
public:
	AudioSystem();
	virtual ~AudioSystem();

public:
	bool Initialize();
	void Update(const GameTimer& gt);

	// Load / unload banks
	bool LoadBank(const std::string& inFileName);
	void UnloadBank(const std::string& inFileName);
	void UnloadAllBanks();

	SoundEvent PlayEvent(const std::string& inEventName);

	// For posiitonal audio
	void SetListener(const DirectX::XMFLOAT4X4& inViewMatrix);

	// Control buses
	float GetBusVolume(const std::string& inBusName) const;
	bool GetBusPaused(const std::string& inBusName) const;
	void SetBusVolume(const std::string& inBusName, float inVolume);
	void SetBusPaused(const std::string& inBusName, bool inPause);

protected:
	friend class SoundEvent;
	FMOD::Studio::EventInstance* GetEventInstance(unsigned int inId);

private:
	// FMOD studio system
	FMOD::Studio::System* mSystem = nullptr;
	// FMODE Low-level system (in case needed)
	FMOD::System* mLowLevelSystem = nullptr;

	// Map of loaded banks
	std::unordered_map<std::string, FMOD::Studio::Bank*> mBanks;
	// Map of event name to EventDescription
	std::unordered_map<std::string, FMOD::Studio::EventDescription*> mEvents;
	// Map of event id to EventInstance
	std::unordered_map<unsigned int, FMOD::Studio::EventInstance*> mEventInstances;
	// Map of buses
	std::unordered_map<std::string, FMOD::Studio::Bus*> mBuses;

	// Tracks the next ID to use for event instances
	static unsigned int sNextID;
};