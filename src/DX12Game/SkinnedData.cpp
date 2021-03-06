#include "DX12Game/SkinnedData.h"

using namespace DirectX;
using namespace DirectX::PackedVector;

Game::Bone::Bone(const std::string inName, int inParentIndex,
			const DirectX::XMFLOAT4X4& inLocalBindPose,
			const DirectX::XMFLOAT4X4& inGlobalBindPose,
			const DirectX::XMFLOAT4X4& inGlobalInvBindPose) {
	Name = inName;
	ParentIndex = inParentIndex;
	LocalBindPose = inLocalBindPose;
	GlobalBindPose = inGlobalBindPose;
	GlobalInvBindPose = inGlobalInvBindPose;
}

float Game::SkinnedData::GetTimePosition(const std::string& inClipName, float inTime) const{
	auto animtIter = mAnimations.find(inClipName);
	if (animtIter == mAnimations.cend())
		return 0.0f;

	const auto& anim = animtIter->second;
	float animTime = inTime;
	while (animTime >= anim.mDuration)
		animTime -= anim.mDuration;

	float frameTime = static_cast<float>(animTime / anim.mFrameDuration);
	size_t frame = static_cast<size_t>(frameTime);
	float pct = frameTime - static_cast<float>(frame);

	return static_cast<float>(frame + pct);
}