#include "DX12Game/GameCore.h"
#include "DX12Game/FbxImporter.h"

using namespace DirectX;
using namespace DirectX::PackedVector;
using namespace fbxsdk;

namespace {
	const int InvalidBoneIndex = std::numeric_limits<UINT>::infinity();
}

DxFbxVertex::DxFbxVertex(DirectX::XMFLOAT3 inPos, DirectX::XMFLOAT3 inNormal,
	DirectX::XMFLOAT2 inTexC, DirectX::XMFLOAT3 inTangentU) {
	Pos = inPos;
	Normal = inNormal;
	TexC = inTexC;
	TangentU = inTangentU;
}

bool operator==(const DxFbxVertex& lhs, const DxFbxVertex& rhs) {
	bool bResult = MathHelper::IsEqual(lhs.Pos, rhs.Pos) &&
		MathHelper::IsEqual(lhs.Normal, rhs.Normal) &&
		MathHelper::IsEqual(lhs.TexC, rhs.TexC) &&
		MathHelper::IsEqual(lhs.TangentU, rhs.TangentU);
	if (bResult != true)
		return false;

	for (size_t i = 0; i < 8; ++i) {
		if (!MathHelper::IsEqual(lhs.BoneWeights[i], rhs.BoneWeights[i]) || lhs.BoneIndices[i] != rhs.BoneIndices[i])
			return false;
	}

	return true;
}

const std::vector<DxFbxBone>& DxFbxSkeleton::GetBones() const {
	return mBones;
}

size_t DxFbxSkeleton::GetNumBones() const {
	return mBones.size();
}

size_t DxFbxAnimation::GetNumFrames() const {
	return mNumFrames;
}

float DxFbxAnimation::GetDuration() const {
	return mDuration;
}

float DxFbxAnimation::GetFrameDuration() const {
	return mFrameDuration;
}

const std::unordered_map<UINT, std::vector<DirectX::XMFLOAT4X4>>& DxFbxAnimation::GetCurves() const {
	return mCurves;
}

DxFbxImporter::DxFbxImporter() {}

DxFbxImporter::~DxFbxImporter() {
	if (mFbxScene != nullptr)
		mFbxScene->Destroy();

	if (mFbxIos != nullptr)
		mFbxIos->Destroy();

	if (mFbxManager != nullptr)
		mFbxManager->Destroy();
}

bool DxFbxImporter::LoadFile(const std::string& inFileName) {
	if (!LoadScene(inFileName))
		return false;

	auto fbxRootNode = mFbxScene->GetRootNode();
	LoadSkeletonHierarchy(fbxRootNode);

	for (size_t i = 0; i < fbxRootNode->GetChildCount(); ++i) {
		FbxNode* childNode = fbxRootNode->GetChild((int)i);
		if (childNode->GetNodeAttribute() && childNode->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eMesh)
			LoadBones(childNode);
	}

	MoveDataFbxAMatrixToDirectXMath();
	NormalizeWeigths();

	UINT prevVertexCounter = 0;
	for (size_t i = 0; i < fbxRootNode->GetChildCount(); ++i) {
		FbxNode* childNode = fbxRootNode->GetChild((int)i);
		if (childNode->GetNodeAttribute() && childNode->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eMesh) {
			mSubsetNames.push_back(childNode->GetName());

			LoadMaterials(childNode);

			prevVertexCounter = LoadMesh(childNode, prevVertexCounter);
			if (prevVertexCounter < 0)
				return false;
		}
	}

	return true;
}

bool DxFbxImporter::MTLoadFile(const std::string& inFileName) {
	if (!LoadScene(inFileName))
		return false;

	auto rootNode = mFbxScene->GetRootNode();

	LoadSkeletonHierarchy(rootNode);

	std::thread loadingBonesThread = std::thread([this](fbxsdk::FbxNode* inFbxRootNode) -> void {
		for (size_t i = 0; i < inFbxRootNode->GetChildCount(); ++i) {
			FbxNode* childNode = inFbxRootNode->GetChild((int)i);
			if (childNode->GetNodeAttribute() && childNode->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eMesh)
				LoadBones(childNode);
		}
	}, rootNode);

	std::thread loadingMatsThread = std::thread([this](fbxsdk::FbxNode* inFbxRootNode) -> void {
		for (size_t i = 0; i < inFbxRootNode->GetChildCount(); ++i) {
			FbxNode* childNode = inFbxRootNode->GetChild((int)i);
			if (childNode->GetNodeAttribute() && childNode->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eMesh)
				LoadMaterials(childNode);
		}
	}, rootNode);

	loadingBonesThread.join();
	loadingMatsThread.join();

	MoveDataFbxAMatrixToDirectXMath();
	NormalizeWeigths();

	UINT prevVertexCounter = 0;
	for (size_t i = 0; i < rootNode->GetChildCount(); ++i) {
		FbxNode* childNode = rootNode->GetChild((int)i);
		if (childNode->GetNodeAttribute() && childNode->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eMesh) {
			mSubsetNames.push_back(childNode->GetName());

			prevVertexCounter = MTLoadMeshAlt(childNode, prevVertexCounter);
			if (prevVertexCounter < 0)
				return false;
		}
	}

	return true;
}

const std::vector<DxFbxVertex>& DxFbxImporter::GetVertices() const {
	return mVertices;
}

