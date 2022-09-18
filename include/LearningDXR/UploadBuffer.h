#pragma once

#include <Windows.h>

#include "common/d3dx12.h"
#include "LearningDXR/D3D12Util.h"

template<typename T>
class UploadBuffer {
public:
	UploadBuffer() = default;
	virtual ~UploadBuffer() {
		if (mUploadBuffer != nullptr)
			mUploadBuffer->Unmap(0, nullptr);
		mMappedData = nullptr;
	}

private:
	UploadBuffer(const UploadBuffer& inRef) = delete;
	UploadBuffer(UploadBuffer&& inRVal) = delete;
	UploadBuffer& operator=(const UploadBuffer& inRef) = delete;
	UploadBuffer& operator=(UploadBuffer&& inRVal) = delete;

public:
	GameResult Initialize(ID3D12Device* device, UINT elementCount, bool isConstantBuffer) {
		mIsConstantBuffer = isConstantBuffer;

		mElementByteSize = sizeof(T);

		// Constant buffer elements need to be multiples of 256 bytes.
		// This is because the hardware can only view constant data 
		// at m*256 byte offsets and of n*256 byte lengths. 
		// typedef struct D3D12_CONSTANT_BUFFER_VIEW_DESC {
		// UINT64 OffsetInBytes; // multiple of 256
		// UINT   SizeInBytes;   // multiple of 256
		// } D3D12_CONSTANT_BUFFER_VIEW_DESC;
		if (isConstantBuffer)
			mElementByteSize = D3D12Util::CalcConstantBufferByteSize(sizeof(T));

		ReturnIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(mElementByteSize * elementCount),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&mUploadBuffer)));

		ReturnIfFailed(mUploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mMappedData)));

		// We do not need to unmap until we are done with the resource.  However, we must not write to
		// the resource while it is in use by the GPU (so we must use synchronization techniques).

		return GameResult(S_OK);
	}

	ID3D12Resource* Resource() const {
		return mUploadBuffer.Get();
	}

	void CopyData(int elementIndex, const T& data) {
		std::memcpy(&mMappedData[elementIndex * mElementByteSize], &data, sizeof(T));
	}

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> mUploadBuffer;
	BYTE* mMappedData = nullptr;

	UINT mElementByteSize = 0;
	bool mIsConstantBuffer = false;
};