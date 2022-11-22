#include "LearningDXR/FrameResource.h"
#include "LearningDXR/Logger.h"

FrameResource::FrameResource(ID3D12Device* inDevice, UINT inPassCount, UINT inObjectCount, UINT inMaterialCount) :
		PassCount(inPassCount),
		ObjectCount(inObjectCount),
		MaterialCount(inMaterialCount) {
	Device = inDevice;
	Fence = 0;
}

bool FrameResource::Initialize() {
	CheckHResult(Device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(CmdListAlloc.GetAddressOf())
	));

	CheckIsValid(PassCB.Initialize(Device, PassCount, true));
	CheckIsValid(ObjectCB.Initialize(Device, ObjectCount, true));
	CheckIsValid(MaterialCB.Initialize(Device, MaterialCount, true));
	CheckIsValid(DebugPassCB.Initialize(Device, PassCount, true));

	return true;
}