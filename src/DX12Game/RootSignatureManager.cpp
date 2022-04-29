#include "DX12Game/RootSignatureManager.h"

using namespace Microsoft::WRL;

namespace {
	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 8> GetStaticSamplers8() {
		// Applications usually only need a handful of samplers.  So just define them all up front
		// and keep them available as part of the root signature.  

		const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
			0, // shaderRegister
			D3D12_FILTER_MIN_MAG_MIP_POINT,		// filter
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,	// addressU
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,	// addressV
			D3D12_TEXTURE_ADDRESS_MODE_WRAP		// addressW
		);

		const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
			1, // shaderRegister
			D3D12_FILTER_MIN_MAG_MIP_POINT,		// filter
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,	// addressU
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,	// addressV
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP	// addressW
		);

		const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
			2, // shaderRegister
			D3D12_FILTER_MIN_MAG_MIP_LINEAR,	// filter
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,	// addressU
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,	// addressV
			D3D12_TEXTURE_ADDRESS_MODE_WRAP		// addressW
		);

		const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
			3, // shaderRegister
			D3D12_FILTER_MIN_MAG_MIP_LINEAR,	// filter
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,	// addressU
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,	// addressV
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP	// addressW
		);

		const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
			4, // shaderRegister
			D3D12_FILTER_ANISOTROPIC,			// filter
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,	// addressU
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,	// addressV
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,	// addressW
			0.0f,								// mipLODBias
			8									// maxAnisotropy
		);

		const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
			5, // shaderRegister
			D3D12_FILTER_ANISOTROPIC,			// filter
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,	// addressU
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,	// addressV
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,	// addressW
			0.0f,								// mipLODBias
			8									// maxAnisotropy
		);

		const CD3DX12_STATIC_SAMPLER_DESC shadow(
			6,													// shaderRegister
			D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,	// filter
			D3D12_TEXTURE_ADDRESS_MODE_BORDER,					// addressU
			D3D12_TEXTURE_ADDRESS_MODE_BORDER,					// addressV
			D3D12_TEXTURE_ADDRESS_MODE_BORDER,					// addressW
			0.0f,												// mipLODBias
			16,													// maxAnisotropy
			D3D12_COMPARISON_FUNC_LESS_EQUAL,
			D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK
		);

		const CD3DX12_STATIC_SAMPLER_DESC depthMapSam(
			7,									// shaderRegister
			D3D12_FILTER_MIN_MAG_MIP_LINEAR,	// filter
			D3D12_TEXTURE_ADDRESS_MODE_BORDER,	// addressU
			D3D12_TEXTURE_ADDRESS_MODE_BORDER,	// addressV
			D3D12_TEXTURE_ADDRESS_MODE_BORDER,	// addressW
			0.0f,
			0,
			D3D12_COMPARISON_FUNC_LESS_EQUAL,
			D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE
		);

		return { pointWrap, pointClamp, linearWrap, linearClamp, anisotropicWrap, anisotropicClamp, shadow, depthMapSam };
	}

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 4> GetStaticSamplers4() {
		const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
			0,									// shaderRegister
			D3D12_FILTER_MIN_MAG_MIP_POINT,		// filter
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,	// addressU
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,	// addressV
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP	// addressW
		);

		const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
			1,									// shaderRegister
			D3D12_FILTER_MIN_MAG_MIP_LINEAR,	// filter
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,	// addressU
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,	// addressV
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP);	// addressW

		const CD3DX12_STATIC_SAMPLER_DESC depthMapSam(
			2,									// shaderRegister
			D3D12_FILTER_MIN_MAG_MIP_LINEAR,	// filter
			D3D12_TEXTURE_ADDRESS_MODE_BORDER,	// addressU
			D3D12_TEXTURE_ADDRESS_MODE_BORDER,	// addressV
			D3D12_TEXTURE_ADDRESS_MODE_BORDER,	// addressW
			0.0f,
			0,
			D3D12_COMPARISON_FUNC_LESS_EQUAL,
			D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE
		);

		const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
			3,									// shaderRegister
			D3D12_FILTER_MIN_MAG_MIP_LINEAR,	// filter
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,	// addressU
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,	// addressV
			D3D12_TEXTURE_ADDRESS_MODE_WRAP		// addressW
		);

		return { pointClamp, linearWrap, linearClamp, depthMapSam };
	}
}

GameResult RootSignatureManager::Initialize(ID3D12Device* inDevice) {
	md3dDevice = inDevice;

	return GameResultOk;
}

GameResult RootSignatureManager::BuildRootSignatures() {
	CheckGameResult(BuildBasicRootSignature());
	CheckGameResult(BuildSsaoRootSignature());
	CheckGameResult(BuildPostPassRootSignature());
	CheckGameResult(BuildSsrRootSignature());
	CheckGameResult(BuildBloomRootSignature());

	return GameResultOk;
}

ID3D12RootSignature* RootSignatureManager::GetBasicRootSignature() {
	return mBasicRootSignature.Get();
}

