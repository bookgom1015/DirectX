#include "DX12Game/GameCore.h"
#include "DX12Game/FrameResource.h"

using namespace DirectX;

Vertex::Vertex(XMFLOAT3 pos, XMFLOAT3 normal, XMFLOAT2 texC, XMFLOAT3 tangent) {
	Pos = pos;
	Normal = normal;
	TexC = texC;
	TangentU = tangent;
}

bool operator==(const Vertex& lhs, const Vertex& rhs) {
	return MathHelper::IsEqual(lhs.Pos, rhs.Pos) &&
		MathHelper::IsEqual(lhs.Normal, rhs.Normal) &&
		MathHelper::IsEqual(lhs.TexC, rhs.TexC) &&
		MathHelper::IsEqual(lhs.TangentU, rhs.TangentU);
}

SkinnedVertex::SkinnedVertex(XMFLOAT3 pos, XMFLOAT3 normal, XMFLOAT2 texC, XMFLOAT3 tangent) {
	Pos = pos;
	Normal = normal;
	TexC = texC;
	TangentU = tangent;
}

bool operator==(const SkinnedVertex& lhs, const SkinnedVertex& rhs) {
	bool bResult = MathHelper::IsEqual(lhs.Pos, rhs.Pos) &&
		MathHelper::IsEqual(lhs.Normal, rhs.Normal) &&
		MathHelper::IsEqual(lhs.TexC, rhs.TexC) &&
		MathHelper::IsEqual(lhs.TangentU, rhs.TangentU) &&
		MathHelper::IsEqual(lhs.BoneWeights0, rhs.BoneWeights0) &&
		MathHelper::IsEqual(lhs.BoneWeights1, rhs.BoneWeights1);
	if (bResult != true)
		return false;

	for (size_t i = 0; i < 4; ++i) {
		if (lhs.BoneIndices0[i] != rhs.BoneIndices0[i] || lhs.BoneIndices1[i] != rhs.BoneIndices1[i])
			return false;
	}

	return true;
}

FrameResource::FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, 
		UINT maxInstanceCount, UINT materialCount) {
    ThrowIfFailed(device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(CmdListAlloc.GetAddressOf())));

    PassCB = std::make_unique<UploadBuffer<PassConstants>>(device, passCount, true);
	ObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(device, objectCount, true);
	SsaoCB = std::make_unique<UploadBuffer<SsaoConstants>>(device, 1, true);
	MaterialBuffer = std::make_unique<UploadBuffer<MaterialData>>(device, materialCount, false);
	InstanceBuffer = std::make_unique<UploadBuffer<InstanceData>>(device, objectCount * maxInstanceCount, false);
}