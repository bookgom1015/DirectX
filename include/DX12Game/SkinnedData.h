#pragma once

struct Bone {
	Bone() = default;
	Bone(const std::string inName, int inParentIndex,
		const DirectX::XMFLOAT4X4& inLocalBindPose,
		const DirectX::XMFLOAT4X4& inGlobalBindPose,
		const DirectX::XMFLOAT4X4& inGlobalInvBindPose);

	std::string mName;
	int mParentIndex = -1;

	DirectX::XMFLOAT4X4 mLocalBindPose = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 mGlobalBindPose = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 mGlobalInvBindPose = MathHelper::Identity4x4();
};

class Skeleton {
public:
	Skeleton() = default;
	virtual ~Skeleton() = default;

public:
	// The bones in the skeleton.
	std::vector<Bone> mBones;
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

	//std::unordered_map<UINT /* Bone index */, std::vector<DirectX::XMFLOAT4X4>> mCurves;
	std::vector<std::vector<DirectX::XMFLOAT4X4>> mCurves;
};

class SkinnedData {
public:
	SkinnedData() = default;
	virtual ~SkinnedData() = default;

public:
	void GetFinalTransforms(const std::string& inClipName, float inTimePos, 
							std::vector<DirectX::XMFLOAT4X4>& outFinalTransforms) const;
	float GetTimePosition(const std::string& inClipName, float inTime) const;

public:
	Skeleton mSkeleton;

	std::unordered_map<std::string /* Clip name */, Animation> mAnimations;
};