const std::vector<std::uint32_t>& DxFbxImporter::GetIndices() const {
	return mIndices;
}

const std::vector<std::string>& DxFbxImporter::GetSubsetNames() const {
	return mSubsetNames;
}

const std::vector<std::pair<UINT, UINT>>& DxFbxImporter::GetSubsets() const {
	return mSubsets;
}

const std::unordered_map<std::string, DxFbxMaterial>& DxFbxImporter::GetMaterials() const {
	return mMaterials;
}

const DxFbxSkeleton& DxFbxImporter::GetSkeleton() const {
	return mSkeleton;
}

const std::unordered_map<std::string, DxFbxAnimation>& DxFbxImporter::GetAnimations() const {
	return mAnimations;
}

bool DxFbxImporter::LoadScene(const std::string& inFileName) {
	mFbxManager = FbxManager::Create();
	mFbxIos = FbxIOSettings::Create(mFbxManager, IOSROOT);

	mFbxManager->SetIOSettings(mFbxIos);

	auto fbxImporter = FbxImporter::Create(mFbxManager, "importer");

	bool status = fbxImporter->Initialize(inFileName.c_str(), -1, mFbxManager->GetIOSettings());

	if (!status) {
		std::wstringstream wsstream;
		wsstream << "Failed to initialize FbxImporter" << std::endl;
		wsstream << "File name: " << inFileName.c_str() << std::endl;
		wsstream << "Error returned: " << fbxImporter->GetStatus().GetErrorString() << std::endl;
		MessageBox(nullptr, wsstream.str().c_str(), L"DxFbxImporter", MB_OK);
		return false;
	}

	mFbxScene = FbxScene::Create(mFbxManager, "scene");

	if (!fbxImporter->Import(mFbxScene)) {
		std::wstringstream wsstream;
		wsstream << "Failed to import FbxImporter" << std::endl;
		wsstream << "File name: " << inFileName.c_str() << std::endl;
		MessageBox(nullptr, wsstream.str().c_str(), L"DxFbxImporter", MB_OK);
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

int DxFbxImporter::LoadMesh(FbxNode* inNode, UINT inPrevVertexCounter) {
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

	for (UINT i = 0; i < polygonCount; ++i) {
		const UINT numPolygonVertices = fbxMesh->GetPolygonSize(i);

		if (numPolygonVertices != 3) {
			MessageBox(nullptr, L"Polygon vertex size for Fbx mesh is not matched 3", L"FbxImporter", MB_OK);
			return -1;
		}

		for (int j = 0; j < 3; ++j) {
			UINT controlPointIndex = fbxMesh->GetPolygonVertex(i, j);

			const XMFLOAT3 pos = controlPoints[controlPointIndex];
			const XMFLOAT3 normal = ReadNormal(fbxMesh, controlPointIndex, vertexCounter);
			const XMFLOAT2 texC = ReadUV(fbxMesh, controlPointIndex, vertexCounter);
			const XMFLOAT3 tangent = ReadTangent(fbxMesh, controlPointIndex, vertexCounter);

			DxFbxVertex vertex = { pos, normal, texC, tangent };
			const auto& boneIdxWeights = mControlPointsWeights[meshName][controlPointIndex];
			for (size_t i = 0, end = boneIdxWeights.size(); i < end; ++i) {
				vertex.BoneIndices[i] = boneIdxWeights[i].mBoneIndex;
				vertex.BoneWeights[i] = boneIdxWeights[i].mWeight;
			}

			auto iter = std::find(mVertices.cbegin(), mVertices.cend(), vertex);
			if (iter != mVertices.cend()) {
				const UINT index = static_cast<UINT>(iter - mVertices.cbegin());
				mIndices.push_back(index);
			}
			else {
				const UINT index = (UINT)mVertices.size();
				mIndices.push_back(index);
				mVertices.push_back(vertex);
			}

			++vertexCounter;
		}
	}

	mSubsets.emplace_back((UINT)vertexCounter, inPrevVertexCounter);

	return vertexCounter + inPrevVertexCounter;
}

namespace {
	void Distribute(size_t inPid, UINT inLineSize, UINT inRemaining, UINT inEachPolygonCount, 
					const std::vector<DxFbxVertex>& inPolygonVertices,
					std::vector<DxFbxVertex>& outVertexSet, std::vector<std::uint32_t>& outIndexSet) {
		UINT modifiedIndex = static_cast<UINT>(inPid * inLineSize);
		if (inPid < inRemaining)
			modifiedIndex += static_cast<UINT>(inPid);
		else
			modifiedIndex += inRemaining;

		for (UINT i = 0; i < inEachPolygonCount; ++i) {
			const auto& pVert = inPolygonVertices[modifiedIndex + i];

			auto iter = std::find(outVertexSet.cbegin(), outVertexSet.cend(), pVert);
			if (iter != outVertexSet.cend()) {
				UINT diff = static_cast<UINT>(iter - outVertexSet.cbegin());

				outIndexSet.push_back(diff);
			}
			else {
				UINT index = static_cast<UINT>(outVertexSet.size());

				outVertexSet.push_back(pVert);
				outIndexSet.push_back(index);
			}
		}
	}

	void Composite(std::vector<DxFbxVertex>& outPriVertexSet, const std::vector<DxFbxVertex>& inAuxVertexSet,
					std::vector<std::uint32_t>& outPriIndexSet, const std::vector<std::uint32_t>& inAuxIndexSet) {
		for (const auto& index : inAuxIndexSet) {
			const auto& vertex = inAuxVertexSet[index];

			auto iter = std::find(outPriVertexSet.cbegin(), outPriVertexSet.cend(), vertex);
			if (iter != outPriVertexSet.cend()) {
				std::uint32_t index = static_cast<std::uint32_t>(iter - outPriVertexSet.cbegin());

				outPriIndexSet.push_back(index);
			}
			else {
				std::uint32_t index = (std::uint32_t)outPriVertexSet.size();

				outPriVertexSet.push_back(vertex);
				outPriIndexSet.push_back(index);
			}
		}
	}
}

int DxFbxImporter::MTLoadMesh(FbxNode* inNode, UINT inPrevVertexCounter) {
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
	std::vector<DxFbxVertex> polygonVertices;

	for (UINT i = 0; i < polygonCount; ++i) {
		const UINT numPolygonVertices = fbxMesh->GetPolygonSize(i);

		if (numPolygonVertices != 3) {
			MessageBox(nullptr, L"Polygon vertex size for Fbx mesh is not matched 3", L"FbxImporter", MB_OK);
			return -1;
		}

		for (int j = 0; j < 3; ++j) {
			UINT controlPointIndex = fbxMesh->GetPolygonVertex(i, j);

			const XMFLOAT3 pos = controlPoints[controlPointIndex];
			const XMFLOAT3 normal = ReadNormal(fbxMesh, controlPointIndex, vertexCounter);
			const XMFLOAT2 texC = ReadUV(fbxMesh, controlPointIndex, vertexCounter);
			const XMFLOAT3 tangent = ReadTangent(fbxMesh, controlPointIndex, vertexCounter);

			DxFbxVertex vertex = { pos, normal, texC, tangent };
			const auto& boneIdxWeights = mControlPointsWeights[meshName][controlPointIndex];
			for (size_t i = 0, end = boneIdxWeights.size(); i < end; ++i) {
				vertex.BoneIndices[i] = boneIdxWeights[i].mBoneIndex;
				vertex.BoneWeights[i] = boneIdxWeights[i].mWeight;
			}

			polygonVertices.push_back(vertex);

			++vertexCounter;
		}
	}

	size_t numProcessors = (size_t)ThreadUtil::GetNumberOfProcessors();

	std::vector<std::thread> threads(numProcessors);
	std::vector<std::vector<DxFbxVertex>> vertexSets(numProcessors);
	std::vector<std::vector<std::uint32_t>> indexSets(numProcessors);
	std::vector<UINT> eachPolygonCounts(numProcessors);

	const UINT lineSize = static_cast<UINT>(vertexCounter / numProcessors);
	const UINT remaining = static_cast<UINT>(vertexCounter % numProcessors);
	for (size_t i = 0; i < numProcessors; ++i) {
		if (i < remaining)
			eachPolygonCounts[i] = lineSize + 1;
		else
			eachPolygonCounts[i] = lineSize;
	}

	for (size_t i = 0; i < numProcessors; ++i) {
		threads[i] = std::thread(Distribute, i, lineSize, remaining, eachPolygonCounts[i],
			polygonVertices, std::ref(vertexSets[i]), std::ref(indexSets[i]));
	}
	
	for (auto& thread : threads)
		thread.join();

	const size_t log = static_cast<size_t>(log2(numProcessors));
	size_t increase = 2;
	size_t neighbor = 1;
	for (size_t count = 0; count < log; ++count) {
		for (size_t pid = 0; pid < numProcessors; pid += increase) {
			threads[pid] = std::thread(Composite, 
				std::ref(vertexSets[pid]), vertexSets[pid + neighbor],
				std::ref(indexSets[pid]), indexSets[pid + neighbor]);
		}

		for (size_t i = 0; i < numProcessors; i += increase)
			threads[i].join();
	
		increase = increase << 1;
		neighbor = neighbor << 1;
	}

	std::uint32_t vertSize = (std::uint32_t)mVertices.size();

	for (const auto& vertex : vertexSets[0])
		mVertices.push_back(vertex);

	for (const auto& index : indexSets[0])
		mIndices.push_back(vertSize + index);

	mSubsets.emplace_back((UINT)vertexCounter, inPrevVertexCounter);

	return vertexCounter + inPrevVertexCounter;
}

namespace {
	void DistributeAlt(size_t inEachVertexCount, size_t inOffset, const std::vector<DxFbxVertex>& inPolygonVertices,
			const std::vector<DxFbxVertex>& inOverlappedVertexSet, std::vector<DxFbxVertex>& outUniqueVertexSet) {
		std::uint32_t counter = 0;
		for (auto vertIter = inOverlappedVertexSet.cbegin(), iterEnd = inOverlappedVertexSet.cend(); 
				vertIter != iterEnd; ++vertIter) {
			auto vert = (*vertIter);
			bool isOverlapped = false;
			size_t vertOffset = inOffset + (vertIter - inOverlappedVertexSet.cbegin());

			for (size_t i = inOffset, iEnd = inOffset + inEachVertexCount; i < iEnd; ++i) {
				++counter;
				if (vertOffset == i)
					break;

				const auto& polyVert = inPolygonVertices[i];

				if (vert == polyVert) {
					isOverlapped = true;
					break;
				}
			}

			if (!isOverlapped)
				outUniqueVertexSet.push_back(vert);
		}
		ThreadUtil::Logln({ L"Distribution Loop-Count: ", std::to_wstring(counter) });
	}

	void CompositeAlt(const std::vector<DxFbxVertex>& inOverlappedVertexSet, const std::vector<DxFbxVertex>& inUniqueVertexSet,
						std::vector<std::uint32_t>& outIndexSet,
						const std::vector<std::vector<DxFbxVertex>>& inFrontUniqueVertexSets) {
		std::uint32_t counter = 0;
		size_t frontSize = 0;
		for (const auto& vertexSet : inFrontUniqueVertexSets)
			frontSize += vertexSet.size();

		for (const auto& oVert : inOverlappedVertexSet) {
			++counter;
			auto iter = std::find(inUniqueVertexSet.cbegin(), inUniqueVertexSet.cend(), oVert);
			if (iter != inUniqueVertexSet.cend()) {
				size_t index = iter - inUniqueVertexSet.cbegin();
				outIndexSet.push_back(static_cast<std::uint32_t>(frontSize + index));
			}
			else {
				size_t stackedFrontSize = 0;
				for (const auto& vertexSet : inFrontUniqueVertexSets) {
					for (auto vIter = vertexSet.cbegin(), end = vertexSet.cend(); vIter != end; ++vIter) {
						if (oVert == (*vIter)) {
							size_t index = vIter - vertexSet.cbegin();
							outIndexSet.push_back(static_cast<std::uint32_t>(stackedFrontSize + index));
						}
					}

					stackedFrontSize += vertexSet.size();
				}
			}
		}
		ThreadUtil::Logln({ L"Composition Loop-Count: ", std::to_wstring(counter) });
	}
}

int DxFbxImporter::MTLoadMeshAlt(FbxNode* inNode, UINT inPrevVertexCounter) {
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
	std::vector<DxFbxVertex> polygonVertices;

	for (UINT i = 0; i < polygonCount; ++i) {
		const UINT numPolygonVertices = fbxMesh->GetPolygonSize(i);

		if (numPolygonVertices != 3) {
			MessageBox(nullptr, L"Polygon vertex size for Fbx mesh is not matched 3", L"FbxImporter", MB_OK);
			return -1;
		}

		for (int j = 0; j < 3; ++j) {
			UINT controlPointIndex = fbxMesh->GetPolygonVertex(i, j);

			const XMFLOAT3 pos = controlPoints[controlPointIndex];
			const XMFLOAT3 normal = ReadNormal(fbxMesh, controlPointIndex, vertexCounter);
			const XMFLOAT2 texC = ReadUV(fbxMesh, controlPointIndex, vertexCounter);
			const XMFLOAT3 tangent = ReadTangent(fbxMesh, controlPointIndex, vertexCounter);

			DxFbxVertex vertex = { pos, normal, texC, tangent };
			const auto& boneIdxWeights = mControlPointsWeights[meshName][controlPointIndex];
			for (size_t i = 0, end = boneIdxWeights.size(); i < end; ++i) {
				vertex.BoneIndices[i] = boneIdxWeights[i].mBoneIndex;
				vertex.BoneWeights[i] = boneIdxWeights[i].mWeight;
			}

			polygonVertices.push_back(vertex);

			++vertexCounter;
		}
	}

	size_t numProcessors = (size_t)ThreadUtil::GetNumberOfProcessors();

	std::vector<UINT> eachPolygonCounts(numProcessors);	
	const UINT lineSize = static_cast<UINT>(vertexCounter / numProcessors);
	const UINT remaining = static_cast<UINT>(vertexCounter % numProcessors);
	{
		std::wstringstream wsstream;
		for (size_t i = 0; i < numProcessors; ++i) {
			if (i < remaining)
				eachPolygonCounts[i] = lineSize + 1;
			else
				eachPolygonCounts[i] = lineSize;
			wsstream << L"Thread_" << i << " Line-Size:" << eachPolygonCounts[i] << std::endl;
		}
		ThreadUtil::Lognid(wsstream.str());
	}
	/*
	{
		UINT accumulator = 0;
		UINT lineSize = vertexCounter;
		for (size_t i = 0; i < numProcessors; ++i) {
			lineSize /= 2;
			eachPolygonCounts[i] = lineSize > 0 ? lineSize : 1;
			accumulator += eachPolygonCounts[i];
		}
	
		int diff = vertexCounter - accumulator;
		if (diff > 0) {
			UINT diffLineSize = static_cast<UINT>(diff / numProcessors);
			UINT remaining = static_cast<UINT>(diff % numProcessors);
	
			for (size_t i = 0; i < numProcessors; ++i) {
				eachPolygonCounts[i] += diffLineSize;
				if (i < remaining)
					++eachPolygonCounts[i];
			}
		}
		else if (diff < 0) {
			diff = -diff;
			UINT diffLineSize = static_cast<UINT>(diff / numProcessors);
			UINT remaining = static_cast<UINT>(diff % numProcessors);
	
			for (size_t i = 0; i < numProcessors; ++i) {
				eachPolygonCounts[i] -= diffLineSize;
				if (i < remaining)
					--eachPolygonCounts[i];
			}
		}
	}
	*/
	
	std::vector<std::vector<DxFbxVertex>> overlappedVertexSets(numProcessors);
	{
		size_t counter = 0;
		for (size_t i = 0; i < numProcessors; ++i) {
			for (std::uint32_t j = 0; j < eachPolygonCounts[i]; ++j) {
				const auto& vert = polygonVertices[counter++];
				overlappedVertexSets[i].push_back(vert);
			}
		}
	}

	std::vector<std::vector<DxFbxVertex>> uniqueVertexSets(numProcessors);
	{
		size_t counter = 0;
		for (auto& vertexSet : uniqueVertexSets) {
			vertexSet.reserve(eachPolygonCounts[counter++]);
		}
	}
	
	std::vector<std::thread> threads(numProcessors);
	{
		size_t offset = 0;
		for (size_t i = 0; i < numProcessors; ++i) {
			threads[i] = std::thread(DistributeAlt,
				eachPolygonCounts[i], offset, polygonVertices,
				overlappedVertexSets[i], std::ref(uniqueVertexSets[i]));

			offset += eachPolygonCounts[i];
		}
	}
	for (auto& thread : threads)
		thread.join();

	std::vector<std::vector<std::uint32_t>> indexSets(numProcessors);
	{
		size_t counter = 0;
		for (auto& indexSet : indexSets) {
			indexSet.reserve(eachPolygonCounts[counter++]);
		}
	}

	std::vector<std::vector<DxFbxVertex>> frontUniqueVertexSets;
	for (size_t i = 0; i < numProcessors; ++i) {
		threads[i] = std::thread(CompositeAlt, 
			overlappedVertexSets[i], uniqueVertexSets[i], 
			std::ref(indexSets[i]), frontUniqueVertexSets);
		
		frontUniqueVertexSets.push_back(uniqueVertexSets[i]);
	}
	
	for (auto& thread : threads)
		thread.join();

	std::uint32_t vertSize = (std::uint32_t)mVertices.size();

	for (const auto& vertexSet : uniqueVertexSets) {
		for (const auto& vertex : vertexSet)
			mVertices.push_back(vertex);
	}
	
	for (const auto& indexSet : indexSets) {
		for (const auto& index : indexSet)
			mIndices.push_back(vertSize + index);
	}

	mSubsets.emplace_back((UINT)vertexCounter, inPrevVertexCounter);

	return vertexCounter + inPrevVertexCounter;
}

void DxFbxImporter::LoadMaterials(FbxNode* inNode) {
	int materialCount = inNode->GetMaterialCount();

	for (int i = 0; i < materialCount; ++i) {
		auto material = inNode->GetMaterial(i);

		std::string meshName = inNode->GetName();
		mMaterials[meshName].MaterialName = material->GetName();

		auto diffProp = material->FindProperty(FbxSurfaceMaterial::sDiffuse);

		int diffuseMapCount = diffProp.GetSrcObjectCount<FbxTexture>();
		for (int j = 0; j < diffuseMapCount; ++j) {
			auto texture = FbxCast<FbxTexture>(diffProp.GetSrcObject<FbxTexture>(j));
			auto fileTexture = FbxCast<FbxFileTexture>(texture);

			texture->GetAlphaSource();

			std::string str = fileTexture->GetFileName();
			size_t strIndex = str.find_last_of('\\', str.length()) + 1;

			mMaterials[meshName].DiffuseMapFileName = str.substr(strIndex);
		}

		auto normalProp = material->FindProperty(FbxSurfaceMaterial::sNormalMap);

		int normalMapCount = normalProp.GetSrcObjectCount<FbxTexture>();
		for (int j = 0; j < normalMapCount; ++j) {
			auto texture = FbxCast<FbxTexture>(normalProp.GetSrcObject<FbxTexture>(j));
			auto fileTexture = FbxCast<FbxFileTexture>(texture);

			std::string str = fileTexture->GetFileName();
			size_t strIndex = str.find_last_of('\\', str.length()) + 1;

			mMaterials[meshName].NormalMapFileName = str.substr(strIndex);
		}

		auto specProp = material->FindProperty(FbxSurfaceMaterial::sSpecularFactor);

		int specularMapCount = specProp.GetSrcObjectCount<FbxTexture>();
		for (int j = 0; j < specularMapCount; ++j) {
			auto texture = FbxCast<FbxTexture>(specProp.GetSrcObject<FbxTexture>(j));
			auto fileTexture = FbxCast<FbxFileTexture>(texture);

			std::string str = fileTexture->GetFileName();
			size_t strIndex = str.find_last_of('\\', str.length()) + 1;

			mMaterials[meshName].SpecularMapFileName = str.substr(strIndex);
		}

		auto alphaProp = material->FindProperty(FbxSurfaceMaterial::sTransparentColor);

		int alphaMapCount = alphaProp.GetSrcObjectCount<FbxTexture>();
		//ThreadUtil::Logln(std::to_wstring(alphaMapCount));

		XMStoreFloat4x4(&mMaterials[meshName].MatTransform, XMMatrixScaling(1.0f, -1.0f, 0.0f));
		mMaterials[meshName].DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
		mMaterials[meshName].FresnelR0 = { 0.1f, 0.1f, 0.1f };
		mMaterials[meshName].Roughness = 0.6f;
	}
}

void DxFbxImporter::LoadSkeletonHierarchy(FbxNode* inRootNode) {
	for (int childIndex = 0; childIndex < inRootNode->GetChildCount(); ++childIndex) {
		FbxNode* currNode = inRootNode->GetChild(childIndex);
		LoadSkeletonHierarchyRecurcively(currNode, 0, 0, -1);
	}
}

void DxFbxImporter::LoadSkeletonHierarchyRecurcively(FbxNode* inNode, int inDepth, int inMyIndex, int inParentIndex) {
	bool bResult = false;

	if (inNode->GetNodeAttribute() && inNode->GetNodeAttribute()->GetAttributeType() &&
		inNode->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eSkeleton) {

		DxFbxBone bone;
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
			sstream << __FILE__ << ' ' << __LINE__ << ": Mesh geometry is invalid" << std::endl;
			throw std::invalid_argument(sstream.str().c_str());
		}

		auto scale = inNode->GetGeometricScaling(FbxNode::eSourcePivot);
		auto rot = inNode->GetGeometricRotation(FbxNode::eSourcePivot);
		auto trans = inNode->GetGeometricTranslation(FbxNode::eSourcePivot);

		return FbxAMatrix(trans, rot, scale);
	}

	void AlignFbxAMatrix(FbxAMatrix& outFbxMat) {
		auto rot = outFbxMat.GetR();
		auto trans = outFbxMat.GetT();

		rot.mData[1] *= -1.0;
		rot.mData[2] *= -1.0;
		outFbxMat.SetR(rot);

		trans.mData[0] *= -1.0;
		outFbxMat.SetT(trans);
	}

	FbxAMatrix AlignFbxAMatrix(const FbxAMatrix& inFbxMat) {
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

void DxFbxImporter::LoadBones(FbxNode* inNode) {
	auto currMesh = inNode->GetMesh();
	std::string meshName = currMesh->GetName();

	// For compability with other softwares... (usually identity matrix)
	auto geometryTransform = GetGeometryTransformation(inNode);
	AlignFbxAMatrix(geometryTransform);

	unsigned int deformerCount = currMesh->GetDeformerCount();
	for (UINT deformerIndex = 0; deformerIndex < deformerCount; ++deformerIndex) {
		auto currSkin = reinterpret_cast<FbxSkin*>(currMesh->GetDeformer(deformerIndex));
		if (currSkin == nullptr)
			continue;

		int clusterCount = currSkin->GetClusterCount();
		for (int i = 0; i < clusterCount; ++i) {
			auto currCluster = currSkin->GetCluster(i);			
			UINT clusterIndex = FindBoneIndexUsingName(currCluster->GetLink()->GetName());

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
}

void DxFbxImporter::NormalizeWeigths() {
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

		XMVECTOR Pitch = XMQuaternionRotationAxis(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f),
			MathHelper::DegreesToRadians((float)r.mData[0]));
		XMVECTOR Yaw = XMQuaternionRotationAxis(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),
			MathHelper::DegreesToRadians((float)r.mData[1]));
		XMVECTOR Roll = XMQuaternionRotationAxis(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f),
			MathHelper::DegreesToRadians((float)r.mData[2]));
		XMVECTOR Quat = XMQuaternionMultiply(XMQuaternionMultiply(Pitch, Yaw), Roll);

		XMVECTOR S = XMVectorSet((float)s.mData[0], (float)s.mData[1], (float)s.mData[2], 0.0f);
		XMVECTOR Q = XMVectorSet(Quat.m128_f32[0], Quat.m128_f32[1], Quat.m128_f32[2], Quat.m128_f32[3]);
		XMVECTOR T = XMVectorSet((float)t.mData[0], (float)t.mData[1], (float)t.mData[2], 1.0f);
		XMVECTOR Zero = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
		XMMATRIX affine = XMMatrixAffineTransformation(S, Zero, Q, T);
		XMStoreFloat4x4(&outDxMat, affine);
	}

	XMFLOAT4X4 FbxAMatrixToXMFloat4x4(const FbxAMatrix& inFbxMat) {
		const auto& s = inFbxMat.GetS();
		const auto& r = inFbxMat.GetR();
		const auto& t = inFbxMat.GetT();

		XMVECTOR Pitch = XMQuaternionRotationAxis(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f),
			MathHelper::DegreesToRadians((float)r.mData[0]));
		XMVECTOR Yaw = XMQuaternionRotationAxis(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),
			MathHelper::DegreesToRadians((float)r.mData[1]));
		XMVECTOR Roll = XMQuaternionRotationAxis(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f),
			MathHelper::DegreesToRadians((float)r.mData[2]));
		XMVECTOR Quat = XMQuaternionMultiply(XMQuaternionMultiply(Pitch, Yaw), Roll);
		
		XMVECTOR S = XMVectorSet((float)s.mData[0], (float)s.mData[1], (float)s.mData[2], 0.0f);
		XMVECTOR Q = XMVectorSet(Quat.m128_f32[0], Quat.m128_f32[1], Quat.m128_f32[2], Quat.m128_f32[3]);
		XMVECTOR T = XMVectorSet((float)t.mData[0], (float)t.mData[1], (float)t.mData[2], 1.0f);
		XMVECTOR Zero = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
		
		XMFLOAT4X4 result;
		XMStoreFloat4x4(&result, XMMatrixAffineTransformation(S, Zero, Q, T));
		return result;
	}
}

