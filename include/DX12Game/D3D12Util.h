#pragma once

#include <DirectXCollision.h>
#include <DirectXColors.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <dxgi1_4.h>
#include <D3Dcompiler.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxc/dxcapi.h>
#include <dxc/Support/dxcapi.use.h>
#include <SimpleMath.h>
#include <wrl.h>
#include <unordered_map>

#include "common/d3dx12.h"
#include "common/MathHelper.h"
#include "common/DDSTextureLoader.h"
#include "DX12Game/GameResult.h"

extern const int gNumFrameResources;

// Defines a subrange of geometry in a MeshGeometry.  This is for when multiple
// geometries are stored in one vertex and index buffer.  It provides the offsets
// and data needed to draw a subset of geometry stores in the vertex and index 
// buffers so that we can implement the technique described by Figure 6.3.
struct SubmeshGeometry {
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	INT BaseVertexLocation = 0;

	// Bounding box of the geometry defined by this submesh. 
	// This is used in later chapters of the book.
	DirectX::BoundingBox AABB;
	DirectX::BoundingOrientedBox OBB;
	DirectX::BoundingSphere Sphere;
};

struct MeshGeometry {
	// Give it a name so we can look it up by name.
	std::string Name;

	// System memory copies.  Use Blobs because the vertex/index format can be generic.
	// It is up to the client to cast appropriately.  
	Microsoft::WRL::ComPtr<ID3DBlob> VertexBufferCPU = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> IndexBufferCPU = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferUploader = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;

	// Data about the buffers.
	UINT VertexByteStride = 0;
	UINT VertexBufferByteSize = 0;
	DXGI_FORMAT IndexFormat = DXGI_FORMAT_R16_UINT;
	UINT IndexBufferByteSize = 0;

	// A MeshGeometry may store multiple geometries in one vertex/index buffer.
	// Use this container to define the Submesh geometries so we can draw
	// the Submeshes individually.
	std::unordered_map<std::string, SubmeshGeometry> DrawArgs;

	D3D12_VERTEX_BUFFER_VIEW VertexBufferView() const {
		D3D12_VERTEX_BUFFER_VIEW vbv;
		vbv.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
		vbv.StrideInBytes = VertexByteStride;
		vbv.SizeInBytes = VertexBufferByteSize;

		return vbv;
	}

	D3D12_INDEX_BUFFER_VIEW IndexBufferView() const {
		D3D12_INDEX_BUFFER_VIEW ibv;
		ibv.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
		ibv.Format = IndexFormat;
		ibv.SizeInBytes = IndexBufferByteSize;

		return ibv;
	}

	// We can free this memory after we finish upload to the GPU.
	void DisposeUploaders() {
		VertexBufferUploader = nullptr;
		IndexBufferUploader = nullptr;
	}
};

struct Light {
	DirectX::XMFLOAT3 Strength = { 0.5f, 0.5f, 0.5f };
	float FalloffStart = 1.0f;                          // point/spot light only
	DirectX::XMFLOAT3 Direction = { 0.0f, -1.0f, 0.0f };// directional/spot light only
	float FalloffEnd = 10.0f;                           // point/spot light only
	DirectX::XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };  // point/spot light only
	float SpotPower = 64.0f;                            // spot light only
};

#define MaxLights 16

struct MaterialConstants {
	DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
	float Roughness = 0.25f;

	// Used in texture mapping.
	DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();
};

// Simple struct to represent a material for our demos.  A production 3D engine
// would likely create a class hierarchy of Materials.
struct Material {
	// Unique material name for lookup.
	std::string Name;

	// Index into constant buffer corresponding to this material.
	int MatCBIndex = -1;

	// Index into SRV heap for diffuse texture.
	int DiffuseSrvHeapIndex = -1;

	// Index into SRV heap for normal texture.
	int NormalSrvHeapIndex = -1;

	// Index into SRV heap for specular texture.
	int SpecularSrvHeapIndex = -1;

