#include "DX12Game/FbxImporter.h"
#include "DX12Game/StringUtil.h"
#include "DX12Game/ThreadUtil.h"

#include <string>

using namespace DirectX;
using namespace DirectX::PackedVector;
using namespace fbxsdk;

namespace {
	const UINT InvalidBoneIndex = std::numeric_limits<UINT>::max();
}

Game::FbxVertex::FbxVertex(
		XMFLOAT3 inPos		/* = { 0.0f, 0.0f, 0.0f } */, 
		XMFLOAT3 inNormal	/* = { 0.0f, 0.0f, 0.0f } */,
		XMFLOAT2 inTexC		/* = { 0.0f, 0.0f } */, 
		XMFLOAT3 inTangentU	/* = { 0.0f, 0.0f, 0.0f } */) {
	mPos = inPos;
	mNormal = inNormal;
	mTexC = inTexC;
	mTangentU = inTangentU;
	for (auto& weight : mBoneWeights)
		weight = 0.0f;
	for (auto& index : mBoneIndices)
		index = -1;
}

bool Game::operator==(const Game::FbxVertex& lhs, const Game::FbxVertex& rhs) {
	bool bResult = MathHelper::IsEqual(lhs.mPos,		rhs.mPos)		&&
				   MathHelper::IsEqual(lhs.mNormal,		rhs.mNormal)	&&
				   MathHelper::IsEqual(lhs.mTexC,		rhs.mTexC)		&&
				   MathHelper::IsEqual(lhs.mTangentU,	rhs.mTangentU);
	if (!bResult) 
		return false;
	
	for (UINT i = 0; i < 8; ++i) {
		if (MathHelper::IsNotEqual(lhs.mBoneWeights[i], rhs.mBoneWeights[i]) || lhs.mBoneIndices[i] != rhs.mBoneIndices[i])
			return false;
	}

	return true;
}

Game::FbxMaterial::FbxMaterial() {
	mMatTransform = MathHelper::Identity4x4();
	mDiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	mFresnelR0 = { 0.5f, 0.5f, 0.5f };
	mRoughness = 0.5f;
}
Game::
FbxBone::FbxBone() {
	mParentIndex = -1;

	mLocalBindPose = MathHelper::Identity4x4();
	mGlobalBindPose = MathHelper::Identity4x4();
	mGlobalInvBindPose = MathHelper::Identity4x4();
}

const std::vector<Game::FbxBone>& Game::FbxSkeleton::GetBones() const {
	return mBones;
}

size_t Game::FbxSkeleton::GetNumBones() const {
	return mBones.size();
}

size_t Game::FbxAnimation::GetNumFrames() const {
	return mNumFrames;
}

float Game::FbxAnimation::GetDuration() const {
	return mDuration;
}

float Game::FbxAnimation::GetFrameDuration() const {
	return mFrameDuration;
}

const std::unordered_map<UINT, std::vector<DirectX::XMFLOAT4X4>>& Game::FbxAnimation::GetCurves() const {
	return mCurves;
}

Game::FbxImporter::FbxImporter() {
	mFbxManager = nullptr;
	mFbxIos = nullptr;
	mFbxScene = nullptr;
}

Game::FbxImporter::~FbxImporter() {
	if (mFbxScene != nullptr)
		mFbxScene->Destroy();

	if (mFbxIos != nullptr)
		mFbxIos->Destroy();

	if (mFbxManager != nullptr)
		mFbxManager->Destroy();
}

bool Game::FbxImporter::LoadDataFromFile(const std::string& inFileName) {
	if (!LoadFbxScene(inFileName)) {
		WErrln(L"Failed to load fbx scnene");
		return false;
	}

	auto fbxRootNode = mFbxScene->GetRootNode();
	
	LoadSkeletonHierarchy(fbxRootNode);

	bool loadingBonesStatus;
	std::thread loadingBonesThread = std::thread([this](fbxsdk::FbxNode* inFbxRootNode, bool& inStatus) -> void {
		for (size_t i = 0; i < inFbxRootNode->GetChildCount(); ++i) {
			FbxNode* childNode = inFbxRootNode->GetChild(static_cast<int>(i));
			if (childNode->GetNodeAttribute() && 
					(childNode->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eMesh)) {
				if (!LoadBones(childNode)) {
					inStatus = false;
					return;
				}
			}
		}
		inStatus = true;
	}, fbxRootNode, std::ref(loadingBonesStatus));

	std::thread loadingMatsThread = std::thread([this](fbxsdk::FbxNode* inFbxRootNode) -> void {
		for (size_t i = 0; i < inFbxRootNode->GetChildCount(); ++i) {
			FbxNode* childNode = inFbxRootNode->GetChild(static_cast<int>(i));
			if (childNode->GetNodeAttribute() && childNode->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eMesh)
				LoadMaterials(childNode);
		}
	}, fbxRootNode);

	loadingBonesThread.join();
	if (!loadingBonesStatus) {
		WErrln(L"Failed to load bones");
		return false;
	}

	loadingMatsThread.join();
	
	MoveDataFromFbxAMatrixToDirectXMath();
	NormalizeWeigths();

	UINT prevNumVertices = 0;
	for (size_t i = 0; i < fbxRootNode->GetChildCount(); ++i) {
		FbxNode* childNode = fbxRootNode->GetChild((int)i);
		if (childNode->GetNodeAttribute() && childNode->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eMesh) {
			mSubsetNames.push_back(childNode->GetName());

			prevNumVertices = LoadDataFromMesh(childNode, prevNumVertices);

			if (prevNumVertices < 0)
				return false;
		}
	}

	return true;
}

