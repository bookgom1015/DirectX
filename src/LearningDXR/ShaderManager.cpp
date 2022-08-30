#include "LearningDXR/ShaderManager.h"
#include "LearningDXR/Macros.h"

#include <d3dcompiler.h>

using namespace Microsoft::WRL;

ShaderManager::~ShaderManager() {
	if (!bIsCleanedUp) {
		CleanUp();
	}
}

GameResult ShaderManager::Initialize() {
	ReturnIfFailed(mDxcDllHelper.Initialize());
	ReturnIfFailed(mDxcDllHelper.CreateInstance(CLSID_DxcCompiler, &mCompiler));
	ReturnIfFailed(mDxcDllHelper.CreateInstance(CLSID_DxcLibrary, &mLibrary));
	return GameResultOk;
}

void ShaderManager::CleanUp() {
	ReleaseCom(mCompiler);
	ReleaseCom(mLibrary);
	//mDxcDllHelper.Cleanup();
	bIsCleanedUp = true;
}

GameResult ShaderManager::CompileShader(
		const std::wstring& inFilePath,
		const D3D_SHADER_MACRO* inDefines,
		const std::string& inEntryPoint,
		const std::string& inTarget,
		const std::string& inName) {
#if defined(_DEBUG)  
	UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT compileFlags = 0;
#endif

	HRESULT hr = S_OK;

	ComPtr<ID3DBlob> byteCode = nullptr;
	ComPtr<ID3DBlob> errors;
	hr = D3DCompileFromFile(inFilePath.c_str(), inDefines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		inEntryPoint.c_str(), inTarget.c_str(), compileFlags, 0, &mShaders[inName], &errors);

	std::wstringstream wsstream;
	if (errors != nullptr)
		wsstream << reinterpret_cast<char*>(errors->GetBufferPointer());

	if (FAILED(hr)) {
		ReturnGameResult(hr, wsstream.str());
	}

	return GameResultOk;
}

GameResult ShaderManager::CompileShader(const D3D12ShaderInfo& inShaderInfo, const std::string& inName) {
	UINT32 code = 0;
	IDxcBlobEncoding* shaderText = nullptr;
	ReturnIfFailed(mLibrary->CreateBlobFromFile(inShaderInfo.FileName, &code, &shaderText));

	ComPtr<IDxcIncludeHandler> includeHandler;
	ReturnIfFailed(mLibrary->CreateIncludeHandler(&includeHandler));

	IDxcOperationResult* result;
	ReturnIfFailed(mCompiler->Compile(
		shaderText,
		inShaderInfo.FileName,
		inShaderInfo.EntryPoint,
		inShaderInfo.TargetProfile,
		inShaderInfo.Arguments,
		inShaderInfo.ArgCount,
		inShaderInfo.Defines,
		inShaderInfo.DefineCount,
		includeHandler.Get(),
		&result
	));

	HRESULT hr;
	ReturnIfFailed(result->GetStatus(&hr));
	if (FAILED(hr)) {
		IDxcBlobEncoding* error;
		ReturnIfFailed(result->GetErrorBuffer(&error));

		auto bufferSize = error->GetBufferSize();
		std::vector<char> infoLog(bufferSize + 1);
		std::memcpy(infoLog.data(), error->GetBufferPointer(), bufferSize);
		infoLog[bufferSize] = 0;

		std::string errorMsg = "Shader Compiler Error:\n";
		errorMsg.append(infoLog.data());

		std::wstring errorMsgW;
		errorMsgW.assign(errorMsg.begin(), errorMsg.end());

		ReturnGameResult(E_FAIL, errorMsgW);
	}

	ReturnIfFailed(result->GetResult(&mRTShaders[inName]));

	return GameResultOk;
}

ID3DBlob* ShaderManager::GetShader(const std::string& inName) {
	return mShaders[inName].Get();
}

IDxcBlob* ShaderManager::GetRTShader(const std::string& inName) {
	return mRTShaders[inName].Get();
}