	// Index into SRV heap for subsurface sacttering texture.
	int SSSSrvHeapIndex = -1;

	// Index into SRV heap for displacment texture.
	int DispSrvHeapIndex = -1;

	// Index into SRV heap for alpha texture.
	int AlphaSrvHeapIndex = -1;

	// Dirty flag indicating the material has changed and we need to update the constant buffer.
	// Because we have a material constant buffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify a material we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int NumFramesDirty = gNumFrameResources;

	// Material constant buffer data used for shading.
	DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
	float Roughness = .25f;
	DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();
};

struct MaterialIn {
	std::string MaterialName;

	std::string DiffuseMapFileName;
	std::string NormalMapFileName;
	std::string SpecularMapFileName;
	std::string AlphaMapFileName;

	DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();
	DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT3 FresnelR0 = { 0.5f, 0.5f, 0.5f };
	float Roughness = 0.5f;
};

struct Texture {
	// Unique material name for lookup.
	std::string Name;

	std::wstring Filename;

	Microsoft::WRL::ComPtr<ID3D12Resource> Resource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> UploadHeap = nullptr;
};

struct D3D12BufferCreateInfo {
	UINT64					Size		= 0;
	UINT64					Alignment	= 0;
	D3D12_HEAP_TYPE			HeapType	= D3D12_HEAP_TYPE_DEFAULT;
	D3D12_RESOURCE_FLAGS	Flags		= D3D12_RESOURCE_FLAG_NONE;
	D3D12_RESOURCE_STATES	State		= D3D12_RESOURCE_STATE_COMMON;

	D3D12BufferCreateInfo() {}
	D3D12BufferCreateInfo(UINT64 InSize, D3D12_RESOURCE_FLAGS InFlags) : 
		Size(InSize), 
		Flags(InFlags) {}
	D3D12BufferCreateInfo(UINT64 InSize, D3D12_HEAP_TYPE InHeapType, D3D12_RESOURCE_STATES InState) :
		Size(InSize),
		HeapType(InHeapType),
		State(InState) {}
	D3D12BufferCreateInfo(UINT64 InSize, D3D12_RESOURCE_FLAGS InFlags, D3D12_RESOURCE_STATES InState) :
		Size(InSize),
		Flags(InFlags),
		State(InState) {}
	D3D12BufferCreateInfo(UINT64 InSize, UINT64 InAlignment, D3D12_HEAP_TYPE InHeapType, D3D12_RESOURCE_FLAGS InFlags, D3D12_RESOURCE_STATES InState) :
		Size(InSize),
		Alignment(InAlignment),
		HeapType(InHeapType),
		Flags(InFlags),
		State(InState) {}
};

struct D3D12ShaderCompilerInfo {
	dxc::DxcDllSupport	DxcDllHelper;
	IDxcCompiler*		Compiler	= nullptr;
	IDxcLibrary*		Library		= nullptr;
};

struct D3D12ShaderInfo {
	LPCWSTR		FileName		= nullptr;
	LPCWSTR		EntryPoint		= nullptr;
	LPCWSTR		TargetProfile	= nullptr;
	LPCWSTR*	Arguments		= nullptr;
	DxcDefine*	Defines			= nullptr;
	UINT32		ArgCount		= 0;
	UINT32		DefineCount		= 0;

	D3D12ShaderInfo() {}
	D3D12ShaderInfo(LPCWSTR inFileName, LPCWSTR inEntryPoint, LPCWSTR inProfile) {
		FileName = inFileName;
		EntryPoint = inEntryPoint;
		TargetProfile = inProfile;
	}
};

struct RaytracingProgram {
	D3D12ShaderInfo								Info			= {};
	Microsoft::WRL::ComPtr<IDxcBlob>			Blob			= nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature>	RootSignature	= nullptr;

