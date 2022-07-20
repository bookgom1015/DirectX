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

	PassCB.Initialize(Device, PassCount, true);
	ObjectCB.Initialize(Device, ObjectCount, true);
	MaterialCB.Initialize(Device, MaterialCount, false);

	return GameResult(S_OK);
}

DXRFrameResource::DXRFrameResource(ID3D12Device* inDevice, UINT inPassCount, UINT inMaterialCount) :
		PassCount(inPassCount),
		MaterialCount(inMaterialCount) {
	Device = inDevice;
}

GameResult DXRFrameResource::Initialize() {
	ReturnIfFailed(Device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(CmdListAlloc.GetAddressOf())
	));

	PassCB.Initialize(Device, PassCount, true);
	MaterialCB.Initialize(Device, MaterialCount, false);

	return GameResult(S_OK);
}