const std::vector<Game::FbxVertex>& Game::FbxImporter::GetVertices() const {
	return mVertices;
}

const std::vector<std::uint32_t>& Game::FbxImporter::GetIndices() const {
	return mIndices;
}

const std::vector<std::string>& Game::FbxImporter::GetSubsetNames() const {
	return mSubsetNames;
}

const std::vector<std::pair<UINT, UINT>>& Game::FbxImporter::GetSubsets() const {
	return mSubsets;
}

const std::unordered_map<std::string, Game::FbxMaterial>& Game::FbxImporter::GetMaterials() const {
	return mMaterials;
}

const Game::FbxSkeleton& Game::FbxImporter::GetSkeleton() const {
	return mSkeleton;
}

const std::unordered_map<std::string, Game::FbxAnimation>& Game::FbxImporter::GetAnimations() const {
	return mAnimations;
}

bool Game::FbxImporter::LoadFbxScene(const std::string& inFileName) {
	mFbxManager = FbxManager::Create();
	mFbxIos = FbxIOSettings::Create(mFbxManager, IOSROOT);

	mFbxManager->SetIOSettings(mFbxIos);

	auto fbxImporter = fbxsdk::FbxImporter::Create(mFbxManager, "importer");

	bool status = fbxImporter->Initialize(inFileName.c_str(), -1, mFbxManager->GetIOSettings());

	if (!status) {
		std::wstringstream wsstream;
		wsstream << "Failed to initialize FbxImporter" << std::endl;
		wsstream << "File name: " << inFileName.c_str() << std::endl;
		wsstream << "Error returned: " << fbxImporter->GetStatus().GetErrorString() << std::endl;
		WErrln({ wsstream.str().c_str() });
		return false;
	}

	mFbxScene = FbxScene::Create(mFbxManager, "scene");

	if (!fbxImporter->Import(mFbxScene)) {
		std::wstringstream wsstream;
		wsstream << "Failed to import FbxImporter" << std::endl;
		wsstream << "File name: " << inFileName.c_str() << std::endl;
		WErrln(wsstream.str().c_str());
		return false;
	}

	//FbxGeometryConverter geometryConverter(fbxManager);
	//geometryConverter.Triangulate(fbxScene, true);

	fbxImporter->Destroy();

	return true;
}

namespace {
	XMFLOAT3 ReadNormal(const FbxMesh* inMesh, UINT inControlPointIndex, UINT inVertexCounter) {
		if (inMesh->GetElementNormalCount() < 1)
			return XMFLOAT3(0.0f, 0.0f, 0.0f);

		const FbxGeometryElementNormal* elementNormal = inMesh->GetElementNormal();
		XMFLOAT3 normal;

		switch (elementNormal->GetMappingMode()) {
		case FbxGeometryElement::eByControlPoint: {
			switch (elementNormal->GetReferenceMode()) {
			case FbxGeometryElement::eDirect:
				normal.x = static_cast<float>(elementNormal->GetDirectArray().GetAt(inControlPointIndex).mData[0]);
				normal.y = static_cast<float>(elementNormal->GetDirectArray().GetAt(inControlPointIndex).mData[1]);
				normal.z = static_cast<float>(elementNormal->GetDirectArray().GetAt(inControlPointIndex).mData[2]);
				break;

			case FbxGeometryElement::eIndexToDirect:
				const UINT index = elementNormal->GetIndexArray().GetAt(inControlPointIndex);
				normal.x = static_cast<float>(elementNormal->GetDirectArray().GetAt(index).mData[0]);
				normal.y = static_cast<float>(elementNormal->GetDirectArray().GetAt(index).mData[1]);
				normal.z = static_cast<float>(elementNormal->GetDirectArray().GetAt(index).mData[2]);
				break;
			}
		}
		break;

		case FbxGeometryElement::eByPolygonVertex: {
			switch (elementNormal->GetReferenceMode()) {
			case FbxGeometryElement::eDirect:
				normal.x = static_cast<float>(elementNormal->GetDirectArray().GetAt(inVertexCounter).mData[0]);
				normal.y = static_cast<float>(elementNormal->GetDirectArray().GetAt(inVertexCounter).mData[1]);
				normal.z = static_cast<float>(elementNormal->GetDirectArray().GetAt(inVertexCounter).mData[2]);
				break;

			case FbxGeometryElement::eIndexToDirect:
				const UINT index = elementNormal->GetIndexArray().GetAt(inVertexCounter);
				normal.x = static_cast<float>(elementNormal->GetDirectArray().GetAt(index).mData[0]);
				normal.y = static_cast<float>(elementNormal->GetDirectArray().GetAt(index).mData[1]);
				normal.z = static_cast<float>(elementNormal->GetDirectArray().GetAt(index).mData[2]);
				break;
			}
		}
		break;
		}

		return normal;
	}

