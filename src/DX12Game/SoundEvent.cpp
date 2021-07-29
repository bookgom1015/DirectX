#include "DX12Game/SoundEvent.h"
#include "DX12Game/AudioSystem.h"

#include <fmod_studio.hpp>

using namespace DirectX;
using namespace DirectX::PackedVector;

SoundEvent::SoundEvent(AudioSystem* inSystem, unsigned int inId)
	: mSystem(inSystem), mID(inId) {}

SoundEvent::SoundEvent()
	: mSystem(nullptr), mID(0) {}

bool SoundEvent::IsValid() {
	return (mSystem && mSystem->GetEventInstance(mID) != nullptr);
	return false;
}

void SoundEvent::Restart() {
	FMOD::Studio::EventInstance* event = mSystem ? mSystem->GetEventInstance(mID) : nullptr;
	if (event)
		event->start();
}

void SoundEvent::Stop(bool inAllowFadeOut) {
	FMOD::Studio::EventInstance* event = mSystem ? mSystem->GetEventInstance(mID) : nullptr;
	if (event) {
		FMOD_STUDIO_STOP_MODE mode = inAllowFadeOut ?
			FMOD_STUDIO_STOP_ALLOWFADEOUT : FMOD_STUDIO_STOP_IMMEDIATE;
		event->stop(mode);
	}
}

bool SoundEvent::GetPaused() const {
	bool retVal = false;
	FMOD::Studio::EventInstance* event = mSystem ? mSystem->GetEventInstance(mID) : nullptr;
	if (event)
		event->getPaused(&retVal);
	return retVal;
}

void SoundEvent::SetPaused(bool inPause) {
	FMOD::Studio::EventInstance* event = mSystem ? mSystem->GetEventInstance(mID) : nullptr;
	if (event)
		event->setPaused(inPause);
}

float SoundEvent::GetVolume() const {
	float retVal = 0.0f;
	FMOD::Studio::EventInstance* event = mSystem ? mSystem->GetEventInstance(mID) : nullptr;
	if (event)
		event->getVolume(&retVal);
	return retVal;
}

void SoundEvent::SetVolume(float inVolume) {
	FMOD::Studio::EventInstance* event = mSystem ? mSystem->GetEventInstance(mID) : nullptr;
	if (event)
		event->setVolume(inVolume);
}

float SoundEvent::GetPitch() const {
	float retVal = 0.0f;
	FMOD::Studio::EventInstance* event = mSystem ? mSystem->GetEventInstance(mID) : nullptr;
	if (event)
		event->getPitch(&retVal);
	return retVal;
}

void SoundEvent::SetPitch(float inPitch) {
	FMOD::Studio::EventInstance* event = mSystem ? mSystem->GetEventInstance(mID) : nullptr;
	if (event)
		event->setPitch(inPitch);
}

float SoundEvent::GetParameter(const std::string& inParamName) {
	float retVal = 0.0f;
	FMOD::Studio::EventInstance* event = mSystem ? mSystem->GetEventInstance(mID) : nullptr;
	if (event)
		event->getParameterByName(inParamName.c_str(), &retVal);
	return retVal;
}

void SoundEvent::SetParameter(const std::string& inParamName, float inValue) {
	FMOD::Studio::EventInstance* event = mSystem ? mSystem->GetEventInstance(mID) : nullptr;
	if (event)
		event->setParameterByName(inParamName.c_str(), inValue);
}

bool SoundEvent::Is3D() const {
	bool retVal = false;
	FMOD::Studio::EventInstance* event = mSystem ? mSystem->GetEventInstance(mID) : nullptr;
	if (event) {
		// Get the event description
		FMOD::Studio::EventDescription* ed = nullptr;
		event->getDescription(&ed);
		if (ed)
			ed->is3D(&retVal);
	}
	return retVal;
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

void SoundEvent::Set3DAttributes(const XMFLOAT4X4& inWorldTransform) {
	FMOD::Studio::EventInstance* event = mSystem ? mSystem->GetEventInstance(mID) : nullptr;
	if (event) {
		FMOD_3D_ATTRIBUTES attr;
		// Set position, forward, up
		attr.position = VecToFMOD({ inWorldTransform(3, 0), inWorldTransform(3, 1), inWorldTransform(3, 2) });
		// In world transform, firt row is forward
		attr.forward = VecToFMOD({ inWorldTransform(0, 0), inWorldTransform(0, 1), inWorldTransform(0, 2) });
		// Third row is up
		attr.up = VecToFMOD({ inWorldTransform(2, 0), inWorldTransform(2, 1), inWorldTransform(2, 2) });
		// Set velocity to zero (fix if using Doppler effect)
		attr.velocity = { 0.0f, 0.0f, 0.0f };
		event->set3DAttributes(&attr);
	}
}