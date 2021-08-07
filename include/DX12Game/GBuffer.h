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
		DXGI_FORMAT inNormalMapFormat,
		DXGI_FORMAT inDepthMapFormat);

	void BuildDescriptors(
		ID3D12Resource* inDepthStencilBuffer,
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv,
		UINT inCbvSrvUavDescriptorSize,
		UINT inRtvDescriptorSize);

	GameResult OnResize(UINT inClientWidth, UINT inClientHeight, ID3D12Resource* inDepthStencilBuffer);

	ID3D12Resource* GetDiffuseMap();
	CD3DX12_GPU_DESCRIPTOR_HANDLE GetDiffuseMapSrv() const;
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetDiffuseMapRtv() const;

	ID3D12Resource* GetNormalMap();
	CD3DX12_GPU_DESCRIPTOR_HANDLE GetNormalMapSrv() const;
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetNormalMapRtv() const;

	CD3DX12_GPU_DESCRIPTOR_HANDLE GetDepthMapSrv() const;

	DXGI_FORMAT GetDiffuseMapFormat() const;
	DXGI_FORMAT GetNormalMapFormat() const;
	DXGI_FORMAT GetDepthMapFormat() const;

private:
	GameResult BuildResources();
	void BuildGBuffer(ID3D12Resource* inDepthStencilBuffer);

private:
	ID3D12Device* md3dDevice;

	Microsoft::WRL::ComPtr<ID3D12Resource> mDiffuseMap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> mNormalMap = nullptr;

	DXGI_FORMAT mDiffuseMapFormat;
	DXGI_FORMAT mNormalMapFormat;
	DXGI_FORMAT mDepthMapFormat;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhDiffuseMapCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhDiffuseMapGpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhDiffuseMapCpuRtv;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhNormalMapCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhNormalMapGpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhNormalMapCpuRtv;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhDepthMapCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhDepthMapGpuSrv;

	UINT mCbvSrvUavDescriptorSize;
	UINT mRtvDescriptorSize;

	UINT mClientWidth;
	UINT mClientHeight;
};