	XMFLOAT3 ReadTangent(const FbxMesh* inMesh, UINT inControlPointIndex, UINT inVertexCounter) {
		if (inMesh->GetElementTangentCount() < 1)
			return XMFLOAT3(0.0f, 0.0f, 0.0f);

		const FbxGeometryElementBinormal* elementTangent = inMesh->GetElementBinormal();
		XMFLOAT3 tangent;

		switch (elementTangent->GetMappingMode()) {
		case FbxGeometryElement::eByControlPoint: {
			switch (elementTangent->GetReferenceMode()) {
			case FbxGeometryElement::eDirect:
				tangent.x = static_cast<float>(elementTangent->GetDirectArray().GetAt(inControlPointIndex).mData[0]);
				tangent.y = static_cast<float>(elementTangent->GetDirectArray().GetAt(inControlPointIndex).mData[1]);
				tangent.z = static_cast<float>(elementTangent->GetDirectArray().GetAt(inControlPointIndex).mData[2]);
				break;

			case FbxGeometryElement::eIndexToDirect:
				const UINT index = elementTangent->GetIndexArray().GetAt(inControlPointIndex);
				tangent.x = static_cast<float>(elementTangent->GetDirectArray().GetAt(index).mData[0]);
				tangent.y = static_cast<float>(elementTangent->GetDirectArray().GetAt(index).mData[1]);
				tangent.z = static_cast<float>(elementTangent->GetDirectArray().GetAt(index).mData[2]);
				break;
			}
		}
		break;

		case FbxGeometryElement::eByPolygonVertex: {
			switch (elementTangent->GetReferenceMode()) {
			case FbxGeometryElement::eDirect:
				tangent.x = static_cast<float>(elementTangent->GetDirectArray().GetAt(inVertexCounter).mData[0]);
				tangent.y = static_cast<float>(elementTangent->GetDirectArray().GetAt(inVertexCounter).mData[1]);
				tangent.z = static_cast<float>(elementTangent->GetDirectArray().GetAt(inVertexCounter).mData[2]);
				break;

			case FbxGeometryElement::eIndexToDirect:
				const UINT index = elementTangent->GetIndexArray().GetAt(inVertexCounter);
				tangent.x = static_cast<float>(elementTangent->GetDirectArray().GetAt(index).mData[0]);
				tangent.y = static_cast<float>(elementTangent->GetDirectArray().GetAt(index).mData[1]);
				tangent.z = static_cast<float>(elementTangent->GetDirectArray().GetAt(index).mData[2]);
				break;
			}
		}
		break;
		}

		return tangent;
	}

	XMFLOAT2 ReadUV(const FbxMesh* inMesh, UINT inControlPointIndex, UINT inTextureUVIndex) {
		if (inMesh->GetElementUVCount() < 1)
			return XMFLOAT2(0.0f, 0.0f);

		const FbxGeometryElementUV* elementUV = inMesh->GetElementUV();
		XMFLOAT2 uv;

		switch (elementUV->GetMappingMode()) {
		case FbxGeometryElement::eByControlPoint: {
			switch (elementUV->GetReferenceMode()) {
			case FbxGeometryElement::eDirect:
				uv.x = static_cast<float>(elementUV->GetDirectArray().GetAt(inControlPointIndex).mData[0]);
				uv.y = static_cast<float>(elementUV->GetDirectArray().GetAt(inControlPointIndex).mData[1]);
				break;

			case FbxGeometryElement::eIndexToDirect:
				const UINT index = elementUV->GetIndexArray().GetAt(inControlPointIndex);
				uv.x = static_cast<float>(elementUV->GetDirectArray().GetAt(index).mData[0]);
				uv.y = static_cast<float>(elementUV->GetDirectArray().GetAt(index).mData[1]);
				break;
			}
		}
		break;

		case FbxGeometryElement::eByPolygonVertex: {
			switch (elementUV->GetReferenceMode()) {
			case FbxGeometryElement::eDirect:
				uv.x = static_cast<float>(elementUV->GetDirectArray().GetAt(inTextureUVIndex).mData[0]);
				uv.y = static_cast<float>(elementUV->GetDirectArray().GetAt(inTextureUVIndex).mData[1]);
				break;

			case FbxGeometryElement::eIndexToDirect:
				const UINT index = elementUV->GetIndexArray().GetAt(inTextureUVIndex);
				uv.x = static_cast<float>(elementUV->GetDirectArray().GetAt(index).mData[0]);
				uv.y = static_cast<float>(elementUV->GetDirectArray().GetAt(index).mData[1]);
				break;
			}
		}
		break;
		}

		return uv;
	}
}

int Game::FbxImporter::LoadDataFromMesh(FbxNode* inNode, UINT inPrevNumVertices) {
	auto fbxMesh = inNode->GetMesh();
	std::string meshName = fbxMesh->GetName();

	const int controlPointCount = fbxMesh->GetControlPointsCount();
	std::vector<XMFLOAT3> controlPoints(controlPointCount);

	for (int i = 0; i < controlPointCount; ++i) {
		controlPoints[i].x = static_cast<float>(fbxMesh->GetControlPointAt(i).mData[0]);
		controlPoints[i].y = static_cast<float>(fbxMesh->GetControlPointAt(i).mData[1]);
		controlPoints[i].z = static_cast<float>(fbxMesh->GetControlPointAt(i).mData[2]);
	}

	const UINT polygonCount = fbxMesh->GetPolygonCount();
	UINT vertexCounter = 0;

	for (UINT polygonIdx = 0; polygonIdx < polygonCount; ++polygonIdx) {
		const UINT numPolygonVertices = fbxMesh->GetPolygonSize(polygonIdx);

		if (numPolygonVertices != 3) {
			WErrln(L"Polygon vertex size for Fbx mesh is not matched 3");
			return -1;
		}

		for (int vertIdx = 0; vertIdx < 3; ++vertIdx) {
			UINT controlPointIndex = fbxMesh->GetPolygonVertex(polygonIdx, vertIdx);

			const XMFLOAT3 pos = controlPoints[controlPointIndex];
			const XMFLOAT3 normal = ReadNormal(fbxMesh, controlPointIndex, vertexCounter);
			const XMFLOAT2 texC = ReadUV(fbxMesh, controlPointIndex, vertexCounter);
			const XMFLOAT3 tangent = ReadTangent(fbxMesh, controlPointIndex, vertexCounter);

			FbxVertex vertex = { pos, normal, texC, tangent };
			const auto& boneIdxWeights = mControlPointsWeights[meshName][controlPointIndex];
			for (size_t i = 0, end = boneIdxWeights.size(); i < end; ++i) {
				vertex.mBoneIndices[i] = boneIdxWeights[i].mBoneIndex;
				vertex.mBoneWeights[i] = boneIdxWeights[i].mWeight;
			}

			if (mUniqueVertices.count(vertex) == 0) {
				mUniqueVertices[vertex] = static_cast<std::uint32_t>(mVertices.size());
				mVertices.push_back(vertex);
			}
			
			mIndices.push_back(mUniqueVertices[vertex]);
			++vertexCounter;
		}
	}

	mSubsets.emplace_back(static_cast<UINT>(vertexCounter), inPrevNumVertices);

	return vertexCounter + inPrevNumVertices;
}

