#include "DX12Game/GameCore.h"
#include "DX12Game/Mesh.h"
#include "DX12Game/GameWorld.h"
#include "DX12Game/Renderer.h"
#include "DX12Game/FBXImporter.h"

using namespace DirectX;
using namespace DirectX::PackedVector;

namespace {
	const std::string fileNamePrefix = "./../../../../Assets/Models/";

	void LoadVertices(const DxFbxImporter& inImporter, std::vector<Vertex>& outVertices) {
		const auto& fbxVertices = inImporter.GetVertices();
		std::vector<Vertex> vertices(fbxVertices.size());
		for (size_t i = 0; i < fbxVertices.size(); ++i) {
			auto& vert = vertices[i];
			vert.Pos = fbxVertices[i].Pos;
			vert.Normal = fbxVertices[i].Normal;
			vert.TexC = fbxVertices[i].TexC;
			vert.TangentU = fbxVertices[i].TangentU;

			vert.Pos.x *= -1.0f;
			vert.Normal.x *= -1.0f;
			vert.TangentU.x *= -1.0f;
		}
		outVertices.swap(vertices);
	}

	void LoadSkinnedVertices(const DxFbxImporter& inImporter, std::vector<SkinnedVertex>& outVertices) {
		const auto& fbxVertices = inImporter.GetVertices();
		std::vector<SkinnedVertex> vertices(fbxVertices.size());
		for (size_t i = 0; i < fbxVertices.size(); ++i) {
			vertices[i].Pos = fbxVertices[i].Pos;
			vertices[i].Normal = fbxVertices[i].Normal;
			vertices[i].TexC = fbxVertices[i].TexC;
			vertices[i].TangentU = fbxVertices[i].TangentU;

			vertices[i].BoneWeights0.x = fbxVertices[i].BoneWeights[0];
			vertices[i].BoneWeights0.y = fbxVertices[i].BoneWeights[1];
			vertices[i].BoneWeights0.z = fbxVertices[i].BoneWeights[2];
			vertices[i].BoneWeights0.w = fbxVertices[i].BoneWeights[3];

			vertices[i].BoneWeights1.x = fbxVertices[i].BoneWeights[4];
			vertices[i].BoneWeights1.y = fbxVertices[i].BoneWeights[5];
			vertices[i].BoneWeights1.z = fbxVertices[i].BoneWeights[6];
			vertices[i].BoneWeights1.w = fbxVertices[i].BoneWeights[7];

			vertices[i].BoneIndices0[0] = fbxVertices[i].BoneIndices[0];
			vertices[i].BoneIndices0[1] = fbxVertices[i].BoneIndices[1];
			vertices[i].BoneIndices0[2] = fbxVertices[i].BoneIndices[2];
			vertices[i].BoneIndices0[3] = fbxVertices[i].BoneIndices[3];

			vertices[i].BoneIndices1[0] = fbxVertices[i].BoneIndices[4];
			vertices[i].BoneIndices1[1] = fbxVertices[i].BoneIndices[5];
			vertices[i].BoneIndices1[2] = fbxVertices[i].BoneIndices[6];
			vertices[i].BoneIndices1[3] = fbxVertices[i].BoneIndices[7];

			vertices[i].Pos.x *= -1.0f;
			vertices[i].Normal.x *= -1.0f;
			vertices[i].TangentU.x *= -1.0f;
		}
		outVertices.swap(vertices);
	}

	void LoadIndices(const DxFbxImporter& inImporter, std::vector<std::uint32_t>& outIndices) {
		const auto& fbxIndices = inImporter.GetIndices();
		std::vector<std::uint32_t> indices(fbxIndices.size());
		for (UINT i = 0; i < fbxIndices.size(); i += 3) {
			indices[i] = fbxIndices[i + 2];
			indices[i + 1] = fbxIndices[i + 1];
			indices[i + 2] = fbxIndices[i];
		}
		outIndices.swap(indices);
	}

	void LoadDrawArgs(const DxFbxImporter& inImporter, const std::string& inMeshName,
						std::vector<std::string>& outDrawArgs) {
		const auto& subsetNames = inImporter.GetSubsetNames();
		for (const auto& subsetName : subsetNames) {
			std::stringstream sstream;
			sstream << inMeshName << '_' << subsetName.c_str();
			outDrawArgs.push_back(sstream.str());
		}
	}

	void LoadSubsets(const DxFbxImporter& inImporter, std::vector<std::pair<UINT, UINT>>& outSubsets) {
		const auto& subsets = inImporter.GetSubsets();
		for (const auto& subset : subsets)
			outSubsets.push_back(subset);
	}

