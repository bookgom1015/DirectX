#pragma once

#pragma comment(lib, "libfbxsdk.lib")

#define FBXSDK_SHARED
#include <fbxsdk.h>

#include "DX12Game/GameCore.h"

struct DxFbxVertex {
public:
	DirectX::XMFLOAT3 mPos;
	DirectX::XMFLOAT3 mNormal;
	DirectX::XMFLOAT2 mTexC;
	DirectX::XMFLOAT3 mTangentU;
	float mBoneWeights[8];
	int mBoneIndices[8];

public:
	DxFbxVertex(
		DirectX::XMFLOAT3 inPos			= { 0.0f, 0.0f, 0.0f },
		DirectX::XMFLOAT3 inNormal		= { 0.0f, 0.0f, 0.0f },
		DirectX::XMFLOAT2 inTexC		= { 0.0f, 0.0f },
		DirectX::XMFLOAT3 inTangentU	= { 0.0f, 0.0f, 0.0f }
	);

public:
	friend bool operator==(const DxFbxVertex& lhs, const DxFbxVertex& rhs);
};

struct DxFbxMaterial {
public:
	std::string mMaterialName;
	std::string mDiffuseMapFileName;
	std::string mNormalMapFileName;
	std::string mSpecularMapFileName;

	DirectX::XMFLOAT4X4 mMatTransform;
	DirectX::XMFLOAT4 mDiffuseAlbedo;
	DirectX::XMFLOAT3 mFresnelR0;
	float mRoughness;

public:
	DxFbxMaterial();
};

struct DxFbxBone {
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
	DxFbxBone();
};

class DxFbxSkeleton {
public:
	DxFbxSkeleton() = default;
	virtual ~DxFbxSkeleton() = default;

public:
	const GVector<DxFbxBone>& GetBones() const;
	size_t GetNumBones() const;

private:
	friend class DxFbxImporter;

	GVector<DxFbxBone> mBones;
};

class DxFbxAnimation {
public:
	DxFbxAnimation() = default;
	virtual ~DxFbxAnimation() = default;

public:
	size_t GetNumFrames() const;
	float GetDuration() const;
	float GetFrameDuration() const;
	const GUnorderedMap<UINT, GVector<DirectX::XMFLOAT4X4>>& GetCurves() const;

private:
	friend class DxFbxImporter;

	// Number of frames in the animation
	size_t mNumFrames = 0;
	// Duration of the animation in seconds
	float mDuration = 0.0f;
	// Duration of each frame in the animation
	float mFrameDuration = 0.0f;

	GUnorderedMap<UINT /* Bone index */, GVector<DirectX::XMFLOAT4X4>> mCurves;
	GUnorderedMap<int /* Parent Bone index */, GVector<fbxsdk::FbxAMatrix>> mParentGlobalTransforms;
};

class DxFbxImporter {
private:
	struct BoneIndexWeight {
		int mBoneIndex = -1;
		float mWeight = 0.0f;

		friend bool operator==(const BoneIndexWeight& lhs, const BoneIndexWeight& rhs) {
			return lhs.mBoneIndex == rhs.mBoneIndex && MathHelper::IsEqual(lhs.mWeight, rhs.mWeight);
		}
	};

public:
	DxFbxImporter();
	virtual ~DxFbxImporter();

private:
	DxFbxImporter(const DxFbxImporter& src) = delete;
	DxFbxImporter(DxFbxImporter&& src) = delete;
	DxFbxImporter& operator=(const DxFbxImporter& rhs) = delete;
	DxFbxImporter& operator=(DxFbxImporter&& rhs) = delete;

public:
	bool LoadDataFromFile(const std::string& inFileName, bool bMultiThreading = false);

	const GVector<DxFbxVertex>& GetVertices() const;
	const GVector<std::uint32_t>& GetIndices() const;
	const GVector<std::string>& GetSubsetNames() const;
	const GVector<std::pair<UINT, UINT>>& GetSubsets() const;
	const GUnorderedMap<std::string, DxFbxMaterial>& GetMaterials() const;
	const DxFbxSkeleton& GetSkeleton() const;
	const GUnorderedMap<std::string, DxFbxAnimation>& GetAnimations() const;

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
		UINT inClusterIndex, int inParentIndex, const DxFbxSkeleton& inSkeleton, DxFbxBone& outBone);
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
		const fbxsdk::FbxAMatrix& inGeometryTransform, fbxsdk::FbxTakeInfo* inTakeInfo, DxFbxAnimation& outAnimation,
		UINT inClusterIndex, int inParentIndex);
	//* Deprecated.
	void BuildAnimationKeyFrames(fbxsdk::FbxTakeInfo* inTakeInfo, fbxsdk::FbxCluster* inCluster, fbxsdk::FbxNode* inNode,
		fbxsdk::FbxAMatrix inGeometryTransform, DxFbxAnimation& outAnimation, UINT inClusterIndex, int inParentIndex);

private:
	fbxsdk::FbxManager* mFbxManager;
	fbxsdk::FbxIOSettings* mFbxIos;
	fbxsdk::FbxScene* mFbxScene;

	GVector<DxFbxVertex> mVertices;
	GVector<std::uint32_t> mIndices;

	GVector<std::string> mSubsetNames;
	GVector<std::pair<UINT /* Index count */, UINT /* Start index */>> mSubsets;

	GUnorderedMap<std::string /* Submesh name */, DxFbxMaterial> mMaterials;

	DxFbxSkeleton mSkeleton;

	GUnorderedMap<std::string /* Submesh name */,
		GUnorderedMap<int /* Control point index */,
		GVector<BoneIndexWeight> /* Max size: 8 */>> mControlPointsWeights;

	GUnorderedMap<std::string /* Clip name */, DxFbxAnimation> mAnimations;

	GUnorderedMap<UINT /* Bone index */, fbxsdk::FbxCluster*> mClusters;
	GVector<UINT /* BOne index */> mNestedClusters;
};