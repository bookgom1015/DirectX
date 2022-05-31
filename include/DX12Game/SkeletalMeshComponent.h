#pragma once

#include "DX12Game/MeshComponent.h"

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
	virtual GameResult LoadMesh(const std::string& inMeshName, const std::string& inFileName) override;

	void SetClipName(const std::string& inClipName);
	virtual void SetVisible(bool inState) override;
	void SetSkeleletonVisible(bool inState);

private:
	std::vector<DirectX::XMFLOAT4X4> mBoneTransforms;

	std::string mClipName;

	float mLastTotalTime;
	bool mClipIsChanged;
};