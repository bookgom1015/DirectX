#pragma once

#include "GameCore.h"

class AnimationsMap {
public:
	AnimationsMap(ID3D12Device* inDevice, ID3D12GraphicsCommandList* inCmdList);
	virtual ~AnimationsMap() = default;

public:
	DxResult Initialize();

	UINT AddAnimation(const std::string& inClipName, const GVector<GVector<DirectX::XMFLOAT4>>& inAnimCurves);

	void BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv);

	void UpdateAnimationsMap();

	ID3D12Resource* GetAnimationsMap() const;
	CD3DX12_GPU_DESCRIPTOR_HANDLE AnimationsMapSrv() const;

	UINT GetLineSize() const;
	double GetInvLineSize() const;

private:
	DxResult BuildResource();
	void BuildDescriptors();

public:
	static const DXGI_FORMAT AnimationsMapFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;

private:
	ID3D12Device* md3dDevice;
	ID3D12GraphicsCommandList* mCommandList;

	Microsoft::WRL::ComPtr<ID3D12Resource> mAnimsMap;
	Microsoft::WRL::ComPtr<ID3D12Resource> mAnimsMapUploadBuffer;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhAnimsMapCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhAnimsMapGpuSrv;

	std::vector<DirectX::XMFLOAT4> mAnimations;

	UINT mCurrIndex = 0;
	UINT mNumSubresources;
};