#include "DX12Game/ShaderManager.h"

using namespace Microsoft::WRL;

namespace {
	const D3D_SHADER_MACRO AlphaTestDefines[] = {
		"ALPHA_TEST", "1",
		"BLUR_RADIUS_3", "1",
		NULL, NULL
	};

	const D3D_SHADER_MACRO SkinnedDefines[] = {
		"SKINNED", "1",
		NULL, NULL
	};
}

GameResult ShaderManager::Initialize() {
	return GameResultOk;
}

GameResult ShaderManager::CompileShader(
		const std::wstring& inFilePath,
		const D3D_SHADER_MACRO* inDefines,
		const std::string& inEntryPoint,
		const std::string& inTarget,
		const std::string& inName) {
	CheckGameResult(D3D12Util::CompileShader(inFilePath, inDefines, inEntryPoint, inTarget, mShaders[inName]));
	return GameResultOk;
}

ComPtr<ID3DBlob>& ShaderManager::GetShader(const std::string& inName) {
	return mShaders[inName];
}

DxcShaderManager::~DxcShaderManager() {
	if (!bIsCleanedUp)
		CleanUp();
}

GameResult DxcShaderManager::Initialize() {
	ReturnIfFailed(mDxcDllHelper.Initialize());
	ReturnIfFailed(mDxcDllHelper.CreateInstance(CLSID_DxcCompiler, &mCompiler));
	ReturnIfFailed(mDxcDllHelper.CreateInstance(CLSID_DxcLibrary, &mLibrary));
	return GameResultOk;
}

void DxcShaderManager::CleanUp() {
	ReleaseCom(mCompiler);
	ReleaseCom(mLibrary);
	mDxcDllHelper.Cleanup();
	bIsCleanedUp = true;
}

GameResult DxcShaderManager::CompileShader(const D3D12ShaderInfo& inShaderInfo, const std::string& inName) {
	CheckGameResult(D3D12Util::CompileShader(mCompiler, mLibrary, inShaderInfo, &mShaders[inName]));
	return GameResultOk;
}

IDxcBlob* DxcShaderManager::GetShader(const std::string& inName) {
	return mShaders[inName];
}