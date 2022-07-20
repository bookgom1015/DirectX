#pragma once

#include "LearningDXR/GameResult.h"

#include <d3d12.h>
#include <wrl.h>

struct D3D12BufferCreateInfo {
	UINT64					Size = 0;
	UINT64					Alignment = 0;
	D3D12_HEAP_TYPE			HeapType = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_RESOURCE_FLAGS	Flags = D3D12_RESOURCE_FLAG_NONE;
	D3D12_RESOURCE_STATES	State = D3D12_RESOURCE_STATE_COMMON;

	D3D12BufferCreateInfo() {}
	D3D12BufferCreateInfo(UINT64 InSize, D3D12_RESOURCE_FLAGS InFlags) :
		Size(InSize),
		Flags(InFlags) {}
	D3D12BufferCreateInfo(UINT64 InSize, D3D12_HEAP_TYPE InHeapType, D3D12_RESOURCE_STATES InState) :
		Size(InSize),
		HeapType(InHeapType),
		State(InState) {}
	D3D12BufferCreateInfo(UINT64 InSize, D3D12_RESOURCE_FLAGS InFlags, D3D12_RESOURCE_STATES InState) :
		Size(InSize),
		Flags(InFlags),
		State(InState) {}
	D3D12BufferCreateInfo(UINT64 InSize, UINT64 InAlignment, D3D12_HEAP_TYPE InHeapType, D3D12_RESOURCE_FLAGS InFlags, D3D12_RESOURCE_STATES InState) :
		Size(InSize),
		Alignment(InAlignment),
		HeapType(InHeapType),
		Flags(InFlags),
		State(InState) {}
};

class D3D12Util {
public:
	static UINT CalcConstantBufferByteSize(UINT byteSize) {
		// Constant buffers must be a multiple of the minimum hardware
		// allocation size (usually 256 bytes).  So round up to nearest
		// multiple of 256.  We do this by adding 255 and then masking off
		// the lower 2 bytes which store all bits < 256.
		// Example: Suppose byteSize = 300.
		// (300 + 255) & ~255
		// 555 & ~255
		// 0x022B & ~0x00ff
		// 0x022B & 0xff00
		// 0x0200
		// 512
		return (byteSize + 255) & ~255;
	}

	static GameResult LoadBinary(const std::wstring& inFilename, Microsoft::WRL::ComPtr<ID3DBlob>& outBlob);

	static GameResult CreateDefaultBuffer(
		ID3D12Device* inDevice,
		ID3D12GraphicsCommandList* inCmdList,
		const void* inInitData,
		UINT64 inByteSize,
		Microsoft::WRL::ComPtr<ID3D12Resource>& outUploadBuffer,
		Microsoft::WRL::ComPtr<ID3D12Resource>& outDefaultBuffer);

	static GameResult CreateRootSignature(ID3D12Device* pDevice, const D3D12_ROOT_SIGNATURE_DESC& inRootSignatureDesc, ID3D12RootSignature** ppRootSignature);

	static GameResult CreateBuffer(ID3D12Device* pDevice, D3D12BufferCreateInfo& inInfo, ID3D12Resource** ppResource);
	static GameResult CreateConstantBuffer(ID3D12Device* pDevice, ID3D12Resource** ppResource, UINT64 inSize);
};