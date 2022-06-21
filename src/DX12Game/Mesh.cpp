#include "DX12Game/Mesh.h"
#include "DX12Game/GameWorld.h"
#include "DX12Game/Renderer.h"
#include "DX12Game/FrameResource.h"
#include "DX12Game/FBXImporter.h"

using namespace DirectX;
using namespace DirectX::PackedVector;

namespace {
	const std::string fileNamePrefix = "./../../../../Assets/Models/";

	void LoadVertices(const Game::FbxImporter& inImporter, std::vector<Game::Vertex>& outVertices) {
		const auto& fbxVertices = inImporter.GetVertices();
		std::vector<Game::Vertex> vertices(fbxVertices.size());
		
		for (size_t i = 0; i < fbxVertices.size(); ++i) {
			auto& vert = vertices[i];
			vert.mPos		= fbxVertices[i].mPos;
			vert.mNormal	= fbxVertices[i].mNormal;
			vert.mTexC		= fbxVertices[i].mTexC;
			vert.mTangentU	= fbxVertices[i].mTangentU;
		
			vert.mPos.x		 *= -1.0f;
			vert.mNormal.x	 *= -1.0f;
			vert.mTangentU.x *= -1.0f;
		}
		
		outVertices.swap(vertices);
	}

	void LoadSkinnedVertices(const Game::FbxImporter& inImporter, std::vector<Game::SkinnedVertex>& outVertices) {
		const auto& fbxVertices = inImporter.GetVertices();
		std::vector<Game::SkinnedVertex> vertices(fbxVertices.size());

		for (size_t i = 0; i < fbxVertices.size(); ++i) {
			vertices[i].mPos		= fbxVertices[i].mPos;
			vertices[i].mNormal		= fbxVertices[i].mNormal;
			vertices[i].mTexC		= fbxVertices[i].mTexC;
			vertices[i].mTangentU	= fbxVertices[i].mTangentU;

			vertices[i].mBoneWeights0.x = fbxVertices[i].mBoneWeights[0];
			vertices[i].mBoneWeights0.y = fbxVertices[i].mBoneWeights[1];
			vertices[i].mBoneWeights0.z = fbxVertices[i].mBoneWeights[2];
			vertices[i].mBoneWeights0.w = fbxVertices[i].mBoneWeights[3];

			vertices[i].mBoneWeights1.x = fbxVertices[i].mBoneWeights[4];
			vertices[i].mBoneWeights1.y = fbxVertices[i].mBoneWeights[5];
			vertices[i].mBoneWeights1.z = fbxVertices[i].mBoneWeights[6];
			vertices[i].mBoneWeights1.w = fbxVertices[i].mBoneWeights[7];

			vertices[i].mBoneIndices0[0] = fbxVertices[i].mBoneIndices[0];
			vertices[i].mBoneIndices0[1] = fbxVertices[i].mBoneIndices[1];
			vertices[i].mBoneIndices0[2] = fbxVertices[i].mBoneIndices[2];
			vertices[i].mBoneIndices0[3] = fbxVertices[i].mBoneIndices[3];

			vertices[i].mBoneIndices1[0] = fbxVertices[i].mBoneIndices[4];
			vertices[i].mBoneIndices1[1] = fbxVertices[i].mBoneIndices[5];
			vertices[i].mBoneIndices1[2] = fbxVertices[i].mBoneIndices[6];
			vertices[i].mBoneIndices1[3] = fbxVertices[i].mBoneIndices[7];

			vertices[i].mPos.x		*= -1.0f;
			vertices[i].mNormal.x	*= -1.0f;
			vertices[i].mTangentU.x *= -1.0f;
		}

		outVertices.swap(vertices);
	}

	void LoadIndices(const Game::FbxImporter& inImporter, std::vector<std::uint32_t>& outIndices) {
		const auto& fbxIndices = inImporter.GetIndices();
		std::vector<std::uint32_t> indices(fbxIndices.size());
		
		for (UINT i = 0; i < fbxIndices.size(); i += 3) {
			indices[i]		= fbxIndices[i + 2];
			indices[i + 1]	= fbxIndices[i + 1];
			indices[i + 2]	= fbxIndices[i];
		}
		
		outIndices.swap(indices);
	}

