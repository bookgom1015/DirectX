#include "DX12Game/GameCore.h"

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
	UINT compileFlags = 0; = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
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

	if (FAILED(hr))
		return GameResult(hr, wsstream.str());

	return GameResult(S_OK);
}