ID3D12RootSignature* RootSignatureManager::GetSsaoRootSignature() {
	return mSsaoRootSignature.Get();
}

ID3D12RootSignature* RootSignatureManager::GetPostPassRootSignature() {
	return mPostPassRootSignature.Get();
}

ID3D12RootSignature* RootSignatureManager::GetSsrRootSignature() {
	return mSsrRootSignature.Get();
}

ID3D12RootSignature* RootSignatureManager::GetBloomRootSignature() {
	return mBloomRootSignature.Get();
}

UINT RootSignatureManager::GetObjectCBIndex() {
	return mObjectCBIndex;
}

UINT RootSignatureManager::GetPassCBIndex() {
	return mPassCBIndex;
}

UINT RootSignatureManager::GetInstIdxBufferIndex() {
	return mInstIdxBufferIndex;
}

UINT RootSignatureManager::GetInstBufferIndex() {
	return mInstBufferIndex;
}

UINT RootSignatureManager::GetMatBufferIndex() {
	return mMatBufferIndex;
}

UINT RootSignatureManager::GetMiscTextureMapIndex() {
	return mMiscTextureMapIndex;
}

UINT RootSignatureManager::GetTextureMapIndex() {
	return mTextureMapIndex;
}

UINT RootSignatureManager::GetConstSettingsIndex() {
	return mConstSettingsIndex;
}

UINT RootSignatureManager::GetAnimationsMapIndex() {
	return mAnimationsMapIndex;
}

GameResult RootSignatureManager::BuildBasicRootSignature() {
	CD3DX12_DESCRIPTOR_RANGE texTable0;
	texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 13, 0, 0);

	CD3DX12_DESCRIPTOR_RANGE texTable1;
	texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 64, 13, 0);

	CD3DX12_DESCRIPTOR_RANGE texTable2;
	texTable2.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 77, 0);

	const UINT numParameters = 9;

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[numParameters];

	UINT cnt = 0;
	mObjectCBIndex = cnt++;
	mPassCBIndex = cnt++;
	mInstIdxBufferIndex = cnt++;
	mInstBufferIndex = cnt++;
	mMatBufferIndex = cnt++;
	mMiscTextureMapIndex = cnt++;
	mConstSettingsIndex = cnt++;
	mTextureMapIndex = cnt++;
	mAnimationsMapIndex = cnt++;

	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[mObjectCBIndex].InitAsConstantBufferView(
		0, 0, D3D12_SHADER_VISIBILITY_VERTEX
	);
	slotRootParameter[mPassCBIndex].InitAsConstantBufferView(
		1, 0, D3D12_SHADER_VISIBILITY_ALL
	);
	slotRootParameter[mInstIdxBufferIndex].InitAsShaderResourceView(
		0, 1, D3D12_SHADER_VISIBILITY_VERTEX
	);
	slotRootParameter[mInstBufferIndex].InitAsShaderResourceView(
		1, 1, D3D12_SHADER_VISIBILITY_VERTEX
	);
	slotRootParameter[mMatBufferIndex].InitAsShaderResourceView(
		2, 1, D3D12_SHADER_VISIBILITY_ALL
	);
	slotRootParameter[mMiscTextureMapIndex].InitAsDescriptorTable(
		1, &texTable0, D3D12_SHADER_VISIBILITY_PIXEL
	);
	slotRootParameter[mTextureMapIndex].InitAsDescriptorTable(
		1, &texTable1, D3D12_SHADER_VISIBILITY_PIXEL
	);
	slotRootParameter[mConstSettingsIndex].InitAsConstants(
		2, 2, 0, D3D12_SHADER_VISIBILITY_ALL
	);
	slotRootParameter[mAnimationsMapIndex].InitAsDescriptorTable(
		1, &texTable2, D3D12_SHADER_VISIBILITY_ALL
	);

	auto staticSamplers = GetStaticSamplers8();

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		numParameters,
		slotRootParameter,
		static_cast<UINT>(staticSamplers.size()),
		staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	{
		std::wstringstream wsstream;
		if (errorBlob != nullptr)
			wsstream << reinterpret_cast<char*>(errorBlob->GetBufferPointer());

		if (FAILED(hr))
			ReturnGameResult(S_FALSE, wsstream.str());
	}

	ReturnIfFailed(
		md3dDevice->CreateRootSignature(
			0,
			serializedRootSig->GetBufferPointer(),
			serializedRootSig->GetBufferSize(),
			IID_PPV_ARGS(mBasicRootSignature.GetAddressOf())
		)
	);

	return GameResultOk;
}

