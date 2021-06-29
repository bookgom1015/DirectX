#pragma once

struct Bone {
	Bone() = default;
	Bone(const std::string inName, int inParentIndex,
		const DirectX::XMFLOAT4X4& inLocalBindPose,
		const DirectX::XMFLOAT4X4& inGlobalBindPose,
		const DirectX::XMFLOAT4X4& inGlobalInvBindPose);

	std::string Name;
	int ParentIndex = -1;
	DirectX::XMFLOAT4X4 LocalBindPose = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 GlobalBindPose = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 GlobalInvBindPose = MathHelper::Identity4x4();
};

class Skeleton {
public:
	Skeleton() = default;
	virtual ~Skeleton() = default;

public:
	// The bones in the skeleton.
	GVector<Bone> mBones;
};

class Animation {
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

	GVector<GVector<DirectX::XMFLOAT4X4>> mCurves;
};

class SkinnedData {
public:
	SkinnedData() = default;
	virtual ~SkinnedData() = default;

public:
	float GetTimePosition(const std::string& inClipName, float inTime) const;

public:
	Skeleton mSkeleton;

	GUnorderedMap<std::string /* Clip name */, Animation> mAnimations;
};