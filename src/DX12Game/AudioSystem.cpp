#include "DX12Game/GameCore.h"
#include "DX12Game/AudioSystem.h"

#include <fmod_studio.hpp>
#include <fmod_errors.h>

using namespace DirectX;
using namespace DirectX::PackedVector;

unsigned int AudioSystem::sNextID = 0;

AudioSystem::AudioSystem() {}

AudioSystem::~AudioSystem() {
	// Unload all banks
	UnloadAllBanks();

	// Shutdown FMOD system
	if (mSystem)
		mSystem->release();

	if (mLowLevelSystem)
		mLowLevelSystem->release();
}

bool AudioSystem::Initialize() {
	FMOD::Debug_Initialize(
		FMOD_DEBUG_LEVEL_ERROR, // Log only erros
		FMOD_DEBUG_MODE_TTY // Out to stdout
	);

	// Create FMOD studio system object
	FMOD_RESULT result;
	result = FMOD::Studio::System::create(&mSystem);
	if (result != FMOD_OK) {
		std::wstringstream wsstream;
		wsstream << L"Failed to create FMOD system: " << FMOD_ErrorString(result) << std::endl;
		MessageBox(nullptr, wsstream.str().c_str(), L"FMOD Error", MB_OK);
		return false;
	}

	// Initialize FMOD studio system
	result = mSystem->initialize(512, // Max number of concurrent sounds
		FMOD_STUDIO_INIT_NORMAL, // Use default settings
		FMOD_INIT_NORMAL, // Use default settings
		nullptr // Usually null
	);

	if (result != FMOD_OK) {
		std::wstringstream wsstream;
		wsstream << L"Failed to initialize FMOD system: " << FMOD_ErrorString(result) << std::endl;
		return false;
	}

	// Save the low-level system pointer
	mSystem->getLowLevelSystem(&mLowLevelSystem);

	// Load the master banks (strings first)
	if (!LoadBank("./../../../../Assets/Banks/Master Bank.strings.bank"))
		return false;
	if (!LoadBank("./../../../../Assets/Banks/Master Bank.bank"))
		return false;
	if (!LoadBank("./../../../../Assets/Banks/Background.bank"))
		return false;

	return true;
}

void AudioSystem::Update(const GameTimer& gt) {
	// Find any stopped event instance
	std::vector<unsigned int> done;

	std::wstringstream wsstream;
	wsstream << mEventInstances.size() << std::endl;

	for (auto& iter : mEventInstances) {
		FMOD::Studio::EventInstance* e = iter.second;
		// Get the state of this event
		FMOD_STUDIO_PLAYBACK_STATE state;
		e->getPlaybackState(&state);
		if (state == FMOD_STUDIO_PLAYBACK_STOPPED) {
			// Release the event and add id to done
			e->release();
			done.emplace_back(iter.first);
		}
	}

	for (unsigned int id : done)
		mEventInstances.erase(id);

	// Update FMOD
	mSystem->update();
}

bool AudioSystem::LoadBank(const std::string& inFileName) {
	// Prevent double-loading
	if (mBanks.find(inFileName) != mBanks.cend())
		false;

	// Try to load bank
	FMOD::Studio::Bank* bank = nullptr;
	FMOD_RESULT result = mSystem->loadBankFile(
		inFileName.c_str(), // File name of bank
		FMOD_STUDIO_LOAD_BANK_NORMAL, // Normal loading
		&bank // Save pointer to bank
	);

	const int maxPathLength = 512;
	if (result == FMOD_OK) {
		// Add bank to map
		mBanks.emplace(inFileName, bank);
		// Load all non-streaming sample data
		bank->loadSampleData();
		// Get the number of events in this bank
		int numEvents = 0;
		bank->getEventCount(&numEvents);
		if (numEvents > 0) {
			// Get list of event description in this bank
			std::vector<FMOD::Studio::EventDescription*> events(numEvents);
			bank->getEventList(events.data(), numEvents, &numEvents);
			char eventName[maxPathLength];
			for (int i = 0; i < numEvents; i++) {
				FMOD::Studio::EventDescription* e = events[i];
				// Get the path of this event (like event:/Explision2D)
				e->getPath(eventName, maxPathLength, nullptr);
				// Add to event map
				mEvents.emplace(eventName, e);
			}
		}

		// Get the number of buses in this bank
		int numBuses = 0;
		bank->getBusCount(&numBuses);
		if (numBuses > 0) {
			// Get list of buses in this bank
			std::vector<FMOD::Studio::Bus*> buses(numBuses);
			bank->getBusList(buses.data(), numBuses, &numBuses);
			char busName[512];
			for (int i = 0; i < numBuses; i++) {
				FMOD::Studio::Bus* bus = buses[i];
				// Get the path of this bus (like bus:/SFX)
				bus->getPath(busName, 512, nullptr);
				// Add to buses map
				mBuses.emplace(busName, bus);
			}
		}
	}
	else {
		MessageBox(nullptr, L"Failed to load bank", L"FMOD Studio", MB_OK);

		return false;
	}

	return true;
}

