#pragma once

#define FBXSDK_SHARED
#include <fbxsdk.h>

#pragma comment(lib, "libfbxsdk.lib")

struct DxFbxVertex {
	DirectX::XMFLOAT3 Pos = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 Normal = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT2 TexC = { 0.0f, 0.0f };
	DirectX::XMFLOAT3 TangentU = { 0.0f, 0.0f, 0.0f };
	float BoneWeights[8] = { 0.0f };
	int BoneIndices[8] = { -1 };

	DxFbxVertex() = default;
	DxFbxVertex(DirectX::XMFLOAT3 inPos, DirectX::XMFLOAT3 inNormal,
		DirectX::XMFLOAT2 inTexC, DirectX::XMFLOAT3 inTangentU);

	friend bool operator==(const DxFbxVertex& lhs, const DxFbxVertex& rhs);
};

struct DxFbxMaterial {
	std::string MaterialName = "";

	std::string DiffuseMapFileName = "";
	std::string NormalMapFileName = "";
	std::string SpecularMapFileName = "";

	DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();
	DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT3 FresnelR0 = { 0.5f, 0.5f, 0.5f };
	float Roughness = 0.5f;
};

struct DxFbxBone {
	DxFbxBone() = default;

	std::string mName = "";
	int mParentIndex = -1;

	DirectX::XMFLOAT4X4 mLocalBindPose = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 mGlobalBindPose = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 mGlobalInvBindPose = MathHelper::Identity4x4();

	fbxsdk::FbxAMatrix mFbxLocalBindPose;
	fbxsdk::FbxAMatrix mFbxGlobalBindPose;
	fbxsdk::FbxAMatrix mFbxGlobalInvBindPose;
};

class DxFbxSkeleton {
public:
	DxFbxSkeleton() = default;
	virtual ~DxFbxSkeleton() = default;

public:
	const std::vector<DxFbxBone>& GetBones() const;
	size_t GetNumBones() const;

private:
	friend class DxFbxImporter;

	// The bones in the skeleton.
	std::vector<DxFbxBone> mBones;
};

class DxFbxAnimation {
public:
	DxFbxAnimation() = default;
	virtual ~DxFbxAnimation() = default;

public:
	size_t GetNumFrames() const;
	float GetDuration() const;
	float GetFrameDuration() const;
	const std::unordered_map<UINT, std::vector<DirectX::XMFLOAT4X4>>& GetCurves() const;

private:
	friend class DxFbxImporter;

	// Number of frames in the animation
	size_t mNumFrames = 0;
	// Duration of the animation in seconds
	float mDuration = 0.0f;
	// Duration of each frame in the animation
	float mFrameDuration = 0.0f;

	std::unordered_map<UINT /* Bone index */, std::vector<DirectX::XMFLOAT4X4>> mCurves;
	std::unordered_map<int /* Parent Bone index */, std::vector<fbxsdk::FbxAMatrix>> mParentGlobalTransforms;
};

class DxFbxImporter {
private:
	struct BoneIndexWeight {
		int mBoneIndex = -1;
		float mWeight = 0.0f;

		friend bool operator==(const BoneIndexWeight& lhs, const BoneIndexWeight& rhs) {
			return lhs.mBoneIndex == rhs.mBoneIndex &&
				MathHelper::IsEqual(lhs.mWeight, rhs.mWeight);
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
	//* Loads mesh data from the file name.
	bool LoadFile(const std::string& inFileName);
	//* Multi threaded version of the function LoadFile.
	bool MTLoadFile(const std::string& inFileName);

	const std::vector<DxFbxVertex>& GetVertices() const;
	const std::vector<std::uint32_t>& GetIndices() const;
	const std::vector<std::string>& GetSubsetNames() const;
	const std::vector<std::pair<UINT, UINT>>& GetSubsets() const;
	const std::unordered_map<std::string, DxFbxMaterial>& GetMaterials() const;
	const DxFbxSkeleton& GetSkeleton() const;
	const std::unordered_map<std::string, DxFbxAnimation>& GetAnimations() const;

private:
	//* Initializes fbx sdk(The app crashes when triangulating geometry).
	bool LoadScene(const std::string& inFileName);

	int LoadMesh(fbxsdk::FbxNode* inNode, UINT inPrevVertexCounter);
	int MTLoadMesh(fbxsdk::FbxNode* inNode, UINT inPrevVertexCounter);
	int MTLoadMeshAlt(fbxsdk::FbxNode* inNode, UINT inPrevVertexCounter);

	//* Loads texture file name and argument values.
	void LoadMaterials(fbxsdk::FbxNode* inNode);

	//* Generates skeleton hierarchy data that is depth-first.
	void LoadSkeletonHierarchy(fbxsdk::FbxNode* inRootNode);
	//* Finds skeleton node and generate hierarchy data recursively(depth-first).
	void LoadSkeletonHierarchyRecurcively(fbxsdk::FbxNode* inNode, int inDepth, int inMyIndex, int inParentIndex);

	//* Generates pose matrix data for each bone(like a global [inverse] bind pose)
	//   and weight data for each bone.
	void LoadBones(fbxsdk::FbxNode* inNode);

	//* Normalizes the weight for the bones in each control point.
	void NormalizeWeigths();
	//* Moves matrix data in DxFbxBone from fbxsdk::FbxAMatrix to DirectX::XMFLOAT4X4.
	void MoveDataFbxAMatrixToDirectXMath();
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
	//* Temporary function
	void BuildAnimationKeyFrames(fbxsdk::FbxTakeInfo* inTakeInfo, fbxsdk::FbxCluster* inCluster, fbxsdk::FbxNode* inNode,
		fbxsdk::FbxAMatrix inGeometryTransform, DxFbxAnimation& outAnimation, UINT inClusterIndex, int inParentIndex);

private:
	fbxsdk::FbxManager* mFbxManager = nullptr;
	fbxsdk::FbxIOSettings* mFbxIos = nullptr;
	fbxsdk::FbxScene* mFbxScene = nullptr;

	std::vector<DxFbxVertex> mVertices;
	std::vector<std::uint32_t> mIndices;

	std::vector<std::string> mSubsetNames;
	std::vector<std::pair<UINT /* Index count */, UINT /* Start index */>> mSubsets;

	std::unordered_map<std::string /* Submesh name */, DxFbxMaterial> mMaterials;

	DxFbxSkeleton mSkeleton;

	std::unordered_map<std::string /* Submesh name */,	
		std::unordered_map<int /* Control point index */,
		std::vector<BoneIndexWeight> /* Max size: 8 */>> mControlPointsWeights;

	std::unordered_map<std::string /* Clip name */, DxFbxAnimation> mAnimations;

	std::unordered_map<UINT /* Bone index */, fbxsdk::FbxCluster*> mClusters;
	std::vector<UINT /* BOne index */> mNestedClusters;
};