void Game::FbxImporter::LoadMaterials(FbxNode* inNode) {
	int materialCount = inNode->GetMaterialCount();

	for (int i = 0; i < materialCount; ++i) {
		auto material = inNode->GetMaterial(i);

		std::string meshName = inNode->GetName();
		mMaterials[meshName].mMaterialName = material->GetName();

		auto diffProp = material->FindProperty(FbxSurfaceMaterial::sDiffuse);
		int diffuseMapCount = diffProp.GetSrcObjectCount<FbxTexture>();
		for (int j = 0; j < diffuseMapCount; ++j) {
			auto texture = FbxCast<FbxTexture>(diffProp.GetSrcObject<FbxTexture>(j));
			auto fileTexture = FbxCast<FbxFileTexture>(texture);

			std::string str = fileTexture->GetFileName();
			size_t strIndex = str.find_last_of('\\', str.length()) + 1;

			mMaterials[meshName].mDiffuseMapFileName = str.substr(strIndex);
		}

		auto normalProp = material->FindProperty(FbxSurfaceMaterial::sNormalMap);
		int normalMapCount = normalProp.GetSrcObjectCount<FbxTexture>();
		for (int j = 0; j < normalMapCount; ++j) {
			auto texture = FbxCast<FbxTexture>(normalProp.GetSrcObject<FbxTexture>(j));
			auto fileTexture = FbxCast<FbxFileTexture>(texture);

			std::string str = fileTexture->GetFileName();
			size_t strIndex = str.find_last_of('\\', str.length()) + 1;

			mMaterials[meshName].mNormalMapFileName = str.substr(strIndex);
		}

		auto specProp = material->FindProperty(FbxSurfaceMaterial::sSpecularFactor);
		int specularMapCount = specProp.GetSrcObjectCount<FbxTexture>();
		for (int j = 0; j < specularMapCount; ++j) {
			auto texture = FbxCast<FbxTexture>(specProp.GetSrcObject<FbxTexture>(j));
			auto fileTexture = FbxCast<FbxFileTexture>(texture);

			std::string str = fileTexture->GetFileName();
			size_t strIndex = str.find_last_of('\\', str.length()) + 1;

			mMaterials[meshName].mSpecularMapFileName = str.substr(strIndex);
		}

		auto transProp = material->FindProperty(FbxSurfaceMaterial::sTransparencyFactor);
		int transMapCount = transProp.GetSrcObjectCount<FbxTexture>();
		for (int j = 0; j < transMapCount; ++j) {
			auto texture = FbxCast<FbxTexture>(transProp.GetSrcObject<FbxTexture>(j));
			auto fileTexture = FbxCast<FbxFileTexture>(texture);

			std::string str = fileTexture->GetFileName();
			size_t strIndex = str.find_last_of('\\', str.length()) + 1;

			std::string fileName = str.substr(strIndex);
			if (fileName == mMaterials[meshName].mDiffuseMapFileName)
				continue;

			mMaterials[meshName].mAlphaMapFileName = fileName;
		}

		XMStoreFloat4x4(&mMaterials[meshName].mMatTransform, XMMatrixScaling(1.0f, -1.0f, 0.0f));
		mMaterials[meshName].mDiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
		mMaterials[meshName].mFresnelR0 = { 0.1f, 0.1f, 0.1f };
		mMaterials[meshName].mRoughness = 0.6f;
	}
}

void Game::FbxImporter::LoadSkeletonHierarchy(FbxNode* inRootNode) {
	for (int childIndex = 0; childIndex < inRootNode->GetChildCount(); ++childIndex) {
		FbxNode* currNode = inRootNode->GetChild(childIndex);
		LoadSkeletonHierarchyRecurcively(currNode, 0, 0, -1);
	}
}

void Game::FbxImporter::LoadSkeletonHierarchyRecurcively(FbxNode* inNode, int inDepth, int inMyIndex, int inParentIndex) {
	bool bResult = false;

	if (inNode->GetNodeAttribute() && inNode->GetNodeAttribute()->GetAttributeType() &&
		inNode->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eSkeleton) {

		FbxBone bone;
		bone.mParentIndex = inParentIndex;
		bone.mName = inNode->GetName();
		mSkeleton.mBones.push_back(bone);
		bResult = true;
	}

	if (bResult) {
		for (int childIndex = 0; childIndex < inNode->GetChildCount(); ++childIndex) {
			LoadSkeletonHierarchyRecurcively(inNode->GetChild(childIndex), 
				++inDepth, (int)mSkeleton.mBones.size(), inMyIndex);
		}
	}
	else {
		for (int childIndex = 0; childIndex < inNode->GetChildCount(); ++childIndex)
			LoadSkeletonHierarchyRecurcively(inNode->GetChild(childIndex), inDepth, inMyIndex, inParentIndex);
	}
}

