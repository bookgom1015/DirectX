#include "DX12Game/Renderer.h"
#include "DX12Game/GameCamera.h"
#include "DX12Game/Mesh.h"

using namespace DirectX;
using namespace DirectX::PackedVector;

GameCamera* Renderer::GetMainCamera() const {
	return mMainCamera;
}

void Renderer::SetMainCamerea(GameCamera* inCamera) {
	mMainCamera = inCamera;
}

bool Renderer::IsValid() const {
	return bIsValid;
}

void Renderer::AddOutputText(const std::string& inName, const std::wstring& inText,
	float inX /* = 0.0f */, float inY /* = 0.0f */, float inScale /* = 1.0f */, float inLifeTime /* = -1.0f */) {

	mOutputTexts[inName].first = inText;
	mOutputTexts[inName].second = SimpleMath::Vector4(inX, inY, inScale, inLifeTime);
}

void Renderer::RemoveOutputText(const std::string& inName) {
	mOutputTexts.erase(inName);
}