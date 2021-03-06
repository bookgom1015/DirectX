#pragma once

#pragma comment(lib, "libfbxsdk.lib")

#include <DirectXMath.h>
#include <DirectXPackedVector.h>

#define FBXSDK_SHARED
#include <fbxsdk.h>
#include <unordered_map>
#include <vector>

#define NOMINMAX
#include <Windows.h>

#include "common/MathHelper.h"

namespace Game {
	struct FbxVertex;
	struct FbxMaterial;
	struct FbxBone;
	class FbxSkeleton;
	class FbxAnimation;
	class FbxImporter;
}

struct Game::FbxVertex {
public:
	DirectX::XMFLOAT3 mPos;
	DirectX::XMFLOAT3 mNormal;
	DirectX::XMFLOAT2 mTexC;
	DirectX::XMFLOAT3 mTangentU;
	float mBoneWeights[8];
	int mBoneIndices[8];

public:
	FbxVertex(
		DirectX::XMFLOAT3 inPos = { 0.0f, 0.0f, 0.0f },
		DirectX::XMFLOAT3 inNormal = { 0.0f, 0.0f, 0.0f },
		DirectX::XMFLOAT2 inTexC = { 0.0f, 0.0f },
		DirectX::XMFLOAT3 inTangentU = { 0.0f, 0.0f, 0.0f }
	);

public:
	friend bool operator==(const Game::FbxVertex& lhs, const Game::FbxVertex& rhs);
};

namespace std {
	template<> struct hash<Game::FbxVertex> {
		size_t operator()(Game::FbxVertex const& vertex) const {
			size_t pos = static_cast<size_t>(vertex.mPos.x + vertex.mPos.y + vertex.mPos.z);
			size_t normal = static_cast<size_t>(vertex.mNormal.x + vertex.mNormal.y + vertex.mNormal.z);
			size_t texC = static_cast<size_t>(vertex.mTexC.x + vertex.mTexC.y);
			size_t tan = static_cast<size_t>(vertex.mTangentU.x + vertex.mTangentU.y + vertex.mTangentU.z);

			return ((pos ^ normal) << 1) ^ ((texC ^ tan) << 2);
		}
	};
}

struct Game::FbxMaterial {
public:
	std::string mMaterialName;
	std::string mDiffuseMapFileName;
	std::string mNormalMapFileName;
	std::string mSpecularMapFileName;
	std::string mAlphaMapFileName;

	DirectX::XMFLOAT4X4 mMatTransform;
	DirectX::XMFLOAT4 mDiffuseAlbedo;
	DirectX::XMFLOAT3 mFresnelR0;
	float mRoughness;

public:
	FbxMaterial();
};

struct Game::FbxBone {
public:
	std::string mName;
	int mParentIndex;

	DirectX::XMFLOAT4X4 mLocalBindPose;
	DirectX::XMFLOAT4X4 mGlobalBindPose;
	DirectX::XMFLOAT4X4 mGlobalInvBindPose;

	fbxsdk::FbxAMatrix mFbxLocalBindPose;
	fbxsdk::FbxAMatrix mFbxGlobalBindPose;
	fbxsdk::FbxAMatrix mFbxGlobalInvBindPose;

public:
	FbxBone();
};

class Game::FbxSkeleton {
public:
	FbxSkeleton() = default;
	virtual ~FbxSkeleton() = default;

public:
	const std::vector<FbxBone>& GetBones() const;
	size_t GetNumBones() const;

private:
	friend class Game::FbxImporter;

	std::vector<Game::FbxBone> mBones;
};

class Game::FbxAnimation {
public:
	FbxAnimation() = default;
	virtual ~FbxAnimation() = default;

public:
	size_t GetNumFrames() const;
	float GetDuration() const;
	float GetFrameDuration() const;
	const std::unordered_map<UINT, std::vector<DirectX::XMFLOAT4X4>>& GetCurves() const;

private:
	friend class Game::FbxImporter;

	// Number of frames in the animation
	size_t mNumFrames = 0;
	// Duration of the animation in seconds
	float mDuration = 0.0f;
	// Duration of each frame in the animation
	float mFrameDuration = 0.0f;

	std::unordered_map<UINT /* Bone index */, std::vector<DirectX::XMFLOAT4X4>> mCurves;
	std::unordered_map<int /* Parent Bone index */, std::vector<fbxsdk::FbxAMatrix>> mParentGlobalTransforms;
};

class Game::FbxImporter {
private:
	struct BoneIndexWeight {
		int mBoneIndex = -1;
		float mWeight = 0.0f;

		friend bool operator==(const BoneIndexWeight& lhs, const BoneIndexWeight& rhs) {
			return lhs.mBoneIndex == rhs.mBoneIndex && MathHelper::IsEqual(lhs.mWeight, rhs.mWeight);
		}
	};

public:
	FbxImporter();
	virtual ~FbxImporter();

private:
	FbxImporter(const Game::FbxImporter& src) = delete;
	FbxImporter(Game::FbxImporter&& src) = delete;
	Game::FbxImporter& operator=(const Game::FbxImporter& rhs) = delete;
	Game::FbxImporter& operator=(Game::FbxImporter&& rhs) = delete;

public:
	bool LoadDataFromFile(const std::string& inFileName);

