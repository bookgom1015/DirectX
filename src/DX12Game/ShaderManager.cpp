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

GameResult ShaderManager::CompileShaders() {
	const std::wstring skeletonhlsl = mFilePath + L"Skeleton.hlsl";
	CheckGameResult(D3D12Util::CompileShader(skeletonhlsl, nullptr, "VS", "vs_5_1", mShaders["skeletonVS"]));
	CheckGameResult(D3D12Util::CompileShader(skeletonhlsl, SkinnedDefines, "GS", "gs_5_1", mShaders["skeletonGS"]));
	CheckGameResult(D3D12Util::CompileShader(skeletonhlsl, AlphaTestDefines, "PS", "ps_5_1", mShaders["skeletonPS"]));

	const std::wstring shadowhlsl = mFilePath + L"Shadows.hlsl";
	CheckGameResult(D3D12Util::CompileShader(shadowhlsl, nullptr, "VS", "vs_5_1", mShaders["shadowVS"]));
	CheckGameResult(D3D12Util::CompileShader(shadowhlsl, SkinnedDefines, "VS", "vs_5_1", mShaders["skinnedShadowVS"]));
	CheckGameResult(D3D12Util::CompileShader(shadowhlsl, AlphaTestDefines, "PS", "ps_5_1", mShaders["shadowPS"]));

	const std::wstring debughlsl = mFilePath + L"DrawDebug.hlsl";
	CheckGameResult(D3D12Util::CompileShader(debughlsl, nullptr, "VS", "vs_5_1", mShaders["debugVS"]));
	CheckGameResult(D3D12Util::CompileShader(debughlsl, nullptr, "PS", "ps_5_1", mShaders["debugPS"]));

	const std::wstring ssaohlsl = mFilePath + L"Ssao.hlsl";
	CheckGameResult(D3D12Util::CompileShader(ssaohlsl, nullptr, "VS", "vs_5_1", mShaders["ssaoVS"]));
	CheckGameResult(D3D12Util::CompileShader(ssaohlsl, nullptr, "PS", "ps_5_1", mShaders["ssaoPS"]));

	const std::wstring ssaoblurhlsl = mFilePath + L"SsaoBlur.hlsl";
	CheckGameResult(D3D12Util::CompileShader(ssaoblurhlsl, nullptr, "VS", "vs_5_1", mShaders["ssaoBlurVS"]));
	CheckGameResult(D3D12Util::CompileShader(ssaoblurhlsl, nullptr, "PS", "ps_5_1", mShaders["ssaoBlurPS"]));

	const std::wstring skyhlsl = mFilePath + L"Sky.hlsl";
	CheckGameResult(D3D12Util::CompileShader(skyhlsl, nullptr, "VS", "vs_5_1", mShaders["skyVS"]));
	CheckGameResult(D3D12Util::CompileShader(skyhlsl, nullptr, "PS", "ps_5_1", mShaders["skyPS"]));

	const std::wstring gbufferhlsl = mFilePath + L"DrawGBuffer.hlsl";
	CheckGameResult(D3D12Util::CompileShader(gbufferhlsl, nullptr, "VS", "vs_5_1", mShaders["gbufferVS"]));
	CheckGameResult(D3D12Util::CompileShader(gbufferhlsl, SkinnedDefines, "VS", "vs_5_1", mShaders["skinnedGBufferVS"]));
	CheckGameResult(D3D12Util::CompileShader(gbufferhlsl, AlphaTestDefines, "PS", "ps_5_1", mShaders["gbufferPS"]));

	const std::wstring defaultghlsl = mFilePath + L"DefaultUsingGBuffer.hlsl";
	CheckGameResult(D3D12Util::CompileShader(defaultghlsl, nullptr, "VS", "vs_5_1", mShaders["defaultGBufferVS"]));
	CheckGameResult(D3D12Util::CompileShader(defaultghlsl, nullptr, "PS", "ps_5_1", mShaders["defaultGBufferPS"]));

	const std::wstring ssrhlsl = mFilePath + L"Ssr.hlsl";
	CheckGameResult(D3D12Util::CompileShader(ssrhlsl, nullptr, "VS", "vs_5_1", mShaders["ssrVS"]));
	CheckGameResult(D3D12Util::CompileShader(ssrhlsl, nullptr, "PS", "ps_5_1", mShaders["ssrPS"]));

	const std::wstring ssrBlurhlsl = mFilePath + L"SsrBlur.hlsl";
	CheckGameResult(D3D12Util::CompileShader(ssrBlurhlsl, nullptr, "VS", "vs_5_1", mShaders["ssrBlurVS"]));
	CheckGameResult(D3D12Util::CompileShader(ssrBlurhlsl, nullptr, "PS", "ps_5_1", mShaders["ssrBlurPS"]));

	const std::wstring postPassHlsl = mFilePath + L"PostPass.hlsl";
	CheckGameResult(D3D12Util::CompileShader(postPassHlsl, nullptr, "VS", "vs_5_1", mShaders["postPassVS"]));
	CheckGameResult(D3D12Util::CompileShader(postPassHlsl, nullptr, "PS", "ps_5_1", mShaders["postPassPS"]));

	const std::wstring bloomHlsl = mFilePath + L"Bloom.hlsl";
	CheckGameResult(D3D12Util::CompileShader(bloomHlsl, nullptr, "VS", "vs_5_1", mShaders["bloomVS"]));
	CheckGameResult(D3D12Util::CompileShader(bloomHlsl, nullptr, "PS", "ps_5_1", mShaders["bloomPS"]));

	const std::wstring bloomBlurHlsl = mFilePath + L"BloomBlur.hlsl";
	CheckGameResult(D3D12Util::CompileShader(bloomBlurHlsl, nullptr, "VS", "vs_5_1", mShaders["bloomBlurVS"]));
	CheckGameResult(D3D12Util::CompileShader(bloomBlurHlsl, nullptr, "PS", "ps_5_1", mShaders["bloomBlurPS"]));

	return GameResultOk;
}

