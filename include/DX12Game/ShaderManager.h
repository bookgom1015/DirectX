#pragma once

#include "DX12Game/GameCore.h"

class ShaderManager {
public:
	ShaderManager() = default;
	virtual ~ShaderManager() = default;

public:
	GameResult Initialize(const std::wstring& inFilePath);

	GameResult CompileShader(
		const std::wstring& inFilePath, 
		const D3D_SHADER_MACRO* inDefines, 
		const std::string& inEntryPoint, 
		const std::string& inTarget, 
		const std::string& inName);

	Microsoft::WRL::ComPtr<ID3DBlob>& GetShader(const std::string& inName);

private:
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> mShaders;

	std::wstring mFilePath;
};