	const std::vector<Game::FbxVertex>& GetVertices() const;
	const std::vector<std::uint32_t>& GetIndices() const;
	const std::vector<std::string>& GetSubsetNames() const;
	const std::vector<std::pair<UINT, UINT>>& GetSubsets() const;
	const std::unordered_map<std::string, Game::FbxMaterial>& GetMaterials() const;
	const Game::FbxSkeleton& GetSkeleton() const;
	const std::unordered_map<std::string, Game::FbxAnimation>& GetAnimations() const;

private:
	//* Initializes fbx sdk(The app crashes when triangulating geometry).
	bool LoadFbxScene(const std::string& inFileName);

	int LoadDataFromMesh(fbxsdk::FbxNode* inNode, UINT inPrevNumVertices);
	//* Multi threaded version of the fuction LoadMesh.
	int MTLoadDataFromMesh(fbxsdk::FbxNode* inNode, UINT inPrevNumVertices);

	void LoadMaterials(fbxsdk::FbxNode* inNode);

	//* Generates skeleton hierarchy data that is depth-first.
	void LoadSkeletonHierarchy(fbxsdk::FbxNode* inRootNode);
	//* Finds skeleton node and generate hierarchy data recursively(depth-first).
	void LoadSkeletonHierarchyRecurcively(fbxsdk::FbxNode* inNode, int inDepth, int inMyIndex, int inParentIndex);

	//* Generates pose matrix data for each bone(like a global [inverse] bind pose)
	//   and weight data for each bone.
	bool LoadBones(fbxsdk::FbxNode* inNode);

	//* Normalizes the weight for the bones in each control point.
	void NormalizeWeigths();
	//* Moves matrix data in DxFbxBone from fbxsdk::FbxAMatrix to DirectX::XMFLOAT4X4.
	void MoveDataFromFbxAMatrixToDirectXMath();
	//* Returns bone index in unordered_map of DxFbxSkeleton.
	UINT FindBoneIndexUsingName(const std::string& inBoneName);

	//* Build local, global and global inverse bind pose for each bone in skeleton.
	void BuildBindPoseData(fbxsdk::FbxCluster* inCluster, const fbxsdk::FbxAMatrix& inGeometryTransform,
		UINT inClusterIndex, int inParentIndex, const Game::FbxSkeleton& inSkeleton, Game::FbxBone& outBone);
	//* Builds weigths for each control point.
	//* Each cluster has control points that is affected by it.
	//* So the app need to iterate all the meshes that is existed and extract clusters affecting the mesh.
	void BuildControlPointsWeigths(fbxsdk::FbxCluster* inCluster, UINT inClusterIndex, const std::string& inMeshName);
	//* Loads FbxAnimLayer-set in FbxAnimStack-set
	//   and FbxAnimCurve-set in FbxAnimLayer-set.
	//* After loads FbxAnimCurve-set, calls the function BuildAnimationKeyFrames
	//   to build global transform(to-root) matrix at each frame.
	void BuildAnimations(fbxsdk::FbxNode* inNode, fbxsdk::FbxCluster* inCluster,
		const fbxsdk::FbxAMatrix& inGeometryTransform, UINT inClusterIndex, int inParentIndex);
	//* Helper class for the function BuildAnimations.
	//* Loads TRS-components in FbxAnimCurve at each frame to build global transform(to-root).
	void BuildAnimationKeyFrames(fbxsdk::FbxAnimLayer* inAnimLayer, fbxsdk::FbxNode* inNode, fbxsdk::FbxCluster* inCluster,
		const fbxsdk::FbxAMatrix& inGeometryTransform, fbxsdk::FbxTakeInfo* inTakeInfo,
		Game::FbxAnimation& outAnimation, UINT inClusterIndex, int inParentIndex);
	//* Deprecated.
	void BuildAnimationKeyFrames(fbxsdk::FbxTakeInfo* inTakeInfo, fbxsdk::FbxCluster* inCluster, fbxsdk::FbxNode* inNode,
		fbxsdk::FbxAMatrix inGeometryTransform, Game::FbxAnimation& outAnimation,
		UINT inClusterIndex, int inParentIndex);

private:
	fbxsdk::FbxManager* mFbxManager;
	fbxsdk::FbxIOSettings* mFbxIos;
	fbxsdk::FbxScene* mFbxScene;

	std::unordered_map<FbxVertex, std::uint32_t> mUniqueVertices;
	std::vector<FbxVertex> mVertices;
	std::vector<std::uint32_t> mIndices;

	std::vector<std::string> mSubsetNames;
	std::vector<std::pair<UINT /* Index count */, UINT /* Start index */>> mSubsets;

	std::unordered_map<std::string /* Submesh name */, Game::FbxMaterial> mMaterials;

	Game::FbxSkeleton mSkeleton;

	std::unordered_map<std::string /* Submesh name */,
		std::unordered_map<int /* Control point index */,
		std::vector<BoneIndexWeight> /* Max size: 8 */>> mControlPointsWeights;

	std::unordered_map<std::string /* Clip name */, Game::FbxAnimation> mAnimations;

	std::unordered_map<UINT /* Bone index */, fbxsdk::FbxCluster*> mClusters;
	std::vector<UINT /* BOne index */> mNestedClusters;
};