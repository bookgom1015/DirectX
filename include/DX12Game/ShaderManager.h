#pragma once

#include "DX12Game/GameCore.h"

class ShaderManager {
public:
	ShaderManager() = default;
	virtual ~ShaderManager() = default;

public:
	GameResult Initialize(const std::wstring& inFilePath);

	GameResult CompileShaders();

	Microsoft::WRL::ComPtr<ID3DBlob>& GetSkeletonVShader();
	Microsoft::WRL::ComPtr<ID3DBlob>& GetSkeletonGShader();
	Microsoft::WRL::ComPtr<ID3DBlob>& GetSkeletonPShader();
	Microsoft::WRL::ComPtr<ID3DBlob>& GetShadowsVShader();
	Microsoft::WRL::ComPtr<ID3DBlob>& GetSkinnedShadowsVShader();
	Microsoft::WRL::ComPtr<ID3DBlob>& GetShadowsPShader();
	Microsoft::WRL::ComPtr<ID3DBlob>& GetDebugVShader();
	Microsoft::WRL::ComPtr<ID3DBlob>& GetDebugPShader();
	Microsoft::WRL::ComPtr<ID3DBlob>& GetSsaoVShader();
	Microsoft::WRL::ComPtr<ID3DBlob>& GetSsaoPShader();
	Microsoft::WRL::ComPtr<ID3DBlob>& GetSsaoBlurVShader();
	Microsoft::WRL::ComPtr<ID3DBlob>& GetSsaoBlurPShader();
	Microsoft::WRL::ComPtr<ID3DBlob>& GetSkyVShader();
	Microsoft::WRL::ComPtr<ID3DBlob>& GetSkyPShader();
	Microsoft::WRL::ComPtr<ID3DBlob>& GetGBufferVShader();
	Microsoft::WRL::ComPtr<ID3DBlob>& GetSkinnedGBufferVShader();
	Microsoft::WRL::ComPtr<ID3DBlob>& GetGBufferPShader();
	Microsoft::WRL::ComPtr<ID3DBlob>& GetMainPassVShader();
	Microsoft::WRL::ComPtr<ID3DBlob>& GetMainPassPShader();
	Microsoft::WRL::ComPtr<ID3DBlob>& GetSsrVShader();
	Microsoft::WRL::ComPtr<ID3DBlob>& GetSsrPShader();
	Microsoft::WRL::ComPtr<ID3DBlob>& GetSsrBlurVShader();
	Microsoft::WRL::ComPtr<ID3DBlob>& GetSsrBlurPShader();
	Microsoft::WRL::ComPtr<ID3DBlob>& GetPostPassVShader();
	Microsoft::WRL::ComPtr<ID3DBlob>& GetPostPassPShader();
	Microsoft::WRL::ComPtr<ID3DBlob>& GetBloomVShader();
	Microsoft::WRL::ComPtr<ID3DBlob>& GetBloomPShader();
	Microsoft::WRL::ComPtr<ID3DBlob>& GetBloomBlurVShader();
	Microsoft::WRL::ComPtr<ID3DBlob>& GetBloomBlurPShader();

private:
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> mShaders;

	std::wstring mFilePath;
};