ComPtr<ID3DBlob>& ShaderManager::GetSkeletonVShader() {
	return mShaders["skeletonVS"];
}

ComPtr<ID3DBlob>& ShaderManager::GetSkeletonGShader() {
	return mShaders["skeletonGS"];
}

ComPtr<ID3DBlob>& ShaderManager::GetSkeletonPShader() {
	return mShaders["skeletonPS"];
}

ComPtr<ID3DBlob>& ShaderManager::GetShadowsVShader() {
	return mShaders["shadowVS"];
}

ComPtr<ID3DBlob>& ShaderManager::GetSkinnedShadowsVShader() {
	return mShaders["skinnedShadowVS"];
}

ComPtr<ID3DBlob>& ShaderManager::GetShadowsPShader() {
	return mShaders["shadowPS"];
}

ComPtr<ID3DBlob>& ShaderManager::GetDebugVShader() {
	return mShaders["debugVS"];
}

ComPtr<ID3DBlob>& ShaderManager::GetDebugPShader() {
	return mShaders["debugPS"];
}

ComPtr<ID3DBlob>& ShaderManager::GetSsaoVShader() {
	return mShaders["ssaoVS"];
}

ComPtr<ID3DBlob>& ShaderManager::GetSsaoPShader() {
	return mShaders["ssaoPS"];
}

ComPtr<ID3DBlob>& ShaderManager::GetSsaoBlurVShader() {
	return mShaders["ssaoBlurVS"];
}

ComPtr<ID3DBlob>& ShaderManager::GetSsaoBlurPShader() {
	return mShaders["ssaoBlurPS"];
}

ComPtr<ID3DBlob>& ShaderManager::GetSkyVShader() {
	return mShaders["skyVS"];
}

ComPtr<ID3DBlob>& ShaderManager::GetSkyPShader() {
	return mShaders["skyPS"];
}

ComPtr<ID3DBlob>& ShaderManager::GetGBufferVShader() {
	return mShaders["gbufferVS"];
}

ComPtr<ID3DBlob>& ShaderManager::GetSkinnedGBufferVShader() {
	return mShaders["skinnedGBufferVS"];
}

ComPtr<ID3DBlob>& ShaderManager::GetGBufferPShader() {
	return mShaders["gbufferPS"];
}

ComPtr<ID3DBlob>& ShaderManager::GetMainPassVShader() {
	return mShaders["defaultGBufferVS"];
}

ComPtr<ID3DBlob>& ShaderManager::GetMainPassPShader() {
	return mShaders["defaultGBufferPS"];
}

ComPtr<ID3DBlob>& ShaderManager::GetSsrVShader() {
	return mShaders["ssrVS"];
}

ComPtr<ID3DBlob>& ShaderManager::GetSsrPShader() {
	return mShaders["ssrPS"];
}

ComPtr<ID3DBlob>& ShaderManager::GetSsrBlurVShader() {
	return mShaders["ssrBlurVS"];
}

ComPtr<ID3DBlob>& ShaderManager::GetSsrBlurPShader() {
	return mShaders["ssrBlurPS"];
}

ComPtr<ID3DBlob>& ShaderManager::GetPostPassVShader() {
	return mShaders["postPassVS"];
}

ComPtr<ID3DBlob>& ShaderManager::GetPostPassPShader() {
	return mShaders["postPassPS"];
}

ComPtr<ID3DBlob>& ShaderManager::GetBloomVShader() {
	return mShaders["bloomVS"];
}

ComPtr<ID3DBlob>& ShaderManager::GetBloomPShader() {
	return mShaders["bloomPS"];
}

ComPtr<ID3DBlob>& ShaderManager::GetBloomBlurVShader() {
	return mShaders["bloomBlurVS"];
}

ComPtr<ID3DBlob>& ShaderManager::GetBloomBlurPShader() {
	return mShaders["bloomBlurPS"];
}