void DxFbxImporter::MoveDataFbxAMatrixToDirectXMath() {
	for (auto& bone : mSkeleton.mBones) {
		FbxAMatrixToXMFloat4x4(bone.mLocalBindPose, bone.mFbxLocalBindPose);
		FbxAMatrixToXMFloat4x4(bone.mGlobalBindPose, bone.mFbxGlobalBindPose);
		FbxAMatrixToXMFloat4x4(bone.mGlobalInvBindPose, bone.mFbxGlobalInvBindPose);
	}
}

UINT DxFbxImporter::FindBoneIndexUsingName(const std::string& inBoneName) {
	const auto& bones = mSkeleton.mBones;
	for (auto iter = bones.cbegin(), end = bones.cend(); iter != end; ++iter) {
		if (iter->mName == inBoneName)
			return static_cast<int>(iter - bones.cbegin());
	}

	return { InvalidBoneIndex };
}

void DxFbxImporter::BuildBindPoseData(fbxsdk::FbxCluster* inCluster, const fbxsdk::FbxAMatrix& inGeometryTransform,
										UINT inClusterIndex, int inParentIndex,
										const DxFbxSkeleton& inSkeleton, DxFbxBone& outBone) {
	// For compability with other softwares... (usually identity matrix)
	FbxAMatrix transformMatrix;
	inCluster->GetTransformMatrix(transformMatrix);
	AlignFbxAMatrix(transformMatrix);

	FbxAMatrix transformLinkMatrix;
	inCluster->GetTransformLinkMatrix(transformLinkMatrix);
	AlignFbxAMatrix(transformLinkMatrix);

	if (inClusterIndex != 0) {
		FbxAMatrix transformParentLinkMatrix;
		mClusters[inParentIndex]->GetTransformLinkMatrix(transformParentLinkMatrix);
		AlignFbxAMatrix(transformParentLinkMatrix);

		outBone.mFbxLocalBindPose = transformParentLinkMatrix.Inverse() * transformLinkMatrix;
	}
	else {
		outBone.mFbxLocalBindPose = transformLinkMatrix;
	}

	outBone.mFbxGlobalBindPose = transformLinkMatrix * inGeometryTransform;
	outBone.mFbxGlobalInvBindPose = transformLinkMatrix.Inverse() * inGeometryTransform;
}