GameResult RootSignatureManager::BuildSsaoRootSignature() {
	CD3DX12_DESCRIPTOR_RANGE texTable0;
	texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0);

	CD3DX12_DESCRIPTOR_RANGE texTable1;
	texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0);

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[4];

	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[0].InitAsConstantBufferView(0);
	slotRootParameter[1].InitAsConstants(1, 1);
	slotRootParameter[2].InitAsDescriptorTable(1, &texTable0, D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[3].InitAsDescriptorTable(1, &texTable1, D3D12_SHADER_VISIBILITY_PIXEL);

	auto staticSamplers = GetStaticSamplers4();

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		4, slotRootParameter,
		static_cast<UINT>(staticSamplers.size()),
		staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;

	HRESULT hr = D3D12SerializeRootSignature(
		&rootSigDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(),
		errorBlob.GetAddressOf()
	);

	if (errorBlob != nullptr)
		::OutputDebugStringA(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
	ReturnIfFailed(hr);

	ReturnIfFailed(
		md3dDevice->CreateRootSignature(
			0,
			serializedRootSig->GetBufferPointer(),
			serializedRootSig->GetBufferSize(),
			IID_PPV_ARGS(mSsaoRootSignature.GetAddressOf())
		)
	);

	return GameResultOk;
}

GameResult RootSignatureManager::BuildPostPassRootSignature() {
	CD3DX12_DESCRIPTOR_RANGE texTable0;
	texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 13, 0, 0);

	const UINT numParameters = 3;

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[numParameters];

	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[0].InitAsConstantBufferView(0);
	slotRootParameter[1].InitAsConstants(1, 1, 0, D3D12_SHADER_VISIBILITY_ALL);
	slotRootParameter[2].InitAsDescriptorTable(1, &texTable0, D3D12_SHADER_VISIBILITY_PIXEL);

	auto staticSamplers = GetStaticSamplers8();

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		numParameters,
		slotRootParameter,
		static_cast<UINT>(staticSamplers.size()),
		staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;

	HRESULT hr = D3D12SerializeRootSignature(
		&rootSigDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(),
		errorBlob.GetAddressOf()
	);

	if (errorBlob != nullptr)
		::OutputDebugStringA(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
	ReturnIfFailed(hr);

	ReturnIfFailed(
		md3dDevice->CreateRootSignature(
			0,
			serializedRootSig->GetBufferPointer(),
			serializedRootSig->GetBufferSize(),
			IID_PPV_ARGS(mPostPassRootSignature.GetAddressOf())
		)
	);

	return GameResultOk;
}

GameResult RootSignatureManager::BuildSsrRootSignature() {
	// For main pass maps.
	CD3DX12_DESCRIPTOR_RANGE texTable0;
	texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0);

	// For normal and depth map.
	CD3DX12_DESCRIPTOR_RANGE texTable1;
	texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 2, 0);

	// For input map.
	CD3DX12_DESCRIPTOR_RANGE texTable2;
	texTable2.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4, 0);

	const int numRootParameters = 5;

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[numRootParameters];

	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[0].InitAsConstantBufferView(0);
	slotRootParameter[1].InitAsConstants(1, 1);
	slotRootParameter[2].InitAsDescriptorTable(1, &texTable0, D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[3].InitAsDescriptorTable(1, &texTable1, D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[4].InitAsDescriptorTable(1, &texTable2, D3D12_SHADER_VISIBILITY_PIXEL);

	auto staticSamplers = GetStaticSamplers4();

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		numRootParameters,
		slotRootParameter,
		static_cast<UINT>(staticSamplers.size()),
		staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;

	HRESULT hr = D3D12SerializeRootSignature(
		&rootSigDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(),
		errorBlob.GetAddressOf()
	);

	if (errorBlob != nullptr)
		::OutputDebugStringA(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
	ReturnIfFailed(hr);

	ReturnIfFailed(
		md3dDevice->CreateRootSignature(
			0,
			serializedRootSig->GetBufferPointer(),
			serializedRootSig->GetBufferSize(),
			IID_PPV_ARGS(mSsrRootSignature.GetAddressOf())
		)
	);

	return GameResultOk;
}

GameResult RootSignatureManager::BuildBloomRootSignature() {
	// For source map(to extract highlight).
	CD3DX12_DESCRIPTOR_RANGE texTable0;
	texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0);

	// For affecting blur.
	CD3DX12_DESCRIPTOR_RANGE texTable1;
	texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0);

	const int numRootParameters = 4;

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[numRootParameters];

	slotRootParameter[0].InitAsDescriptorTable(1, &texTable0, D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[1].InitAsDescriptorTable(1, &texTable1, D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[2].InitAsConstantBufferView(0);
	slotRootParameter[3].InitAsConstants(1, 1);

	auto staticSamplers = GetStaticSamplers4();

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		numRootParameters,
		slotRootParameter,
		static_cast<UINT>(staticSamplers.size()),
		staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;

	HRESULT hr = D3D12SerializeRootSignature(
		&rootSigDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(),
		errorBlob.GetAddressOf()
	);

	if (errorBlob != nullptr)
		::OutputDebugStringA(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
	ReturnIfFailed(hr);

	ReturnIfFailed(
		md3dDevice->CreateRootSignature(
			0,
			serializedRootSig->GetBufferPointer(),
			serializedRootSig->GetBufferSize(),
			IID_PPV_ARGS(mBloomRootSignature.GetAddressOf())
		)
	);

	return GameResultOk;
}