void AudioSystem::UnloadBank(const std::string& inFileName) {
	// Ignore if not loaded
	auto iter = mBanks.find(inFileName);
	if (iter == mBanks.cend())
		return;

	// First we need to remove all events from this bank
	FMOD::Studio::Bank* bank = iter->second;
	int numEvents = 0;
	bank->getEventCount(&numEvents);
	if (numEvents > 0) {
		// Get event description for this bank
		std::vector<FMOD::Studio::EventDescription*> events(numEvents);
		// Get List of events
		bank->getEventList(events.data(), numEvents, &numEvents);
		char eventName[512];
		for (int i = 0; i < numEvents; i++) {
			FMOD::Studio::EventDescription* e = events[i];
			// Get the path of this event
			e->getPath(eventName, 512, nullptr);
			// Remove this event
			auto eventIter = mEvents.find(eventName);
			if (eventIter != mEvents.cend())
				mEvents.erase(eventIter);
		}
	}

	// Unload sample data and bank
	bank->unloadSampleData();
	bank->unload();
	// Remove from banks map
	mBanks.erase(iter);
}

void AudioSystem::UnloadAllBanks() {
	for (auto& iter : mBanks) {
		// Unload the sample data, then the bank itself
		iter.second->unloadSampleData();
		iter.second->unload();
	}
	mBanks.clear();
	// No banks means no evnets
	mEvents.clear();
}

SoundEvent AudioSystem::PlayEvent(const std::string& inEventName) {
	unsigned int retID = 0;
	auto iter = mEvents.find(inEventName);
	if (iter != mEvents.cend()) {
		// Create instance of event
		FMOD::Studio::EventInstance* event = nullptr;
		iter->second->createInstance(&event);
		if (event) {
			// Start the event instance
			event->start();
			// Get the next id, and add to map
			sNextID++;
			retID = sNextID;
			mEventInstances.emplace(retID, event);
		}
	}

	return SoundEvent(this, retID);
}

FMOD::Studio::EventInstance* AudioSystem::GetEventInstance(unsigned int inId) {
	FMOD::Studio::EventInstance* event = nullptr;
	auto iter = mEventInstances.find(inId);
	if (iter != mEventInstances.cend())
		event = iter->second;
	return event;
}

namespace {
	FMOD_VECTOR VecToFMOD(const XMFLOAT3& in) {
		// Convert from our coordinates (+x forward, +y right, +z up)
		// to FMOD (+z forward, +x right, +y up)
		FMOD_VECTOR v;
		v.x = in.y;
		v.y = in.z;
		v.z = in.x;
		return v;
	}
}

void AudioSystem::SetListener(const XMFLOAT4X4& inViewMatrix) {
	// Invert the view matrix to get the correct vectors
	XMMATRIX invViewMatrix = XMLoadFloat4x4(&inViewMatrix);
	XMMatrixInverse(&XMMatrixDeterminant(invViewMatrix), invViewMatrix);
	XMFLOAT4X4 invView;
	XMStoreFloat4x4(&invView, invViewMatrix);

	FMOD_3D_ATTRIBUTES listener;
	// Set the position, forward, up
	listener.position = VecToFMOD({ invView(3, 0), invView(3, 1), invView(3, 2) });
	// In the inverted view, third row is forward
	listener.forward = VecToFMOD({ invView(2, 0), invView(2, 1), invView(2, 2) });
	// In the inverted view, second row is up
	listener.up = VecToFMOD({ invView(1, 0), invView(1, 1), invView(1, 2) });
	// Set velocity to zero (fix if using Doppler effect)
	listener.velocity = { 0.0f, 0.0f, 0.0f };
	// Send to FMOD
	mSystem->setListenerAttributes(0, &listener);
}

float AudioSystem::GetBusVolume(const std::string& inName) const {
	float retVal = 0.0f;
	auto iter = mBuses.find(inName);
	if (iter != mBuses.cend())
		iter->second->getVolume(&retVal);

	return retVal;
}

bool AudioSystem::GetBusPaused(const std::string& inName) const {
	bool retVal = false;
	auto iter = mBuses.find(inName);
	if (iter != mBuses.cend())
		iter->second->getPaused(&retVal);

	return retVal;
}

void AudioSystem::SetBusVolume(const std::string& inName, float inVolume) {
	auto iter = mBuses.find(inName);
	if (iter != mBuses.end())
		iter->second->setVolume(inVolume);
}

void AudioSystem::SetBusPaused(const std::string& inName, bool inPause) {
	auto iter = mBuses.find(inName);
	if (iter != mBuses.end())
		iter->second->setPaused(inPause);
}