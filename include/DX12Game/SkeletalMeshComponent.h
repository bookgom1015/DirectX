#pragma once

#include "MeshComponent.h"

class Actor;

class SkeletalMeshComponent : public MeshComponent {
public:
	SkeletalMeshComponent(Actor* inOwnerActor);
	virtual ~SkeletalMeshComponent() = default;

public:
	//* Called when world transform changes.
	virtual void OnUpdateWorldTransform() override;

	//* Update this component by delta time.
	virtual void Update(const GameTimer& gt) override;

	bool LoadSkeletalMesh(const std::string& inMeshName, const std::string& inFileName, bool inNeedToBeAligned = false);
	bool MTLoadSkeletalMesh(const std::string& inMeshName, const std::string& inFileName, bool inNeedToBeAligned = false);

	//* Set clip name that mesh will animate.
	void SetClipName(const std::string& inClipName);
	//* Set visibility for this mesh and it's skeleton.
	virtual void SetVisible(bool inState) override;
	//* Set visibility for skeleton of this mesh.
	void SetSkeleletonVisible(bool inState);

private:
	std::vector<DirectX::XMFLOAT4X4> mBoneTransforms;

	std::string mClipName = "";

	float mLastTotalTime = 0.0f;
	bool mClipIsChanged = false;
};