void DxFbxImporter::BuildControlPointsWeigths(FbxCluster* inCluster, UINT inClusterIndex, const std::string& inMeshName) {
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

void DxFbxImporter::BuildAnimations(FbxNode* inNode, FbxCluster* inCluster, const FbxAMatrix& inGeometryTransform, 
		UINT inClusterIndex, int inParentIndex) {
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
				DxFbxAnimation animation;				
				BuildAnimationKeyFrames(animLayer, inNode, inCluster,
					inGeometryTransform, takeInfo, animation, inClusterIndex, inParentIndex);
				//BuildAnimationKeyFrames(takeInfo, inCluster, inNode,
				//	inGeometryTransform, animation, inClusterIndex, inParentIndex);
				
				mAnimations[animationName] = animation;
			}
		}
	}
}

void DxFbxImporter::BuildAnimationKeyFrames(FbxAnimLayer* inAnimLayer, FbxNode* inNode, FbxCluster* inCluster, 
		const FbxAMatrix& inGeometryTransform, FbxTakeInfo* inTakeInfo, DxFbxAnimation& outAnimation, 
		UINT inClusterIndex, int inParentIndex) {
	auto timeMode = mFbxScene->GetGlobalSettings().GetTimeMode();

	FbxTime startTime = inTakeInfo->mLocalTimeSpan.GetStart();
	FbxTime endTime = inTakeInfo->mLocalTimeSpan.GetStop();

	auto startFrameCount = startTime.GetFrameCount(timeMode);
	auto endFrameCount = endTime.GetFrameCount(timeMode);

	if (inClusterIndex == 0) {
		outAnimation.mNumFrames = endFrameCount - startFrameCount;
		outAnimation.mDuration = static_cast<float>(endTime.GetSecondDouble() - startTime.GetSecondDouble());
		outAnimation.mFrameDuration = outAnimation.mDuration / (outAnimation.mNumFrames);
	}
	
	const auto& animCurveTransX = inCluster->GetLink()->LclTranslation.GetCurve(inAnimLayer, FBXSDK_CURVENODE_COMPONENT_X);
	const auto& animCurveTransY = inCluster->GetLink()->LclTranslation.GetCurve(inAnimLayer, FBXSDK_CURVENODE_COMPONENT_Y);
	const auto& animCurveTransZ = inCluster->GetLink()->LclTranslation.GetCurve(inAnimLayer, FBXSDK_CURVENODE_COMPONENT_Z);
	const auto& animCurveRotX = inCluster->GetLink()->LclRotation.GetCurve(inAnimLayer, FBXSDK_CURVENODE_COMPONENT_X);
	const auto& animCurveRotY = inCluster->GetLink()->LclRotation.GetCurve(inAnimLayer, FBXSDK_CURVENODE_COMPONENT_Y);
	const auto& animCurveRotZ = inCluster->GetLink()->LclRotation.GetCurve(inAnimLayer, FBXSDK_CURVENODE_COMPONENT_Z);
	const auto& animCurveScaleX = inCluster->GetLink()->LclScaling.GetCurve(inAnimLayer, FBXSDK_CURVENODE_COMPONENT_X);
	const auto& animCurveScaleY = inCluster->GetLink()->LclScaling.GetCurve(inAnimLayer, FBXSDK_CURVENODE_COMPONENT_Y);
	const auto& animCurveScaleZ = inCluster->GetLink()->LclScaling.GetCurve(inAnimLayer, FBXSDK_CURVENODE_COMPONENT_Z);
	
	for (auto currFrame = startFrameCount; currFrame < endFrameCount; ++currFrame) {
		FbxTime currTime;
		currTime.SetFrame(currFrame, timeMode);
		
		FbxAMatrix localTransform = mSkeleton.mBones[inClusterIndex].mFbxLocalBindPose;
		const auto& T = localTransform.GetT();
		const auto& R = localTransform.GetR();
		const auto& S = localTransform.GetS();

		float transX = (animCurveTransX != NULL) ? -animCurveTransX->Evaluate(currTime) : (float)T.mData[0];
		float transY = (animCurveTransY != NULL) ? animCurveTransY->Evaluate(currTime) : (float)T.mData[1];
		float transZ = (animCurveTransZ != NULL) ? animCurveTransZ->Evaluate(currTime) : (float)T.mData[2];
		FbxVector4 transVector4(transX, transY, transZ, 1.0);

		float rotX = (animCurveRotX != NULL) ? animCurveRotX->Evaluate(currTime) : (float)R.mData[0];
		float rotY = (animCurveRotY != NULL) ? -animCurveRotY->Evaluate(currTime) : (float)R.mData[1];
		float rotZ = (animCurveRotZ != NULL) ? -animCurveRotZ->Evaluate(currTime) : (float)R.mData[2];
		FbxVector4 rotVector4(rotX, rotY, rotZ, 0.0);

		float scaleX = (animCurveScaleX != NULL) ? animCurveScaleX->Evaluate(currTime) : (float)S.mData[0];
		float scaleY = (animCurveScaleY != NULL) ? animCurveScaleY->Evaluate(currTime) : (float)S.mData[1];
		float scaleZ = (animCurveScaleZ != NULL) ? animCurveScaleZ->Evaluate(currTime) : (float)S.mData[2];
		FbxVector4 scaleVector4(scaleX, scaleY, scaleZ, 0.0);

		FbxAMatrix currentPoseTransform(transVector4, rotVector4, scaleVector4);

		FbxAMatrix globalTransform;
		if (inParentIndex != -1)
			globalTransform = outAnimation.mParentGlobalTransforms[inParentIndex][(size_t)currFrame] * currentPoseTransform;
		else 
			globalTransform = currentPoseTransform;

		FbxAMatrix currTransformOffset =
			AlignFbxAMatrix(std::as_const(inNode->EvaluateGlobalTransform(currTime))) *	inGeometryTransform;

		const auto& globalInvTransform = mSkeleton.mBones[inClusterIndex].mFbxGlobalInvBindPose;

		outAnimation.mParentGlobalTransforms[inClusterIndex].push_back(globalTransform);
		outAnimation.mCurves[inClusterIndex].push_back(FbxAMatrixToXMFloat4x4(
			currTransformOffset.Inverse() * globalTransform * globalInvTransform));
	}
}

