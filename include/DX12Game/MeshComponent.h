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
	//* Loads mesh data(vertices, indices, uv coordinaes...).
	bool LoadMesh(const std::string& inMeshName, const std::string& inFileName);
	//* Multi-threaded version of the function LoadMesh.
	bool MTLoadMesh(const std::string& inMeshName, const std::string& inFileName);

	//* Set visibility for the mesh for this component.
	virtual void SetVisible(bool inStatus);

protected:
	//* Helper function for loading mesh.
	bool ProcessLoadingMesh(const std::string& inMeshName, const std::string& inFileName,
							bool inIsSkeletal = false, bool inNeedToBeAligned = false);
	//* Multi-thraded version of the function ProcessMeshLoading.
	bool MTProcessLoadingMesh(const std::string& inMeshName, const std::string& inFileName,
							bool inIsSkeletal = false, bool inNeedToBeAligned = false);

protected:
	Renderer* mRenderer;
	Mesh* mMesh = nullptr;

	std::string mMeshName = "Mesh";

	std::thread mMeshLoadThread;
	std::promise<Mesh*> mMeshPromise;
	std::future<Mesh*> mMeshFuture;

	bool mIsSkeletal;
};