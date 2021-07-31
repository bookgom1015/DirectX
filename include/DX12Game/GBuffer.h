#pragma once

#include "DX12Game/GameCore.h"

class GBuffer {
public:
	GBuffer() = default;
	virtual ~GBuffer() = default;

private:
	GBuffer(const GBuffer& inRef) = delete;
	GBuffer(GBuffer&& inRVal) = delete;
	GBuffer& operator=(const GBuffer& inRef) = delete;
	GBuffer& operator=(GBuffer&& inRVal) = delete;

public:
	GameResult Initialize(
		ID3D12Device* inDevice,
		UINT inClientWidth,
		UINT inClientHeight,
		DXGI_FORMAT inDiffuseMapFormat,
		DXGI_FORMAT inNormalMapFormat);

	GameResult BuildDescriptors(
		ID3D12Resource* depthStencilBuffer,
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv,
		UINT cbvSrvUavDescriptorSize,
		UINT rtvDescriptorSize);

	GameResult OnResize(UINT inClientWidth, UINT inClientHeight);

	CD3DX12_GPU_DESCRIPTOR_HANDLE GetGBufferSrv();
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetGBufferRtv();

private:
	GameResult CreateGBuffer(UINT inClientWidth, UINT inClientHeight);

private:
	ID3D12Device* md3dDevice;

	static const size_t mGBufferSize = 2;
	std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, mGBufferSize> mGBuffer;

	DXGI_FORMAT mDiffuseMapFormat;
	DXGI_FORMAT mNormalMapFormat;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhGpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuRtv;

	UINT mCbvSrvUavDescriptorSize;
	UINT mRtvDescriptorSize;

	UINT mClientWidth;
	UINT mClientHeight;
};