	D3D12_DXIL_LIBRARY_DESC DxilLibDesc;
	D3D12_EXPORT_DESC		ExportDesc;
	D3D12_STATE_SUBOBJECT	Subobject;
	std::wstring			ExportName;

	RaytracingProgram() {
		ExportDesc.ExportToRename = nullptr;
	}

	RaytracingProgram(D3D12ShaderInfo inInfo) {
		Info = inInfo;
		Subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
		ExportName = inInfo.EntryPoint;
		ExportDesc.ExportToRename = nullptr;
		ExportDesc.Flags = D3D12_EXPORT_FLAG_NONE;
	}

	void SetBytecode() {
		ExportDesc.Name = ExportName.c_str();

		DxilLibDesc.NumExports = 1;
		DxilLibDesc.pExports = &ExportDesc;
		DxilLibDesc.DXILLibrary.BytecodeLength = Blob->GetBufferSize();
		DxilLibDesc.DXILLibrary.pShaderBytecode = Blob->GetBufferPointer();

		Subobject.pDesc = &DxilLibDesc;
	}
};

class D3D12Util {
public:
	static UINT CalcConstantBufferByteSize(UINT byteSize) {
		// Constant buffers must be a multiple of the minimum hardware
		// allocation size (usually 256 bytes).  So round up to nearest
		// multiple of 256.  We do this by adding 255 and then masking off
		// the lower 2 bytes which store all bits < 256.
		// Example: Suppose byteSize = 300.
		// (300 + 255) & ~255
		// 555 & ~255
		// 0x022B & ~0x00ff
		// 0x022B & 0xff00
		// 0x0200
		// 512
		return (byteSize + 255) & ~255;
	}

	static GameResult LoadBinary(const std::wstring& inFilename, Microsoft::WRL::ComPtr<ID3DBlob>& outBlob);

	static GameResult CreateDefaultBuffer(
		ID3D12Device* inDevice,
		ID3D12GraphicsCommandList* inCmdList,
		const void* inInitData,
		UINT64 inByteSize,
		Microsoft::WRL::ComPtr<ID3D12Resource>& outUploadBuffer,
		Microsoft::WRL::ComPtr<ID3D12Resource>& outDefaultBuffer);

	static GameResult CompileShader(
		const std::wstring& inFilename,
		const D3D_SHADER_MACRO* inDefines,
		const std::string& inEntrypoint,
		const std::string& inTarget,
		Microsoft::WRL::ComPtr<ID3DBlob>& outByteCode);

	static GameResult CompileShader(
		D3D12ShaderCompilerInfo& inCompilerInfo, 
		D3D12ShaderInfo& inInfo, 
		IDxcBlob** ppBlob);

	static GameResult AllocateUavBuffer(
		ID3D12Device* pDevice,
		UINT64 inBufferSize,
		ID3D12Resource** ppResource,
		D3D12_RESOURCE_STATES inInitState = D3D12_RESOURCE_STATE_COMMON,
		const D3D12_CLEAR_VALUE* pOptimizedClearValue = nullptr,
		const wchar_t* pResourceName = nullptr);

	static GameResult AllocateUploadBuffer(
		ID3D12Device* pDevice,
		void* pData,
		UINT64 inDataSize,
		ID3D12Resource** ppResource,
		const D3D12_CLEAR_VALUE* pOptimizedClearValue = nullptr,
		const wchar_t* pResourceName = nullptr);

	static GameResult CreateBuffer(ID3D12Device* pDevice, D3D12BufferCreateInfo& inInfo, ID3D12Resource** ppResource);

	static GameResult CreateRootSignature(ID3D12Device* pDevice, const D3D12_ROOT_SIGNATURE_DESC& inRootSignatureDesc, ID3D12RootSignature** ppRootSignature);
};

#ifndef ReleaseCom
	#define ReleaseCom(x) { if (x){ x->Release(); x = NULL; } }
#endif

#ifndef Align
	#define Align(alignment, val) (((val + alignment - 1) / alignment) * alignment)
#endif