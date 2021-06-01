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
	const std::vector<std::string>& GetDrawArgs() const;

	const std::vector<Vertex>& GetVertices() const;
	const std::vector<SkinnedVertex>& GetSkinnedVertices() const;
	const std::vector<std::uint32_t>& GetIndices() const;

	const std::vector<std::pair<UINT, UINT>>& GetSubsets() const;
	const std::unordered_map<std::string, MaterialIn>& GetMaterials() const;

	const SkinnedData& GetSkinnedData() const;

	bool GetIsSkeletal() const;

	const std::vector<Vertex>& GetSkeletonVertices() const;
	const std::vector<std::uint32_t> GetSkeletonIndices() const;

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
	std::vector<std::string> mDrawArgs;

	std::vector<Vertex> mVertices;
	std::vector<SkinnedVertex> mSkinnedVertices;
	std::vector<std::uint32_t> mIndices;

	std::vector<std::pair<UINT /* Index count */, UINT /* Start index */>> mSubsets;

	std::unordered_map<std::string /* Geometry name */, MaterialIn> mMaterials;

	SkinnedData mSkinnedData;

	bool mIsSkeletal;
	bool mNeedToBeAligned;

	std::vector<Vertex> mSkeletonVertices;
	std::vector<std::uint32_t> mSkeletonIndices;

	std::unordered_map<std::string /* Clip name */, UINT /* Index */> mClipsIndex;
};