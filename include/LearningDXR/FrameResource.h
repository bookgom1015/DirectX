#pragma once

#include "LearningDXR/GameResult.h"
#include "LearningDXR/D3D12Util.h"
#include "common/d3dx12.h"

#include <DirectXMath.h>
#include <Windows.h>
#include <wrl.h>

#define MaxLights 16

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
	GameResult Initialize(ID3D12Device* inDevice, UINT inElementCount, bool inIsConstantBuffer) {
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

	ID3D12Resource* Resource() const {
		return mUploadBuffer.Get();
	}

	void CopyData(int inElementIndex, const T& inData) {
		std::memcpy(&mMappedData[inElementIndex * mElementByteSize], &inData, sizeof(T));
	}

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> mUploadBuffer;
	BYTE* mMappedData = nullptr;

	UINT mElementByteSize = 0;
	bool mIsConstantBuffer = false;
};

struct Light {
	DirectX::XMFLOAT3 Strength = { 0.5f, 0.5f, 0.5f };
	float FalloffStart = 1.0f;								// point/spot light only
	DirectX::XMFLOAT3 Direction = { 0.0f, -1.0f, 0.0f };	// directional/spot light only
	float FalloffEnd = 10.0f;								// point/spot light only
	DirectX::XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };		// point/spot light only
	float SpotPower = 64.0f;								// spot light only
};

struct ObjectConstants {
	DirectX::XMFLOAT4X4 World;
};

struct PassConstants {
	DirectX::XMFLOAT4X4 View;
	DirectX::XMFLOAT4X4 InvView;
	DirectX::XMFLOAT4X4 Proj;
	DirectX::XMFLOAT4X4 InvProj;
	DirectX::XMFLOAT4X4 ViewProj;
	DirectX::XMFLOAT4X4 InvViewProj;
	DirectX::XMFLOAT3 EyePosW;
	float CBPerObjectPad1;
	DirectX::XMFLOAT4 AmbientLight;
	Light Lights[MaxLights];
};

struct MaterialConstants {
	DirectX::XMFLOAT4 DiffuseAlbedo;
	DirectX::XMFLOAT3 FresnelR0;
	float Roughness;
	DirectX::XMFLOAT4X4 MatTransform;
};

struct FrameResource {
public:
	FrameResource(
		ID3D12Device* inDevice,
		UINT inPassCount,
		UINT inObjectCount,
		UINT inMaterialCount);
	virtual ~FrameResource() = default;

private:
	FrameResource(const FrameResource& src) = delete;
	FrameResource(FrameResource&& src) = delete;
	FrameResource& operator=(const FrameResource& rhs) = delete;
	FrameResource& operator=(FrameResource&& rhs) = delete;

public:
	GameResult Initialize();

public:
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;

	UploadBuffer<PassConstants> PassCB;
	UploadBuffer<ObjectConstants> ObjectCB;
	UploadBuffer<MaterialConstants> MaterialCB;

	UINT64 Fence = 0;

	ID3D12Device* Device;
	UINT PassCount;
	UINT ObjectCount;
	UINT MaterialCount;
};

struct DXRPassConstants {
	DirectX::XMFLOAT4X4	View;
	DirectX::XMFLOAT4X4 InvView;
	DirectX::XMFLOAT4X4 Proj;
	DirectX::XMFLOAT4X4 InvProj;
	DirectX::XMFLOAT4X4 ViewProj;
	DirectX::XMFLOAT4X4 InvViewProj;
	DirectX::XMFLOAT3	EyePosW;
	float				PassConsantPad0;
	DirectX::XMFLOAT2	Resolution;
	float				PassConsantPad1;
	float				PassConsantPad2;
};

struct DXRMaterialConstants {
	DirectX::XMFLOAT4	DiffuseAlbedo;
	DirectX::XMFLOAT3	FresnelR0;
	float				Roughness;
	DirectX::XMFLOAT4	Resolution;
};

struct DXRFrameResource {
public:
	DXRFrameResource(
		ID3D12Device* inDevice,
		UINT inPassCount,
		UINT inMaterialCount);
	virtual ~DXRFrameResource() = default;

private:
	DXRFrameResource(const FrameResource& src) = delete;
	DXRFrameResource(FrameResource&& src) = delete;
	DXRFrameResource& operator=(const FrameResource& rhs) = delete;
	DXRFrameResource& operator=(FrameResource&& rhs) = delete;

public:
	GameResult Initialize();

public:
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;

	UploadBuffer<DXRPassConstants> PassCB;
	UploadBuffer<DXRMaterialConstants> MaterialCB;

	UINT64 Fence = 0;

	ID3D12Device* Device;
	UINT PassCount;
	UINT MaterialCount;
};