namespace {
	FbxAMatrix GetGeometryTransformation(FbxNode* inNode) {
		if (inNode == nullptr) {
			std::stringstream sstream;
			sstream << FileLineStr << "Mesh geometry is invalid" << std::endl;
			throw std::invalid_argument(sstream.str().c_str());
		}

		auto scale = inNode->GetGeometricScaling(FbxNode::eSourcePivot);
		auto rot = inNode->GetGeometricRotation(FbxNode::eSourcePivot);
		auto trans = inNode->GetGeometricTranslation(FbxNode::eSourcePivot);

		return FbxAMatrix(trans, rot, scale);
	}

	void UnifyCoordinates(FbxAMatrix& outFbxMat) {
		auto rot = outFbxMat.GetR();
		auto trans = outFbxMat.GetT();

		rot.mData[1] *= -1.0;
		rot.mData[2] *= -1.0;
		outFbxMat.SetR(rot);

		trans.mData[0] *= -1.0;
		outFbxMat.SetT(trans);
	}

	FbxAMatrix UnifyCoordinates(const FbxAMatrix& inFbxMat) {
		FbxAMatrix mat = inFbxMat;

		auto rot = inFbxMat.GetR();
		auto trans = inFbxMat.GetT();

		rot.mData[1] *= -1.0;
		rot.mData[2] *= -1.0;
		mat.SetR(rot);

		trans.mData[0] *= -1.0;
		mat.SetT(trans);

		return mat;
	}
}

bool Game::FbxImporter::LoadBones(FbxNode* inNode) {
	auto currMesh = inNode->GetMesh();
	std::string meshName = currMesh->GetName();

	// For compability with other softwares... (usually identity matrix)
	auto geometryTransform = GetGeometryTransformation(inNode);
	UnifyCoordinates(geometryTransform);

	unsigned int deformerCount = currMesh->GetDeformerCount();
	for (UINT deformerIndex = 0; deformerIndex < deformerCount; ++deformerIndex) {
		auto currSkin = reinterpret_cast<FbxSkin*>(currMesh->GetDeformer(deformerIndex));
		if (currSkin == nullptr)
			continue;

		int clusterCount = currSkin->GetClusterCount();
		for (int i = 0; i < clusterCount; ++i) {
			auto currCluster = currSkin->GetCluster(i);			
			UINT clusterIndex = FindBoneIndexUsingName(currCluster->GetLink()->GetName());
			if (clusterIndex == InvalidBoneIndex) {
				WErrln(L"FindBoneIndexUsingName() returned invalid index");
				return false;
			}

			BuildControlPointsWeigths(currCluster, clusterIndex, meshName);

			auto iter = std::find(mNestedClusters.cbegin(), mNestedClusters.cend(), clusterIndex);
			if (iter != mNestedClusters.cend())
				continue;
			mNestedClusters.push_back(clusterIndex);

			mClusters[clusterIndex] = currCluster;

			auto& currBone = mSkeleton.mBones[clusterIndex];
			int parentIndex = currBone.mParentIndex;

			BuildBindPoseData(currCluster, geometryTransform, clusterIndex, parentIndex, mSkeleton, currBone);
			BuildAnimations(inNode, currCluster, geometryTransform, clusterIndex, parentIndex);
		}
	}

	return true;
}

void Game::FbxImporter::NormalizeWeigths() {
	for (auto skelIter = mControlPointsWeights.begin(), skelEnd = mControlPointsWeights.end(); skelIter != skelEnd; ++skelIter) {
		auto& skel = skelIter->second;
		for (auto cpIdxIter = skel.begin(), cpIdxEnd = skel.end(); cpIdxIter != cpIdxEnd; ++cpIdxIter) {
			auto& boneIdxWeights = cpIdxIter->second;
			float sum = 0.0f;
			for (const auto& boneIdxWeight : boneIdxWeights)
				sum += boneIdxWeight.mWeight;

			if (!MathHelper::IsEqual(sum, 1.0f)) {
				float invSum = 1.0f / sum;
				for (auto& boneIdxWeight : boneIdxWeights)
					boneIdxWeight.mWeight *= invSum;
			}
		}
	}
}

