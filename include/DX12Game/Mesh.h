#pragma once

#include "DX12Game/SkinnedData.h"

class Renderer;

struct Vertex;
struct SkinnedVertex;
struct MaterialIn;

class Mesh {
public:
	Mesh(bool inIsSkeletal = false, bool inNeedToBeAligned = false);
	virtual ~Mesh() = default;

public:
	virtual bool Load(const std::string& inFileName, bool bMultiThreading = false);

	const std::string& GetMeshName() const;
	const GVector<std::string>& GetDrawArgs() const;

	const GVector<Vertex>& GetVertices() const;
	const GVector<SkinnedVertex>& GetSkinnedVertices() const;
	const GVector<std::uint32_t>& GetIndices() const;

	const GVector<std::pair<UINT, UINT>>& GetSubsets() const;
	const GUnorderedMap<std::string, MaterialIn>& GetMaterials() const;

	const SkinnedData& GetSkinnedData() const;

	bool GetIsSkeletal() const;

	const GVector<Vertex>& GetSkeletonVertices() const;
	const GVector<std::uint32_t> GetSkeletonIndices() const;

	UINT GetClipIndex(const std::string& inClipName) const;

private:
	//* Generates vertices and indices for the skeleton.
	//* It's organized as line-lists.
	void GenerateSkeletonData();
	//* Helper class for the fuction GenerateSkeletonData.
	void AddSkeletonVertex(const Vertex& inVertex);

	//* Outputs information consisting of text for skinned data to ./log.txt
	void OutputSkinnedDataInfo();

protected:
	Renderer* mRenderer;

	std::string mMeshName;
	GVector<std::string> mDrawArgs;

	GVector<Vertex> mVertices;
	GVector<SkinnedVertex> mSkinnedVertices;
	GVector<std::uint32_t> mIndices;

	GVector<std::pair<UINT /* Index count */, UINT /* Start index */>> mSubsets;

	GUnorderedMap<std::string /* Geometry name */, MaterialIn> mMaterials;

	SkinnedData mSkinnedData;

	bool mIsSkeletal;
	bool mNeedToBeAligned;

	GVector<Vertex> mSkeletonVertices;
	GVector<std::uint32_t> mSkeletonIndices;

	GUnorderedMap<std::string /* Clip name */, UINT /* Index */> mClipsIndex;
};