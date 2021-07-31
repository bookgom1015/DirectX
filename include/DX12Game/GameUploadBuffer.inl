#ifndef __GAMEUPLOADBUFFER_INL__
#define __GAMEUPLOADBUFFER_INL__

template <typename T>
GameUploadBuffer<T>::~GameUploadBuffer() {
	if (mUploadBuffer != nullptr)
		mUploadBuffer->Unmap(0, nullptr);

	mMappedData = nullptr;
}

template <typename T>
GameResult GameUploadBuffer<T>::Initialize(ID3D12Device* inDevice, UINT inElementCount, bool inIsConstantBuffer) {
	mIsConstantBuffer = inIsConstantBuffer;

	mElementByteSize = sizeof(T);

	// Constant buffer elements need to be multiples of 256 bytes.
	// This is because the hardware can only view constant data 
	// at m*256 byte offsets and of n*256 byte lengths. 
	// typedef struct D3D12_CONSTANT_BUFFER_VIEW_DESC {
	// UINT64 OffsetInBytes; // multiple of 256
	// UINT   SizeInBytes;   // multiple of 256
	// } D3D12_CONSTANT_BUFFER_VIEW_DESC;
	if (inIsConstantBuffer)
		mElementByteSize = D3D12Util::CalcConstantBufferByteSize(sizeof(T));

	ReturnIfFailed(inDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(mElementByteSize * inElementCount),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&mUploadBuffer)));

	ReturnIfFailed(mUploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mMappedData)));

	// We do not need to unmap until we are done with the resource.  However, we must not write to
	// the resource while it is in use by the GPU (so we must use synchronization techniques).

	return GameResult(S_OK);
}

template <typename T>
ID3D12Resource* GameUploadBuffer<T>::Resource() const {
	return mUploadBuffer.Get();
}

template <typename T>
void GameUploadBuffer<T>::CopyData(int inElementIndex, const T& inData) {
	std::memcpy(&mMappedData[inElementIndex * mElementByteSize], &inData, sizeof(T));
}

#endif // __GAMEUPLOADBUFFER_INL__