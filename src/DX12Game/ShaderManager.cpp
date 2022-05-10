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

GameResult ShaderManager::Initialize(const std::wstring& inFilePath) {
	mFilePath = inFilePath;

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