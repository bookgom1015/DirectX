#include "DX12Game/D3D12Util.h"

#include <fstream>
#include <atlcomcli.h>

using namespace Microsoft::WRL;

GameResult D3D12Util::LoadBinary(const std::wstring& inFilename, ComPtr<ID3DBlob>& outBlob) {
	std::ifstream fin(inFilename, std::ios::binary);

	fin.seekg(0, std::ios_base::end);
	std::ifstream::pos_type size = (int)fin.tellg();
	fin.seekg(0, std::ios_base::beg);

	ReturnIfFailed(D3DCreateBlob(size, outBlob.GetAddressOf()));

	fin.read(reinterpret_cast<char*>(outBlob->GetBufferPointer()), size);
	fin.close();

	return GameResult(S_OK);
}

GameResult D3D12Util::CreateDefaultBuffer(
		ID3D12Device* inDevice,
		ID3D12GraphicsCommandList* inCmdList,
		const void* inInitData,
		UINT64 inByteSize,
		ComPtr<ID3D12Resource>& outUploadBuffer,
		ComPtr<ID3D12Resource>& outDefaultBuffer) {
	// Create the actual default buffer resource.
	ReturnIfFailed(inDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(inByteSize),
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(outDefaultBuffer.GetAddressOf()))
	);
	
	// In order to copy CPU memory data into our default buffer, we need to create
	// an intermediate upload heap. 
	ReturnIfFailed(inDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(inByteSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(outUploadBuffer.GetAddressOf()))
	);
	
	// Describe the data we want to copy into the default buffer.
	D3D12_SUBRESOURCE_DATA subResourceData = {};
	subResourceData.pData = inInitData;
	subResourceData.RowPitch = inByteSize;
	subResourceData.SlicePitch = subResourceData.RowPitch;

	// Schedule to copy the data to the default buffer resource.  At a high level, the helper function UpdateSubresources
	// will copy the CPU memory into the intermediate upload heap.  Then, using ID3D12CommandList::CopySubresourceRegion,
	// the intermediate upload heap data will be copied to mBuffer.
	inCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(outDefaultBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
	UpdateSubresources<1>(inCmdList, outDefaultBuffer.Get(), outUploadBuffer.Get(), 0, 0, 1, &subResourceData);
	inCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(outDefaultBuffer.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));

	// Note: uploadBuffer has to be kept alive after the above function calls because
	// the command list has not been executed yet that performs the actual copy.
	// The caller can Release the uploadBuffer after it knows the copy has been executed.
	
	return GameResult(S_OK);
}

GameResult D3D12Util::CompileShader(
		const std::wstring& inFilename,
		const D3D_SHADER_MACRO* inDefines,
		const std::string& inEntrypoint,
		const std::string& inTarget,
		ComPtr<ID3DBlob>& outByteCode) {
#if defined(_DEBUG)  
	UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT compileFlags = 0;
#endif

	HRESULT hr = S_OK;

	ComPtr<ID3DBlob> byteCode = nullptr;
	ComPtr<ID3DBlob> errors;
	hr = D3DCompileFromFile(inFilename.c_str(), inDefines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		inEntrypoint.c_str(), inTarget.c_str(), compileFlags, 0, &outByteCode, &errors);

	std::wstringstream wsstream;
	if (errors != nullptr)
		wsstream << reinterpret_cast<char*>(errors->GetBufferPointer());

	if (FAILED(hr)) {
		ReturnGameResult(hr, wsstream.str());
	}

	return GameResult(S_OK);
}

GameResult D3D12Util::CompileShader(
		IDxcCompiler* pCompiler,
		IDxcLibrary* pLibrary,
		const D3D12ShaderInfo& inInfo,
		IDxcBlob** outByteCode) {
	UINT32 code = 0;
	IDxcBlobEncoding* shaderText = nullptr;
	ReturnIfFailed(pLibrary->CreateBlobFromFile(inInfo.FileName, &code, &shaderText));

	CComPtr<IDxcIncludeHandler> includeHandler;
	ReturnIfFailed(pLibrary->CreateIncludeHandler(&includeHandler));

	IDxcOperationResult* result;
	ReturnIfFailed(pCompiler->Compile(
		shaderText,
		inInfo.FileName,
		inInfo.EntryPoint,
		inInfo.TargetProfile,
		inInfo.Arguments,
		inInfo.ArgCount,
		inInfo.Defines,
		inInfo.DefineCount,
		includeHandler,
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

	ReturnIfFailed(result->GetResult(outByteCode));

	return GameResultOk;
}

GameResult D3D12Util::AllocateUavBuffer(
		ID3D12Device* pDevice,
		UINT64 inBufferSize,
		ID3D12Resource** ppResource,
		D3D12_RESOURCE_STATES inInitState /* = D3D12_RESOURCE_STATE_COMMON */,
		const D3D12_CLEAR_VALUE* pOptimizedClearValue /* = nullptr */,
		const wchar_t* pResourceName /* = nullptr */) {
	ReturnIfFailed(pDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(inBufferSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
		inInitState,
		pOptimizedClearValue,
		IID_PPV_ARGS(ppResource)
	));
	if (pResourceName)
		(*ppResource)->SetName(pResourceName);
	return GameResultOk;
}

GameResult D3D12Util::AllocateUploadBuffer(
		ID3D12Device* pDevice,
		void* pData,
		UINT64 inDataSize,
		ID3D12Resource** ppResource,
		const D3D12_CLEAR_VALUE* pOptimizedClearValue /* = nullptr */,
		const wchar_t* pResourceName /* = nullptr */) {
	ReturnIfFailed(pDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(inDataSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		pOptimizedClearValue,
		IID_PPV_ARGS(ppResource)
	));
	if (pResourceName)
		(*ppResource)->SetName(pResourceName);
	void* mappedDdata;
	(*ppResource)->Map(0, nullptr, &mappedDdata);
	std::memcpy(mappedDdata, pData, inDataSize);
	(*ppResource)->Unmap(0, nullptr);
	return GameResultOk;
}

GameResult D3D12Util::CreateBuffer(ID3D12Device* pDevice, D3D12BufferCreateInfo& inInfo, ID3D12Resource** ppResource) {
	D3D12_HEAP_PROPERTIES heapDesc = {};
	heapDesc.Type = inInfo.HeapType;
	heapDesc.CreationNodeMask = 1;
	heapDesc.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Alignment = inInfo.Alignment;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Width = inInfo.Size;
	resourceDesc.Flags = inInfo.Flags;

	ReturnIfFailed(pDevice->CreateCommittedResource(&heapDesc, D3D12_HEAP_FLAG_NONE, &resourceDesc, inInfo.State, nullptr, IID_PPV_ARGS(ppResource)));
	return GameResultOk;
}

GameResult D3D12Util::CreateRootSignature(ID3D12Device* pDevice, const D3D12_ROOT_SIGNATURE_DESC& inRootSignatureDesc, ID3D12RootSignature** ppRootSignature) {
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(
		&inRootSignatureDesc, 
		D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), 
		errorBlob.GetAddressOf()
	);
	
	std::wstringstream wsstream;
	if (errorBlob != nullptr)
		wsstream << reinterpret_cast<char*>(errorBlob->GetBufferPointer());

	if (FAILED(hr))
		ReturnGameResult(S_FALSE, wsstream.str());
	
	ReturnIfFailed(pDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(ppRootSignature)
	));

	return GameResultOk;
}