	void LoadDrawArgs(const Game::FbxImporter& inImporter, const std::string& inMeshName, std::vector<std::string>& outDrawArgs) {
		const auto& subsetNames = inImporter.GetSubsetNames();
		for (const auto& subsetName : subsetNames) {
			std::stringstream sstream;
			sstream << inMeshName << '_' << subsetName.c_str();
			outDrawArgs.push_back(sstream.str());
		}
	}

	void LoadSubsets(const Game::FbxImporter& inImporter, std::vector<std::pair<UINT, UINT>>& outSubsets) {
		const auto& subsets = inImporter.GetSubsets();
		for (const auto& subset : subsets)
			outSubsets.push_back(subset);
	}

	void LoadSkeletons(const Game::FbxImporter& inImporter, Game::Skeleton& outSkeleton) {
		const auto& skeleton = inImporter.GetSkeleton();
		for (const auto& bone : skeleton.GetBones()) {
			outSkeleton.mBones.emplace_back(
				bone.mName, 
				bone.mParentIndex,
				bone.mLocalBindPose,
				bone.mGlobalBindPose,
				bone.mGlobalInvBindPose
			);
		}
	}

	void LoadAnimations(const Game::FbxImporter& inImporter, std::unordered_map<std::string, Game::Animation>& outAnimations) {
		const auto& animations = inImporter.GetAnimations();
		for (auto animIter = animations.cbegin(), animEnd = animations.cend(); animIter != animEnd; ++animIter) {
			auto& anim = outAnimations[animIter->first];
			anim.mNumFrames = animIter->second.GetNumFrames();
			anim.mDuration = animIter->second.GetDuration();
			anim.mFrameDuration = animIter->second.GetFrameDuration();

			const auto& curves = animIter->second.GetCurves();
			anim.mCurves.resize(curves.size());
			for (auto curveIter = curves.cbegin(), end = curves.cend(); curveIter != end; ++curveIter) {
				for (const auto& curve : curveIter->second)
					anim.mCurves[curveIter->first].push_back(curve);
			}
		}
	}

	void LoadMaterials(const Game::FbxImporter& inImporter, const std::string& inMeshName,
		std::unordered_map<std::string, MaterialIn>& outMaterials, Renderer*& inRenderer) {

		const auto& fbxMaterials = inImporter.GetMaterials();
		if (!fbxMaterials.empty()) {
			for (auto material : fbxMaterials) {
				std::stringstream matNameSstream;
				matNameSstream << inMeshName << '_' << material.second.mMaterialName;

				std::stringstream geoNameSstream;
				geoNameSstream << inMeshName << '_' << material.first.c_str();

				size_t texFileNameExtIndex;
				const std::string& diffuseMapFileName = material.second.mDiffuseMapFileName;
				std::string modifiedDiffuseMapFileName;
				if (!diffuseMapFileName.empty()) {
					texFileNameExtIndex = diffuseMapFileName.find_last_of('.', diffuseMapFileName.length());
					modifiedDiffuseMapFileName = diffuseMapFileName.substr(0, texFileNameExtIndex);
					modifiedDiffuseMapFileName.append(".dds");
				}

				const std::string& normalMapFileName = material.second.mNormalMapFileName;
				std::string modifiedNormalMapFileName;
				if (!normalMapFileName.empty()) {
					texFileNameExtIndex = normalMapFileName.find_last_of('.', normalMapFileName.length());
					modifiedNormalMapFileName = normalMapFileName.substr(0, texFileNameExtIndex);
					modifiedNormalMapFileName.append(".dds");
				}

				const std::string& specularMapFileName = material.second.mSpecularMapFileName;
				std::string modifiedSpecularMapFileName;
				if (!specularMapFileName.empty()) {
					texFileNameExtIndex = specularMapFileName.find_last_of('.', specularMapFileName.length());
					modifiedSpecularMapFileName = specularMapFileName.substr(0, texFileNameExtIndex);
					modifiedSpecularMapFileName.append(".dds");
				}

				const std::string& alphaMapFileName = material.second.mAlphaMapFileName;
				std::string modifiedAlphaMapFileName;
				if (!alphaMapFileName.empty()) {
					texFileNameExtIndex = alphaMapFileName.find_last_of('.', alphaMapFileName.length());
					modifiedAlphaMapFileName = alphaMapFileName.substr(0, texFileNameExtIndex);
					modifiedAlphaMapFileName.append(".dds");
				}

				MaterialIn matIn = { 
					matNameSstream.str(), 
					modifiedDiffuseMapFileName,
					modifiedNormalMapFileName, 
					modifiedSpecularMapFileName,
					modifiedAlphaMapFileName,
					material.second.mMatTransform, 
					material.second.mDiffuseAlbedo,
					material.second.mFresnelR0, 
					material.second.mRoughness 
				};

				outMaterials[geoNameSstream.str()] = matIn;
			}
		}
	}
}

