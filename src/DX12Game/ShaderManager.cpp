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
	CheckGameResult(mShaderCompiler.DxcDllHelper.Initialize());
	CheckGameResult(mShaderCompiler.DxcDllHelper.CreateInstance(CLSID_DxcCompiler, &mShaderCompiler.Compiler));
	CheckGameResult(mShaderCompiler.DxcDllHelper.CreateInstance(CLSID_DxcLibrary, &mShaderCompiler.Library));
	return GameResultOk;
}

void DxcShaderManager::CleanUp() {
	ReleaseCom(mShaderCompiler.Compiler);
	ReleaseCom(mShaderCompiler.Library);
	mShaderCompiler.DxcDllHelper.Cleanup();

	bIsCleanedUp = true;
}

GameResult DxcShaderManager::CompileShaader(RaytracingProgram& inProgram) {
	CheckGameResult(D3D12Util::CompileShader(mShaderCompiler, inProgram.Info, &inProgram.Blob));
	inProgram.SetBytecode();
	return GameResultOk;
}