namespace {
	void FbxAMatrixToXMFloat4x4(XMFLOAT4X4& outDxMat, const FbxAMatrix& inFbxMat) {
		const auto& s = inFbxMat.GetS();
		const auto& r = inFbxMat.GetR();
		const auto& t = inFbxMat.GetT();

		XMVECTOR Pitch = XMQuaternionRotationAxis(XMVectorSet(
			1.0f, 
			0.0f,
			0.0f,
			0.0f),
			MathHelper::DegreesToRadians(static_cast<float>(r.mData[0]))
		);
		XMVECTOR Yaw = XMQuaternionRotationAxis(XMVectorSet(
			0.0f,
			1.0f,
			0.0f, 
			0.0f),
			MathHelper::DegreesToRadians(static_cast<float>(r.mData[1]))
		);
		XMVECTOR Roll = XMQuaternionRotationAxis(XMVectorSet(
			0.0f, 
			0.0f,
			1.0f,
			0.0f),
			MathHelper::DegreesToRadians(static_cast<float>(r.mData[2]))
		);
		XMVECTOR Quat = XMQuaternionMultiply(XMQuaternionMultiply(Pitch, Yaw), Roll);

		XMVECTOR S = XMVectorSet(static_cast<float>(
			s.mData[0]),
			static_cast<float>(s.mData[1]),
			static_cast<float>(s.mData[2]), 
			0.0f
		);
		XMVECTOR Q = XMVectorSet(
			Quat.m128_f32[0],
			Quat.m128_f32[1],
			Quat.m128_f32[2], 
			Quat.m128_f32[3]
		);
		XMVECTOR T = XMVectorSet(
			static_cast<float>(t.mData[0]),
			static_cast<float>(t.mData[1]),
			static_cast<float>(t.mData[2]), 1.0f
		);
		XMVECTOR Zero = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
		XMMATRIX affine = XMMatrixAffineTransformation(S, Zero, Q, T);
		XMStoreFloat4x4(&outDxMat, affine);
	}

	XMFLOAT4X4 FbxAMatrixToXMFloat4x4(const FbxAMatrix& inFbxMat) {
		const auto& s = inFbxMat.GetS();
		const auto& r = inFbxMat.GetR();
		const auto& t = inFbxMat.GetT();

		XMVECTOR Pitch = XMQuaternionRotationAxis(XMVectorSet(
			1.0f,
			0.0f,
			0.0f,
			0.0f),
			MathHelper::DegreesToRadians(static_cast<float>(r.mData[0]))
		);
		XMVECTOR Yaw = XMQuaternionRotationAxis(XMVectorSet(
			0.0f, 
			1.0f,
			0.0f,
			0.0f),
			MathHelper::DegreesToRadians(static_cast<float>(r.mData[1]))
		);
		XMVECTOR Roll = XMQuaternionRotationAxis(XMVectorSet(
			0.0f,
			0.0f,
			1.0f,
			0.0f),
			MathHelper::DegreesToRadians(static_cast<float>(r.mData[2]))
		);
		XMVECTOR Quat = XMQuaternionMultiply(XMQuaternionMultiply(Pitch, Yaw), Roll);
		
		XMVECTOR S = XMVectorSet(
			static_cast<float>(s.mData[0]), 
			static_cast<float>(s.mData[1]), 
			static_cast<float>(s.mData[2]), 
			0.0f
		);
		XMVECTOR Q = XMVectorSet(
			Quat.m128_f32[0],
			Quat.m128_f32[1], 
			Quat.m128_f32[2], 
			Quat.m128_f32[3]
		);
		XMVECTOR T = XMVectorSet(
			static_cast<float>(t.mData[0]),
			static_cast<float>(t.mData[1]),
			static_cast<float>(t.mData[2]),
			1.0f
		);
		XMVECTOR Zero = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
		
		XMFLOAT4X4 result;
		XMStoreFloat4x4(&result, XMMatrixAffineTransformation(S, Zero, Q, T));
		return result;
	}
}

void Game::FbxImporter::MoveDataFromFbxAMatrixToDirectXMath() {
	for (auto& bone : mSkeleton.mBones) {
		FbxAMatrixToXMFloat4x4(bone.mLocalBindPose,		bone.mFbxLocalBindPose);
		FbxAMatrixToXMFloat4x4(bone.mGlobalBindPose,	bone.mFbxGlobalBindPose);
		FbxAMatrixToXMFloat4x4(bone.mGlobalInvBindPose, bone.mFbxGlobalInvBindPose);
	}
}

UINT Game::FbxImporter::FindBoneIndexUsingName(const std::string& inBoneName) {
	const auto& bones = mSkeleton.mBones;
	for (auto iter = bones.cbegin(), end = bones.cend(); iter != end; ++iter) {
		if (iter->mName == inBoneName)
			return static_cast<int>(iter - bones.cbegin());
	}

	return { InvalidBoneIndex };
}

void Game::FbxImporter::BuildBindPoseData(fbxsdk::FbxCluster*		inCluster,
									  const fbxsdk::FbxAMatrix&	inGeometryTransform,										
									  UINT						inClusterIndex, 
									  int						inParentIndex,										
									  const FbxSkeleton&		inSkeleton,
									  FbxBone&				outBone) {
	// For compability with other softwares... (usually identity matrix)
	FbxAMatrix transformMatrix;
	inCluster->GetTransformMatrix(transformMatrix);
	UnifyCoordinates(transformMatrix);

	FbxAMatrix transformLinkMatrix;
	inCluster->GetTransformLinkMatrix(transformLinkMatrix);
	UnifyCoordinates(transformLinkMatrix);
	
	if (inParentIndex != -1) {
		FbxAMatrix transformParentLinkMatrix;
		mClusters[inParentIndex]->GetTransformLinkMatrix(transformParentLinkMatrix);
		UnifyCoordinates(transformParentLinkMatrix);

		outBone.mFbxLocalBindPose = transformParentLinkMatrix.Inverse() * transformLinkMatrix;
	}
	else {
		outBone.mFbxLocalBindPose = transformLinkMatrix;
	}

	outBone.mFbxGlobalBindPose = transformLinkMatrix * inGeometryTransform;
	outBone.mFbxGlobalInvBindPose = transformLinkMatrix.Inverse() * inGeometryTransform;
}