Mesh::Mesh(bool inIsSkeletal, bool inNeedToBeAligned)
	: bIsSkeletal(inIsSkeletal), 
	  bNeedToBeAligned(inNeedToBeAligned) {
	mRenderer = GameWorld::GetWorld()->GetRenderer();
}

GameResult Mesh::Load(const std::string& inFileName) {
	size_t extIndex = inFileName.find_last_of('.', inFileName.length());
	mMeshName = inFileName.substr(0, extIndex);
	
	TaskTimer timer;
	timer.SetBeginTime();
	Game::FbxImporter importer;
	{
		std::stringstream fileNameSstream;
		fileNameSstream << fileNamePrefix << inFileName;
	
		if (!importer.LoadDataFromFile(fileNameSstream.str())) {
			std::wstring wstr;
			wstr.assign(inFileName.begin(), inFileName.end());
			ReturnGameResult(E_FAIL, L"Failed to load data from file: " + wstr);
		}
	}
	
	if (bIsSkeletal) {
		LoadSkinnedVertices(std::ref(importer), std::ref(mSkinnedVertices));
		LoadSkeletons(std::ref(importer), std::ref(mSkinnedData.mSkeleton));
		GenerateSkeletonData();
	}
	else {
		LoadVertices(importer, std::ref(mVertices));
	}
	
	LoadIndices(std::ref(importer), std::ref(mIndices));
	LoadDrawArgs(std::ref(importer), mMeshName, std::ref(mDrawArgs));
	LoadSubsets(std::ref(importer), std::ref(mSubsets));
	LoadAnimations(std::ref(importer), std::ref(mSkinnedData.mAnimations));
	LoadMaterials(std::ref(importer), mMeshName, std::ref(mMaterials), std::ref(mRenderer));
	mRenderer->AddGeometry(this);
	
	const auto& anims = mSkinnedData.mAnimations;
	if (!anims.empty()) {
		for (const auto& anim : anims) {
			UINT idx = mRenderer->AddAnimations(anim.first, anim.second);
	
			mClipsIndex[anim.first] = idx;
		}
		mRenderer->UpdateAnimationsMap();
	}
	
	timer.SetEndTime();
	Logln("Mesh Name: ", mMeshName);
	Logln("  Loading Time: ", std::to_string(timer.GetElapsedTime()), " seconds");
	if (bIsSkeletal) OutputSkinnedDataInfo();

	return GameResultOk;
}

const std::string& Mesh::GetMeshName() const {
	return mMeshName;
}

const std::vector<std::string>& Mesh::GetDrawArgs() const {
	return mDrawArgs;
}

const std::vector<Game::Vertex>& Mesh::GetVertices() const {
	return mVertices;
}

const std::vector<Game::SkinnedVertex>& Mesh::GetSkinnedVertices() const {
	return mSkinnedVertices;
}

const std::vector<std::uint32_t>& Mesh::GetIndices() const {
	return mIndices;
}

const std::vector<std::pair<UINT, UINT>>& Mesh::GetSubsets() const {
	return mSubsets;
}

