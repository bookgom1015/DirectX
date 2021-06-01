#pragma once

#include "DX12Game/Component.h"

class Actor;
class Mesh;
class Renderer;

class MeshComponent : public Component {
public:
	MeshComponent(Actor* inOwnerActor, int inUpdateOrder = 100);
	virtual ~MeshComponent() = default;

public:
	//* Called when world transform changes.
	virtual void OnUpdateWorldTransform() override;

	virtual bool LoadMesh(const std::string& inMeshName, const std::string& inFileName, bool bMultiThreading = false);

	//* Set visibility for the mesh for this component.
	virtual void SetVisible(bool inStatus);

protected:
	Renderer* mRenderer;
	Mesh* mMesh = nullptr;

	std::string mMeshName;

	std::thread mMeshLoadThread;
	std::promise<Mesh*> mMeshPromise;
	std::future<Mesh*> mMeshFuture;

	bool mIsSkeletal;
};