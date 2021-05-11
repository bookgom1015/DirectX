#pragma once

#include "GameCore.h"

class AnimationsMap {
public:
	AnimationsMap(ID3D12Device* inDevice, UINT inWidth = 512, UINT inHeight = 512);
	virtual ~AnimationsMap();

public:
	UINT AddAnimation(const std::string& inClipName, const std::vector<DirectX::XMFLOAT4X4>& inTransforms);

	void BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv);
	void BuildAnimationsMap(ID3D12GraphicsCommandList* inCmdList);

	ID3D12Resource* GetAnimationsMap() const;
	CD3DX12_GPU_DESCRIPTOR_HANDLE AnimationsMapSrv() const;

private:
	void BuildResource();
	void BuildDescriptors();

public:
	static const DXGI_FORMAT AnimationsMapFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;

private:
	ID3D12Device* md3dDevice;

	Microsoft::WRL::ComPtr<ID3D12Resource> mAnimsMap;
	Microsoft::WRL::ComPtr<ID3D12Resource> mAnimsMapUploadBuffer;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhAnimsMapCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhAnimsMapGpuSrv;

	UINT mAnimsMapWidth;
	UINT mAnimsMapHeight;

	std::vector<DirectX::XMFLOAT4X4> mAnimations;
	std::unordered_map<std::string /* Clip name */, UINT /* Index */> mClipsIndex;

	UINT mCurrIndex = 0;
	UINT mNumSubresources;
};