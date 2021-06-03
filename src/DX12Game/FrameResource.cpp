#include "DX12Game/GameCore.h"
#include "DX12Game/FrameResource.h"

using namespace DirectX;

ObjectConstants::ObjectConstants() {
	InstanceIndex = 0;
	ObjectPad0 = 0;
	ObjectPad1 = 0;
	ObjectPad2 = 0;
}

InstanceData::InstanceData() {
	World = MathHelper::Identity4x4();
	TexTransform = MathHelper::Identity4x4();
	TimePos = 0.0f;
	MaterialIndex = 0;
	AnimClipIndex = 0;
	State = EInstanceDataState::EID_Visible;
}

InstanceData::InstanceData(const DirectX::XMFLOAT4X4& inWorld, const DirectX::XMFLOAT4X4& inTexTransform,
	float inTimePos, UINT inMaterialIndex, UINT inAnimClipIndex) {

	World = inWorld;
	TexTransform = inTexTransform;
	TimePos = inTimePos;
	MaterialIndex = inMaterialIndex;
	AnimClipIndex = inAnimClipIndex;
	State = 0;
}

PassConstants::PassConstants() {
	View = MathHelper::Identity4x4();
	InvView = MathHelper::Identity4x4();
	Proj = MathHelper::Identity4x4();
	InvProj = MathHelper::Identity4x4();
	ViewProj = MathHelper::Identity4x4();
	InvViewProj = MathHelper::Identity4x4();
	ViewProjTex = MathHelper::Identity4x4();
	ShadowTransform = MathHelper::Identity4x4();
	EyePosW = { 0.0f, 0.0f, 0.0f };
	cbPerObjectPad1 = 0.0f;
	RenderTargetSize = { 0.0f, 0.0f };
	InvRenderTargetSize = { 0.0f, 0.0f };
	NearZ = 0.0f;
	FarZ = 0.0f;
	TotalTime = 0.0f;
	DeltaTime = 0.0f;

	AmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };
}

SsaoConstants::SsaoConstants() {
	InvRenderTargetSize = { 0.0f, 0.0f };

	OcclusionRadius = 0.5f;
	OcclusionFadeStart = 0.2f;
	OcclusionFadeEnd = 2.0f;
	SurfaceEpsilon = 0.05f;
}

MaterialData::MaterialData() {
	DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	FresnelR0 = { 0.01f, 0.01f, 0.01f };
	Roughness = 64.0f;

	MatTransform = MathHelper::Identity4x4();
}

Vertex::Vertex() {
	Pos = { 0.0f, 0.0f, 0.0f };
	Normal = { 0.0f, 0.0f, 0.0f };
	TexC = { 0.0f, 0.0f };
	TangentU = { 0.0f, 0.0f, 0.0f };
}

Vertex::Vertex(XMFLOAT3 inPos, XMFLOAT3 inNormal, XMFLOAT2 inTexC, XMFLOAT3 inTangent) {
	Pos = inPos;
	Normal = inNormal;
	TexC = inTexC;
	TangentU = inTangent;
}

bool operator==(const Vertex& lhs, const Vertex& rhs) {
	return MathHelper::IsEqual(lhs.Pos, rhs.Pos) &&
		MathHelper::IsEqual(lhs.Normal, rhs.Normal) &&
		MathHelper::IsEqual(lhs.TexC, rhs.TexC) &&
		MathHelper::IsEqual(lhs.TangentU, rhs.TangentU);
}

SkinnedVertex::SkinnedVertex() {
	Pos = { 0.0f, 0.0f, 0.0f };
	Normal = { 0.0f, 0.0f, 0.0f };
	TexC = { 0.0f, 0.0f };
	TangentU = { 0.0f, 0.0f, 0.0f };
	BoneWeights0 = { 0.0f, 0.0f, 0.0f, 0.0f };
	BoneWeights1 = { 0.0f, 0.0f, 0.0f, 0.0f };
	for (auto& index : BoneIndices0)
		index = -1;
	for (auto& index : BoneIndices1)
		index = -1;
}

SkinnedVertex::SkinnedVertex(XMFLOAT3 inPos, XMFLOAT3 inNormal, XMFLOAT2 inTexC, XMFLOAT3 inTangent) {
	Pos = inPos;
	Normal = inNormal;
	TexC = inTexC;
	TangentU = inTangent;
}

bool operator==(const SkinnedVertex& lhs, const SkinnedVertex& rhs) {
	bool bResult = MathHelper::IsEqual(lhs.Pos, rhs.Pos) &&
		MathHelper::IsEqual(lhs.Normal, rhs.Normal) &&
		MathHelper::IsEqual(lhs.TexC, rhs.TexC) &&
		MathHelper::IsEqual(lhs.TangentU, rhs.TangentU) &&
		MathHelper::IsEqual(lhs.BoneWeights0, rhs.BoneWeights0) &&
		MathHelper::IsEqual(lhs.BoneWeights1, rhs.BoneWeights1);
	if (!bResult)
		return false;

	for (size_t i = 0; i < 4; ++i) {
		if (lhs.BoneIndices0[i] != rhs.BoneIndices0[i] || lhs.BoneIndices1[i] != rhs.BoneIndices1[i])
			return false;
	}

	return true;
}

FrameResource::FrameResource(ID3D12Device* inDevice, 
	UINT inPassCount, UINT inObjectCount, UINT inMaxInstanceCount, UINT inMaterialCount) {

    ThrowIfFailed(inDevice->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(CmdListAlloc.GetAddressOf())));

    PassCB = std::make_unique<UploadBuffer<PassConstants>>(inDevice, inPassCount, true);
	ObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(inDevice, inObjectCount, true);
	SsaoCB = std::make_unique<UploadBuffer<SsaoConstants>>(inDevice, 1, true);
	MaterialBuffer = std::make_unique<UploadBuffer<MaterialData>>(inDevice, inMaterialCount, false);
	InstanceBuffer = std::make_unique<UploadBuffer<InstanceData>>(inDevice, inObjectCount * inMaxInstanceCount, false);
}