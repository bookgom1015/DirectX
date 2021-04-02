#include "DX12Game/GameCore.h"
#include "DX12Game/SkinnedData.h"

using namespace DirectX;
using namespace DirectX::PackedVector;

Bone::Bone(const std::string inName, int inParentIndex,
			const DirectX::XMFLOAT4X4& inLocalBindPose,
			const DirectX::XMFLOAT4X4& inGlobalBindPose,
			const DirectX::XMFLOAT4X4& inGlobalInvBindPose) {
	mName = inName;
	mParentIndex = inParentIndex;
	mLocalBindPose = inLocalBindPose;
	mGlobalBindPose = inGlobalBindPose;
	mGlobalInvBindPose = inGlobalInvBindPose;
}

void SkinnedData::GetFinalTransforms(const std::string& inClipName, float inTimePos,
										std::vector<DirectX::XMFLOAT4X4>& outFinalTransforms) const {
	auto animtIter = mAnimations.find(inClipName);
	if (animtIter != mAnimations.cend()) {
		const auto& anim = animtIter->second;
		float animTime = inTimePos;
		while (animTime >= anim.mDuration)
			animTime -= anim.mDuration;

		float frameTime = static_cast<float>(animTime / anim.mFrameDuration);
		size_t frame = (size_t)frameTime;
		size_t nextFrame = frame + 1;
		if (nextFrame >= anim.mNumFrames)
			nextFrame = 0;
		float pct = frameTime - (float)frame;

		for (const auto& curve : anim.mCurves) {
			int index = curve.first;

			XMMATRIX currentTransform = XMLoadFloat4x4(&curve.second[frame]);
			XMMATRIX nextTransform = XMLoadFloat4x4(&curve.second[nextFrame]);

			XMMATRIX interpolatedTransform = currentTransform * (1.0f - pct) + nextTransform * pct;
			XMStoreFloat4x4(&outFinalTransforms[index], interpolatedTransform);
		}
	}
}