#include "DX12Game/Renderer.h"
#include "DX12Game/GameCamera.h"
#include "DX12Game/Mesh.h"

using namespace DirectX;
using namespace DirectX::PackedVector;

void Renderer::AddOutputText(const std::string& inName, const std::wstring& inText,
	float inX /* = 0.0f */, float inY /* = 0.0f */, float inScale /* = 1.0f */, float inLifeTime /* = -1.0f */) {

	mOutputTexts[inName].first = inText;
	mOutputTexts[inName].second = SimpleMath::Vector4(inX, inY, inScale, inLifeTime);
}

void Renderer::RemoveOutputText(const std::string& inName) {
	mOutputTexts.erase(inName);
}

GameCamera* Renderer::GetMainCamera() const {
	return mMainCamera;
}

void Renderer::SetMainCamerea(GameCamera* inCamera) {
	mMainCamera = inCamera;
}

bool Renderer::IsValid() const {
	return bIsValid;
}

bool Renderer::GetSsaoEnabled() const {
	return (mEffectEnabled & EffectEnabled::ESsao) == EffectEnabled::ESsao;
}

void Renderer::SetSsaoEnabled(bool bState) {
	if (bState)
		mEffectEnabled |= EffectEnabled::ESsao;
	else
		mEffectEnabled &= ~EffectEnabled::ESsao;
}

bool Renderer::GetSsrEnabled() const {
	return (mEffectEnabled & EffectEnabled::ESsr) == EffectEnabled::ESsr;
}

void Renderer::SetSsrEnabled(bool bState) {
	if (bState)
		mEffectEnabled |= EffectEnabled::ESsr;
	else
		mEffectEnabled &= ~EffectEnabled::ESsr;
}

bool Renderer::GetBloomEnabled() const {
	return (mEffectEnabled & EffectEnabled::EBloom) == EffectEnabled::EBloom;
}

void Renderer::SetBloomEnabled(bool bState) {
	if (bState)
		mEffectEnabled |= EffectEnabled::EBloom;
	else
		mEffectEnabled &= ~EffectEnabled::EBloom;
}

bool Renderer::GetDrawDebugSkeletonsEnabled() const {
	return bDrawDebugSkeletonsEnabled;
}

void Renderer::SetDrawDebugSkeletonsEnabled(bool bState) {
	bDrawDebugSkeletonsEnabled = bState;
}

bool Renderer::GetDrawDebugWindowsEnabled() const {
	return bDrawDeubgWindowsEnabled;
}

void Renderer::SetDrawDebugWindowsEnabled(bool bState) {
	bDrawDeubgWindowsEnabled = bState;
}