void Game::FbxImporter::BuildControlPointsWeigths(FbxCluster* inCluster, UINT inClusterIndex, const std::string& inMeshName) {
	UINT indexCount = inCluster->GetControlPointIndicesCount();
	auto controlPointIndices = inCluster->GetControlPointIndices();
	auto controlPointWeights = inCluster->GetControlPointWeights();
	for (UINT i = 0; i < indexCount; ++i) {
		BoneIndexWeight boneIdxWeight;
		boneIdxWeight.mBoneIndex = inClusterIndex;
		boneIdxWeight.mWeight = (float)controlPointWeights[i];

		auto& cpWeights = mControlPointsWeights[inMeshName][controlPointIndices[i]];
		auto iter = std::find(cpWeights.begin(), cpWeights.end(), boneIdxWeight);
		if (iter == cpWeights.end())
			cpWeights.push_back(boneIdxWeight);
	}
}

void Game::FbxImporter::BuildAnimations(FbxNode* inNode,
									FbxCluster* inCluster,
									const FbxAMatrix& inGeometryTransform, 		
									UINT inClusterIndex,
									int inParentIndex) {
	int stackCount = mFbxScene->GetSrcObjectCount<FbxAnimStack>();
	for (int stackIdx = 0; stackIdx < stackCount; ++stackIdx) {
		auto animStack = mFbxScene->GetSrcObject<FbxAnimStack>(stackIdx);	
		int layerCount = animStack->GetMemberCount<FbxAnimLayer>();
		for (int layerIdx = 0; layerIdx < layerCount; ++layerIdx) {
			auto animLayer = FbxCast<FbxAnimLayer>(animStack->GetMember(layerIdx));
			auto layerName = animLayer->GetName();
			
			auto takeInfo = mFbxScene->GetTakeInfo(layerName);

			std::string layerNameStr(layerName);
			size_t markOfIndex = layerNameStr.find_first_of('|');
			std::string animationName = layerNameStr.substr(markOfIndex + 1);

			auto iter = mAnimations.find(animationName);
			if (iter != mAnimations.cend()) {
				auto& animation = iter->second;
				BuildAnimationKeyFrames(animLayer, inNode, inCluster, 
					inGeometryTransform, takeInfo, animation, inClusterIndex, inParentIndex);
				//BuildAnimationKeyFrames(takeInfo, inCluster, inNode, 
				//	inGeometryTransform, animation, inClusterIndex, inParentIndex);
			}
			else {
				FbxAnimation animation;				
				BuildAnimationKeyFrames(animLayer, inNode, inCluster,
					inGeometryTransform, takeInfo, animation, inClusterIndex, inParentIndex);
				//BuildAnimationKeyFrames(takeInfo, inCluster, inNode,
				//	inGeometryTransform, animation, inClusterIndex, inParentIndex);
				
				mAnimations[animationName] = animation;
			}
		}
	}
}

