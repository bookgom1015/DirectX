#pragma once

#include "GameResult.h"

#include <d3d12.h>
#include <dxcapi.h>
#include <dxc/Support/dxcapi.use.h>
#include <wrl.h>

struct D3D12ShaderInfo {
	LPCWSTR		FileName		= nullptr;
	LPCWSTR		EntryPoint		= nullptr;
	LPCWSTR		TargetProfile	= nullptr;
	LPCWSTR*	Arguments		= nullptr;
	DxcDefine*	Defines			= nullptr;
	UINT32		ArgCount		= 0;
	UINT32		DefineCount		= 0;

	D3D12ShaderInfo() = default;
	D3D12ShaderInfo(LPCWSTR inFileName, LPCWSTR inEntryPoint, LPCWSTR inProfile) {
		FileName = inFileName;
		EntryPoint = inEntryPoint;
		TargetProfile = inProfile;
	}
};

class ShaderManager {
public:
	ShaderManager() = default;
	virtual ~ShaderManager();

public:
	GameResult Initialize();
	void CleanUp();

	GameResult CompileShader(
		const std::wstring& inFilePath,
		const D3D_SHADER_MACRO* inDefines,
		const std::string& inEntryPoint,
		const std::string& inTarget,
		const std::string& inName);

	GameResult CompileShader(const D3D12ShaderInfo& inShaderInfo, const std::string& inName);

	Microsoft::WRL::ComPtr<ID3DBlob>& GetShader(const std::string& inName);
	Microsoft::WRL::ComPtr<IDxcBlob>& GetRTShader(const std::string& inName);

private:
	bool bIsCleanedUp = false;

	dxc::DxcDllSupport mDxcDllHelper;
	IDxcCompiler* mCompiler = nullptr;
	IDxcLibrary* mLibrary = nullptr;

	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<IDxcBlob>> mRTShaders;
};