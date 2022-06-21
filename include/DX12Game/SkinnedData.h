#pragma once

#include "DX12Game/GameCore.h"

namespace Game {
	struct Bone;
	class Skeleton;
	class Animation;
	class SkinnedData;
}

struct Game::Bone {
public:
	std::string Name;
	int ParentIndex = -1;
	DirectX::XMFLOAT4X4 LocalBindPose = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 GlobalBindPose = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 GlobalInvBindPose = MathHelper::Identity4x4();

public:
	Bone() = default;
	Bone(const std::string inName, int inParentIndex,
		const DirectX::XMFLOAT4X4& inLocalBindPose,
		const DirectX::XMFLOAT4X4& inGlobalBindPose,
		const DirectX::XMFLOAT4X4& inGlobalInvBindPose);
};

class Game::Skeleton {
public:
	Skeleton() = default;
	virtual ~Skeleton() = default;

public:
	// The bones in the skeleton.
	std::vector<Game::Bone> mBones;
};

class Game::Animation {
public:
	Animation() = default;
	virtual ~Animation() = default;

public:
	// Number of bones for the animation.
	//size_t mNumBones;
	// Number of frames in the animation.
	size_t mNumFrames;
	// Duration of the animation in seconds.
	float mDuration;
	// Duration of each frame in the animation.
	float mFrameDuration;

	std::vector<std::vector<DirectX::XMFLOAT4X4>> mCurves;
};

class Game::SkinnedData {
public:
	SkinnedData() = default;
	virtual ~SkinnedData() = default;

public:
	float GetTimePosition(const std::string& inClipName, float inTime) const;

public:
	Game::Skeleton mSkeleton;

	std::unordered_map<std::string /* Clip name */, Game::Animation> mAnimations;
};