void Game::FbxImporter::BuildAnimationKeyFrames(FbxAnimLayer*		inAnimLayer,
											FbxNode*			inNode, 
											FbxCluster*			inCluster, 
											const FbxAMatrix&	inGeometryTransform, 
											FbxTakeInfo*		inTakeInfo, 
											FbxAnimation&		outAnimation, 
											UINT				inClusterIndex, 
											int					inParentIndex) {
	auto timeMode = mFbxScene->GetGlobalSettings().GetTimeMode();

	FbxTime startTime = inTakeInfo->mLocalTimeSpan.GetStart();
	FbxTime endTime = inTakeInfo->mLocalTimeSpan.GetStop();

	auto startFrameCount = startTime.GetFrameCount(timeMode);
	auto endFrameCount = endTime.GetFrameCount(timeMode);

	if (inClusterIndex == 0) {
		outAnimation.mNumFrames = endFrameCount - startFrameCount;
		outAnimation.mDuration = static_cast<float>(endTime.GetSecondDouble() - startTime.GetSecondDouble());
		outAnimation.mFrameDuration = outAnimation.mDuration / (outAnimation.mNumFrames++);
	}
	
	const auto& animCurveTransX = inCluster->GetLink()->LclTranslation.GetCurve(inAnimLayer, FBXSDK_CURVENODE_COMPONENT_X);
	const auto& animCurveTransY = inCluster->GetLink()->LclTranslation.GetCurve(inAnimLayer, FBXSDK_CURVENODE_COMPONENT_Y);
	const auto& animCurveTransZ = inCluster->GetLink()->LclTranslation.GetCurve(inAnimLayer, FBXSDK_CURVENODE_COMPONENT_Z);
	const auto& animCurveRotX	= inCluster->GetLink()->LclRotation.GetCurve(inAnimLayer,	 FBXSDK_CURVENODE_COMPONENT_X);
	const auto& animCurveRotY	= inCluster->GetLink()->LclRotation.GetCurve(inAnimLayer,	 FBXSDK_CURVENODE_COMPONENT_Y);
	const auto& animCurveRotZ	= inCluster->GetLink()->LclRotation.GetCurve(inAnimLayer,	 FBXSDK_CURVENODE_COMPONENT_Z);
	const auto& animCurveScaleX = inCluster->GetLink()->LclScaling.GetCurve(inAnimLayer,	 FBXSDK_CURVENODE_COMPONENT_X);
	const auto& animCurveScaleY = inCluster->GetLink()->LclScaling.GetCurve(inAnimLayer,	 FBXSDK_CURVENODE_COMPONENT_Y);
	const auto& animCurveScaleZ = inCluster->GetLink()->LclScaling.GetCurve(inAnimLayer,	 FBXSDK_CURVENODE_COMPONENT_Z);
	
	for (auto currFrame = startFrameCount; currFrame <= endFrameCount; ++currFrame) {
		FbxTime currTime;
		currTime.SetFrame(currFrame, timeMode);
		
		FbxAMatrix localTransform = mSkeleton.mBones[inClusterIndex].mFbxLocalBindPose;
		const auto& T = localTransform.GetT();
		const auto& R = localTransform.GetR();
		const auto& S = localTransform.GetS();

		float transX = (animCurveTransX != NULL) ? -animCurveTransX->Evaluate(currTime) : static_cast<float>(T.mData[0]);
		float transY = (animCurveTransY != NULL) ? animCurveTransY->Evaluate(currTime)  : static_cast<float>(T.mData[1]);
		float transZ = (animCurveTransZ != NULL) ? animCurveTransZ->Evaluate(currTime)  : static_cast<float>(T.mData[2]);
		FbxVector4 transVector4(transX, transY, transZ, 1.0);

		float rotX = (animCurveRotX != NULL) ? animCurveRotX->Evaluate(currTime)  : static_cast<float>(R.mData[0]);
		float rotY = (animCurveRotY != NULL) ? -animCurveRotY->Evaluate(currTime) : static_cast<float>(R.mData[1]);
		float rotZ = (animCurveRotZ != NULL) ? -animCurveRotZ->Evaluate(currTime) : static_cast<float>(R.mData[2]);
		FbxVector4 rotVector4(rotX, rotY, rotZ, 0.0);

		float scaleX = (animCurveScaleX != NULL) ? animCurveScaleX->Evaluate(currTime) : static_cast<float>(S.mData[0]);
		float scaleY = (animCurveScaleY != NULL) ? animCurveScaleY->Evaluate(currTime) : static_cast<float>(S.mData[1]);
		float scaleZ = (animCurveScaleZ != NULL) ? animCurveScaleZ->Evaluate(currTime) : static_cast<float>(S.mData[2]);
		FbxVector4 scaleVector4(scaleX, scaleY, scaleZ, 0.0);

		FbxAMatrix currentPoseTransform(transVector4, rotVector4, scaleVector4);

		FbxAMatrix globalTransform;
		if (inParentIndex != -1)
			globalTransform = 
			outAnimation.mParentGlobalTransforms[inParentIndex][static_cast<size_t>(currFrame)] * currentPoseTransform;
		else 
			globalTransform = currentPoseTransform;

		FbxAMatrix currTransformOffset =
			UnifyCoordinates(std::as_const(inNode->EvaluateGlobalTransform(currTime))) * inGeometryTransform;

		const auto& globalInvTransform = mSkeleton.mBones[inClusterIndex].mFbxGlobalInvBindPose;

		outAnimation.mParentGlobalTransforms[inClusterIndex].push_back(globalTransform);
		outAnimation.mCurves[inClusterIndex].push_back(FbxAMatrixToXMFloat4x4(
			currTransformOffset.Inverse() * globalTransform * globalInvTransform));
	}
}

void Game::FbxImporter::BuildAnimationKeyFrames(FbxTakeInfo*	inTakeInfo,
											FbxCluster*		inCluster, 
											FbxNode*		inNode,
											FbxAMatrix		inGeometryTransform,
											FbxAnimation&	outAnimation, 
											UINT			inClusterIndex,
											int				inParentIndex) {
	auto timeMode = mFbxScene->GetGlobalSettings().GetTimeMode();

	FbxTime startTime = inTakeInfo->mLocalTimeSpan.GetStart();
	FbxTime endTime = inTakeInfo->mLocalTimeSpan.GetStop();

	auto startFrameCount = startTime.GetFrameCount(timeMode);
	auto endFrameCount = endTime.GetFrameCount(timeMode);

	if (inClusterIndex == 0) {
		outAnimation.mNumFrames = endFrameCount - startFrameCount;
		outAnimation.mDuration = static_cast<float>(endTime.GetSecondDouble() - startTime.GetSecondDouble());
		outAnimation.mFrameDuration = outAnimation.mDuration / (outAnimation.mNumFrames++);
	}

	for (auto currFrame = startFrameCount; currFrame <= endFrameCount; ++currFrame) {
		FbxTime currTime;
		currTime.SetFrame(currFrame, timeMode);

		FbxAMatrix currTransformOffset = 
			UnifyCoordinates(std::as_const(inNode->EvaluateGlobalTransform(currTime))) *	inGeometryTransform;

		FbxAMatrix globalTransform;
		if (inClusterIndex != 0) {
			auto parentCluster = mClusters[inParentIndex];

			FbxAMatrix parentTransform = parentCluster->GetLink()->EvaluateGlobalTransform(currTime);
			UnifyCoordinates(parentTransform);
			FbxAMatrix localTransform = inCluster->GetLink()->EvaluateLocalTransform(currTime);
			UnifyCoordinates(localTransform);

			globalTransform = currTransformOffset.Inverse() * parentTransform * localTransform;
		}
		else {
			globalTransform = currTransformOffset.Inverse() * 
				UnifyCoordinates(std::as_const(inCluster->GetLink()->EvaluateLocalTransform(currTime)));
		}

		const auto& globalInvTransform = mSkeleton.mBones[inClusterIndex].mFbxGlobalInvBindPose;
		outAnimation.mCurves[inClusterIndex].push_back(FbxAMatrixToXMFloat4x4(globalTransform * globalInvTransform));
	}
}