	void LoadSkeletons(const DxFbxImporter& inImporter, Skeleton& outSkeleton) {
		const auto& skeleton = inImporter.GetSkeleton();
		for (const auto& bone : skeleton.GetBones()) {
			outSkeleton.mBones.emplace_back(bone.Name, bone.ParentIndex,
				bone.LocalBindPose, bone.GlobalBindPose, bone.GlobalInvBindPose);
		}
	}

	void LoadAnimations(const DxFbxImporter& inImporter, std::unordered_map<std::string, Animation>& outAnimations) {
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

	void LoadMaterials(const DxFbxImporter& inImporter, const std::string& inMeshName,
						std::unordered_map<std::string, MaterialIn>& outMaterials, Renderer*& inRenderer) {
		const auto& fbxMaterials = inImporter.GetMaterials();
		if (!fbxMaterials.empty()) {
			for (auto material : fbxMaterials) {
				std::stringstream matNameSstream;
				matNameSstream << inMeshName << '_' << material.second.MaterialName;

				std::stringstream geoNameSstream;
				geoNameSstream << inMeshName << '_' << material.first.c_str();

				size_t texFileNameExtIndex;
				const std::string& diffuseMapFileName = material.second.DiffuseMapFileName;
				std::string modifiedDiffuseMapFileName;
				if (!diffuseMapFileName.empty()) {
					texFileNameExtIndex = diffuseMapFileName.find_last_of('.', diffuseMapFileName.length());
					modifiedDiffuseMapFileName = diffuseMapFileName.substr(0, texFileNameExtIndex);
					modifiedDiffuseMapFileName.append(".dds");
				}

				const std::string& normalMapFileName = material.second.NormalMapFileName;
				std::string modifiedNormalMapFileName;
				if (!normalMapFileName.empty()) {
					texFileNameExtIndex = normalMapFileName.find_last_of('.', normalMapFileName.length());
					modifiedNormalMapFileName = normalMapFileName.substr(0, texFileNameExtIndex);
					modifiedNormalMapFileName.append(".dds");
				}

				const std::string& specularMapFileName = material.second.SpecularMapFileName;
				std::string modifiedSpecularMapFileName;
				if (!specularMapFileName.empty()) {
					texFileNameExtIndex = specularMapFileName.find_last_of('.', specularMapFileName.length());
					modifiedSpecularMapFileName = specularMapFileName.substr(0, texFileNameExtIndex);
					modifiedSpecularMapFileName.append(".dds");
				}

				MaterialIn matIn = { matNameSstream.str(), modifiedDiffuseMapFileName,
					modifiedNormalMapFileName, modifiedSpecularMapFileName,
					material.second.MatTransform, material.second.DiffuseAlbedo,
					material.second.FresnelR0, material.second.Roughness };

				outMaterials[geoNameSstream.str()] = matIn;
			}
		}
	}
}

Mesh::Mesh(bool inIsSkeletal, bool inNeedToBeAligned)
	: mIsSkeletal(inIsSkeletal), 
	  mNeedToBeAligned(inNeedToBeAligned) {
	mRenderer = GameWorld::GetWorld()->GetRenderer();
}

bool Mesh::Load(const std::string& inFileName, bool bMultiThreading) {
	size_t extIndex = inFileName.find_last_of('.', inFileName.length());
	mMeshName = inFileName.substr(0, extIndex);

	TaskTimer timer;
	timer.SetBeginTime();
	DxFbxImporter importer;
	{
		std::stringstream fileNameSstream;
		fileNameSstream << fileNamePrefix << inFileName;

		if (!importer.LoadDataFromFile(fileNameSstream.str(), bMultiThreading))
			return false;
	}
	timer.SetEndTime();
	StringUtil::Log({ inFileName, " Loading Time: ", std::to_string(timer.GetElapsedTime()), " seconds\n" });

	std::thread vertThread;
	if (mIsSkeletal)
		vertThread = std::thread(LoadSkinnedVertices, std::ref(importer), std::ref(mSkinnedVertices));
	else
		vertThread = std::thread(LoadVertices, std::ref(importer), std::ref(mVertices));

	std::thread skeletonThread = std::thread(LoadSkeletons, std::ref(importer), std::ref(mSkinnedData.mSkeleton));
	std::thread idxThread = std::thread(LoadIndices, std::ref(importer), std::ref(mIndices));
	std::thread drawArgThread = std::thread(LoadDrawArgs, std::ref(importer), mMeshName, std::ref(mDrawArgs));
	std::thread subsetThread = std::thread(LoadSubsets, std::ref(importer), std::ref(mSubsets));
	std::thread animationThread = std::thread(LoadAnimations, std::ref(importer), std::ref(mSkinnedData.mAnimations));
	std::thread materialThread = std::thread(LoadMaterials, std::ref(importer), 
		mMeshName, std::ref(mMaterials), std::ref(mRenderer));

	skeletonThread.join();

	std::thread genSkelDataThread = std::thread(&Mesh::GenerateSkeletonData, this);

	vertThread.join();
	idxThread.join();
	drawArgThread.join();
	subsetThread.join();
	animationThread.join();
	materialThread.join();
	genSkelDataThread.join();

	if (mIsSkeletal) {
		GenerateSkeletonData();

		OutputSkinnedDataInfo();
	}

	mRenderer->AddGeometry(this);

	const auto& anims = mSkinnedData.mAnimations;
	if (!anims.empty()) {
		for (const auto& anim : anims) {
			UINT idx = mRenderer->AddAnimations(anim.first, anim.second);

			mClipsIndex[anim.first] = idx;
		}
		mRenderer->UpdateAnimationsMap();
	}

	return true;
}

const std::string& Mesh::GetMeshName() const {
	return mMeshName;
}

const std::vector<std::string>& Mesh::GetDrawArgs() const {
	return mDrawArgs;
}

const std::vector<Vertex>& Mesh::GetVertices() const {
	return mVertices;
}

const std::vector<SkinnedVertex>& Mesh::GetSkinnedVertices() const {
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
	return mIsSkeletal;
}

const SkinnedData& Mesh::GetSkinnedData() const {
	return mSkinnedData;
}

const std::vector<Vertex>& Mesh::GetSkeletonVertices() const {
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
	for (auto boneIter = bones.cbegin(), boneEnd = bones.cend(); boneIter != boneEnd; ++boneIter) {
		int index = static_cast<int>(boneIter - bones.cbegin());
		int parentIndex = boneIter->ParentIndex;

		XMMATRIX globalTransform;
		if (mNeedToBeAligned) {
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

		Vertex vert;
		vert.Pos = { transf.x, transf.y, transf.z };
		vert.TexC = { (float)index, (float)parentIndex };
		AddSkeletonVertex(vert);

		if (parentIndex != -1) {
			XMMATRIX rotMat = XMMatrixRotationQuaternion(quat);
			XMVECTOR forward = XMVector4Transform(XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f), rotMat);

			XMStoreFloat4(&transf, trans + (0.125f * scale * forward));

			vert.Pos = { transf.x, transf.y, transf.z };
		}
		else {
			vert.Pos = { 0.0f, 0.0f, 0.0f };
			vert.TexC = { -1.0f, 0.0f };
		}
		AddSkeletonVertex(vert);
	}
}

void Mesh::AddSkeletonVertex(const Vertex& inVertex) {
	auto vertIter = std::find(mSkeletonVertices.cbegin(), mSkeletonVertices.cend(), inVertex);
	if (vertIter != mSkeletonVertices.cend()) {
		std::uint32_t index = static_cast<std::uint32_t>(vertIter - mSkeletonVertices.cbegin());
		mSkeletonIndices.push_back(index);
	}
	else {		
		mSkeletonIndices.push_back((std::uint32_t)mSkeletonVertices.size());
		mSkeletonVertices.push_back(inVertex);
	}
}

void Mesh::OutputSkinnedDataInfo() {
	StringUtil::Logln({ "Mesh Name: ", mMeshName });
	StringUtil::Logln({ L" Num Bones: ", std::to_wstring(mSkinnedData.mSkeleton.mBones.size()), L"\n" });
	for (auto animIter = mSkinnedData.mAnimations.cbegin(), end = mSkinnedData.mAnimations.cend(); animIter != end; ++animIter) {
		const auto& anim = animIter->second;
		StringUtil::Logln({ " Clip Name: ", animIter->first });
		StringUtil::Logln({ L" Num Frames: ", std::to_wstring(anim.mNumFrames) });
		StringUtil::Logln({ L" Duration: ", std::to_wstring(anim.mDuration) });
		StringUtil::Logln({ L" Frame Duration: ", std::to_wstring(anim.mFrameDuration) });
		StringUtil::Logln({ L" Curves Size: ", std::to_wstring(anim.mCurves.size()), L"\n" });
	}
}