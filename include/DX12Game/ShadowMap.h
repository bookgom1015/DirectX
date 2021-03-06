//***************************************************************************************
// ShadowMap.h by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

#pragma once

#include "DX12Game/GameCore.h"

enum class CubeMapFace : int {
	PositiveX = 0,
	NegativeX = 1,
	PositiveY = 2,
	NegativeY = 3,
	PositiveZ = 4,
	NegativeZ = 5
};

class ShadowMap {
public:
	ShadowMap() = default;
	virtual ~ShadowMap() = default;

private:
	ShadowMap(const ShadowMap& src) = delete;
	ShadowMap(ShadowMap&& src) = delete;
	ShadowMap& operator=(const ShadowMap& rhs) = delete;
	ShadowMap& operator=(ShadowMap&& rhs) = delete;

public:
	GameResult Initialize(ID3D12Device* inDevice, UINT inWidth, UINT inHeight);

	UINT Width() const;
	UINT Height() const;
	ID3D12Resource* Resource();
	CD3DX12_GPU_DESCRIPTOR_HANDLE Srv() const;
	CD3DX12_CPU_DESCRIPTOR_HANDLE Dsv() const;

	D3D12_VIEWPORT Viewport() const;
	D3D12_RECT ScissorRect() const;

	void BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsv);

	GameResult OnResize(UINT inNewWidth, UINT inNewHeight);

private:
	void BuildDescriptors();
	GameResult BuildResource();

private:
	ID3D12Device* md3dDevice = nullptr;

	D3D12_VIEWPORT mViewport;
	D3D12_RECT mScissorRect;

	UINT mWidth = 0;
	UINT mHeight = 0;
	DXGI_FORMAT mFormat = DXGI_FORMAT_R24G8_TYPELESS;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhGpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuDsv;

	Microsoft::WRL::ComPtr<ID3D12Resource> mShadowMap = nullptr;
};