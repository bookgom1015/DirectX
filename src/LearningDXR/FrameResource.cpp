#include "LearningDXR/FrameResource.h"

FrameResource::FrameResource(ID3D12Device* inDevice, UINT inPassCount, UINT inObjectCount, UINT inMaterialCount) :
		PassCount(inPassCount),
		ObjectCount(inObjectCount),
		MaterialCount(inMaterialCount) {
	Device = inDevice;
}

GameResult FrameResource::Initialize() {
	ReturnIfFailed(Device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(CmdListAlloc.GetAddressOf())
	));

	CheckGameResult(PassCB.Initialize(Device, PassCount, true));
	CheckGameResult(ObjectCB.Initialize(Device, ObjectCount, true));
	CheckGameResult(MaterialCB.Initialize(Device, MaterialCount, true));

	return GameResult(S_OK);
}