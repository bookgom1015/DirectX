#pragma once

#include <d3d12.h>
#include <wrl.h>

struct AccelerationStructureBuffer {
	Microsoft::WRL::ComPtr<ID3D12Resource> Scratch		= nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> Result		= nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> InstanceDesc	= nullptr;	// only used in top-level AS
	UINT64 ResultDataMaxSizeInBytes;
};