const std::unordered_map<std::string, MaterialIn>& Mesh::GetMaterials() const {
	return mMaterials;
}

bool Mesh::GetIsSkeletal() const {
	return bIsSkeletal;
}

const Game::SkinnedData& Mesh::GetSkinnedData() const {
	return mSkinnedData;
}

const std::vector<Game::Vertex>& Mesh::GetSkeletonVertices() const {
	return mSkeletonVertices;
}

const std::vector<std::uint32_t> Mesh::GetSkeletonIndices() const {
	return mSkeletonIndices;
}

UINT Mesh::GetClipIndex(const std::string& inClipName) const {
	auto iter = mClipsIndex.find(inClipName);
	return iter != mClipsIndex.end() ? iter->second : std::numeric_limits<UINT>::infinity();
}

void Mesh::GenerateSkeletonData() {
	const auto& bones = mSkinnedData.mSkeleton.mBones;
	for (auto boneIter = bones.begin(), boneEnd = bones.end(); boneIter != boneEnd; ++boneIter) {
		int index = static_cast<int>(boneIter - bones.begin());
		int parentIndex = boneIter->ParentIndex;

		XMMATRIX globalTransform;
		if (bNeedToBeAligned) {
			globalTransform = XMMatrixMultiply(XMLoadFloat4x4(&boneIter->GlobalBindPose),
				XMMatrixRotationAxis(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), XM_PIDIV2));
		}
		else {
			globalTransform = XMLoadFloat4x4(&boneIter->GlobalBindPose);
		}
		XMVECTOR scale;
		XMVECTOR quat;
		XMVECTOR trans;
		XMMatrixDecompose(&scale, &quat, &trans, globalTransform);

		XMFLOAT4 transf;
		XMStoreFloat4(&transf, trans);

		Game::Vertex vert;
		vert.mPos = { transf.x, transf.y, transf.z };
		vert.mTexC = { static_cast<float>(index), static_cast<float>(parentIndex) };
		
		AddSkeletonVertex(vert);

		if (parentIndex != -1) {
			XMMATRIX rotMat = XMMatrixRotationQuaternion(quat);
			XMVECTOR forward = XMVector4Transform(XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f), rotMat);

			XMStoreFloat4(&transf, trans + (0.125f * scale * forward));

			vert.mPos = { transf.x, transf.y, transf.z };
		}
		else {
			vert.mPos = { 0.0f, 0.0f, 0.0f };
			vert.mTexC = { 0.0f, 0.0f };
		}

		AddSkeletonVertex(vert);
	}
}

void Mesh::AddSkeletonVertex(const Game::Vertex& inVertex) {
	try {
		//Logln("Hi");
		auto vertIter = std::find(mSkeletonVertices.begin(), mSkeletonVertices.end(), inVertex);
		//Logln("Bye");
		if (vertIter != mSkeletonVertices.end()) {
			std::uint32_t index = static_cast<std::uint32_t>(vertIter - mSkeletonVertices.begin());
			mSkeletonIndices.push_back(index);
			//Logln("Overlapped");
		}
		else {
			mSkeletonIndices.push_back(static_cast<std::uint32_t>(mSkeletonVertices.size()));
			mSkeletonVertices.push_back(inVertex);
			//Logln("Unique");
		}
	}
	catch (std::exception& e) {
		Logln(e.what());
	}
}

void Mesh::OutputSkinnedDataInfo() {
	WLogln(L"    Num Bones: ", std::to_wstring(mSkinnedData.mSkeleton.mBones.size()));
	for (auto animIter = mSkinnedData.mAnimations.cbegin(), end = mSkinnedData.mAnimations.cend(); animIter != end; ++animIter) {
		const auto& anim = animIter->second;
		Logln("      Clip Name: ", animIter->first);
		WLogln(L"        Num Frames: ", std::to_wstring(anim.mNumFrames));
		WLogln(L"        Duration: ", std::to_wstring(anim.mDuration));
		WLogln(L"        Frame Duration: ", std::to_wstring(anim.mFrameDuration));
		WLogln(L"        Curves Size: ", std::to_wstring(anim.mCurves.size()));
	}
}