void DxFbxImporter::BuildAnimationKeyFrames(FbxTakeInfo* inTakeInfo, FbxCluster* inCluster, FbxNode* inNode,
		FbxAMatrix inGeometryTransform, DxFbxAnimation& outAnimation, UINT inClusterIndex, int inParentIndex) {
	auto timeMode = mFbxScene->GetGlobalSettings().GetTimeMode();

	FbxTime startTime = inTakeInfo->mLocalTimeSpan.GetStart();
	FbxTime endTime = inTakeInfo->mLocalTimeSpan.GetStop();

	auto startFrameCount = startTime.GetFrameCount(timeMode);
	auto endFrameCount = endTime.GetFrameCount(timeMode);

	if (inClusterIndex == 0) {
		outAnimation.mNumFrames = endFrameCount - startFrameCount;
		outAnimation.mDuration = static_cast<float>(endTime.GetSecondDouble() - startTime.GetSecondDouble());
		outAnimation.mFrameDuration = outAnimation.mDuration / (outAnimation.mNumFrames);
	}

	for (auto currFrame = startFrameCount; currFrame < endFrameCount; ++currFrame) {
		FbxTime currTime;
		currTime.SetFrame(currFrame, timeMode);

		FbxAMatrix currTransformOffset = 
			AlignFbxAMatrix(std::as_const(inNode->EvaluateGlobalTransform(currTime))) *	inGeometryTransform;

		FbxAMatrix globalTransform;
		if (inClusterIndex != 0) {
			auto parentCluster = mClusters[inParentIndex];

			FbxAMatrix parentTransform = parentCluster->GetLink()->EvaluateGlobalTransform(currTime);
			AlignFbxAMatrix(parentTransform);
			FbxAMatrix localTransform = inCluster->GetLink()->EvaluateLocalTransform(currTime);
			AlignFbxAMatrix(localTransform);

			globalTransform = currTransformOffset.Inverse() * parentTransform * localTransform;
		}
		else {
			globalTransform = currTransformOffset.Inverse() * 
				AlignFbxAMatrix(std::as_const(inCluster->GetLink()->EvaluateLocalTransform(currTime)));
		}

		const auto& globalInvTransform = mSkeleton.mBones[inClusterIndex].mFbxGlobalInvBindPose;
		outAnimation.mCurves[inClusterIndex].push_back(FbxAMatrixToXMFloat4x4(globalTransform * globalInvTransform));
	}
}