#pragma once

#include "DX12Game/GameCore.h"

template<typename T>
class GameUploadBuffer {
public:
	GameUploadBuffer() = default;
	virtual ~GameUploadBuffer();

private:
	GameUploadBuffer(const GameUploadBuffer& inRef) = delete;
	GameUploadBuffer(GameUploadBuffer&& inRVal) = delete;
	GameUploadBuffer& operator=(const GameUploadBuffer& inRef) = delete;
	GameUploadBuffer& operator=(GameUploadBuffer&& inRVal) = delete;

public:
	GameResult Initialize(ID3D12Device* inDevice, UINT inElementCount, bool inIsConstantBuffer);

	ID3D12Resource* Resource() const;

	void CopyData(int inElementIndex, const T& inData);

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> mUploadBuffer;
	BYTE* mMappedData = nullptr;

	UINT mElementByteSize = 0;
	bool mIsConstantBuffer = false;
};

#include "DX12Game/GameUploadBuffer.inl"