#pragma once

#include "DX12Game/GameCore.h"

class RootSignatureManager {
public:
	RootSignatureManager() = default;
	virtual ~RootSignatureManager() = default;

public:
	GameResult Initialize(ID3D12Device* inDevice);

	GameResult BuildRootSignatures();

	ID3D12RootSignature* GetBasicRootSignature();
	ID3D12RootSignature* GetSsaoRootSignature();
	ID3D12RootSignature* GetPostPassRootSignature();
	ID3D12RootSignature* GetSsrRootSignature();
	ID3D12RootSignature* GetBloomRootSignature();

	UINT GetObjectCBIndex();
	UINT GetPassCBIndex();
	UINT GetInstIdxBufferIndex();
	UINT GetInstBufferIndex();
	UINT GetMatBufferIndex();
	UINT GetMiscTextureMapIndex();
	UINT GetTextureMapIndex();
	UINT GetConstSettingsIndex();
	UINT GetAnimationsMapIndex();

private:
	GameResult BuildBasicRootSignature();
	GameResult BuildSsaoRootSignature();
	GameResult BuildPostPassRootSignature();
	GameResult BuildSsrRootSignature();
	GameResult BuildBloomRootSignature();

private:
	ID3D12Device* md3dDevice;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> mBasicRootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mSsaoRootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mPostPassRootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mSsrRootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mBloomRootSignature = nullptr;

	UINT mObjectCBIndex;
	UINT mPassCBIndex;
	UINT mInstIdxBufferIndex;
	UINT mInstBufferIndex;
	UINT mMatBufferIndex;
	UINT mMiscTextureMapIndex;
	UINT mTextureMapIndex;
	UINT mConstSettingsIndex;
	UINT mAnimationsMapIndex;
};