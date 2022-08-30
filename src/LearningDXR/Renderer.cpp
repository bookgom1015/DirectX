#include "LearningDXR/Renderer.h"
#include "LearningDXR/D3D12Util.h"
#include "LearningDXR/Macros.h"
#include "common/GeometryGenerator.h"

#include <array>
#include <d3dcompiler.h>

using namespace DirectX;
using namespace DirectX::PackedVector;
using namespace Microsoft::WRL;

namespace {
	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers() {
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

		return { pointWrap, pointClamp, linearWrap, linearClamp, anisotropicWrap, anisotropicClamp };
	}
}

Renderer::Renderer() {}

Renderer::~Renderer() {
	if (!bIsCleanedUp)
		CleanUp();
}

GameResult Renderer::Initialize(HWND hMainWnd, UINT inClientWidth /* = 800 */, UINT inClientHeight /* = 600 */) {
	CheckGameResult(LowRenderer::Initialize(hMainWnd, inClientWidth, inClientHeight));

	mMainRenderTargets.resize(gNumFrameResources);

	XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 0.1f, 1000.0f);
	XMStoreFloat4x4(&mProj, P);

	CheckGameResult(mShaderManager.Initialize());

	ReturnIfFailed(mDirectCmdListAlloc->Reset());
	ReturnIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	CheckGameResult(BuildGeometries());

	// Raterization
	CheckGameResult(CompileShaders());
	CheckGameResult(BuildRootSignatures());
	CheckGameResult(BuildResources());
	CheckGameResult(BuildDescriptorHeaps());
	CheckGameResult(BuildDescriptors());
	CheckGameResult(BuildPSOs());
	CheckGameResult(BuildMaterials());
	CheckGameResult(BuildFrameResources());
	CheckGameResult(BuildRenderItems());

	// Ray-tracing
	CheckGameResult(BuildDXRShaders());
	CheckGameResult(BuildGlobalRootSignatures());
	CheckGameResult(BuildLocalRootSignatures());
	CheckGameResult(BuildDXRResources());
	CheckGameResult(BuildDXRDescriptorHeaps());
	CheckGameResult(BuildDXRDescriptors());
	CheckGameResult(BuildBLASs());
	CheckGameResult(BuildTLASs());
	CheckGameResult(BuildDXRPSOs());
	//CheckGameResult(BuildShaderTables());

	ReturnIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };	
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	CheckGameResult(FlushCommandQueue());

	return GameResultOk;
}
 
void Renderer::CleanUp() {
	LowRenderer::CleanUp();

	mShaderManager.CleanUp();

	bIsCleanedUp = true;
}

GameResult Renderer::Update(const GameTimer& gt) {
	CheckGameResult(UpdateCamera(gt));

	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	// Has the GPU finished processing the commands of the current frame resource?
	// If not, wait until the GPU has completed commands up to this fence point.
	if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence) {
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		ReturnIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	CheckGameResult(UpdateObjectCB(gt));
	CheckGameResult(UpdatePassCB(gt));
	CheckGameResult(UpdateMaterialCB(gt));

	return GameResultOk;
}

GameResult Renderer::Draw() {
	ReturnIfFailed(mCurrFrameResource->CmdListAlloc->Reset());
	
	if (bRaytracing) {
		CheckGameResult(Raytrace());
	}
	else {
		CheckGameResult(Rasterize());
	}

	ReturnIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	ReturnIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	mCurrFrameResource->Fence = static_cast<UINT>(++mCurrentFence);
	mCommandQueue->Signal(mFence.Get(), mCurrentFence);

	return GameResultOk;
}

GameResult Renderer::OnResize(UINT inClientWidth, UINT inClientHeight) {
	CheckGameResult(LowRenderer::OnResize(inClientWidth, inClientHeight));

	XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 0.1f, 1000.0f);
	XMStoreFloat4x4(&mProj, P);

	CheckGameResult(BuildResources());
	CheckGameResult(BuildDescriptors());

	CheckGameResult(BuildDXRResources());
	CheckGameResult(BuildDXRDescriptors());

	return GameResultOk;
}

bool Renderer::GetRenderType() const {
	return bRaytracing;
}

void Renderer::SetRenderType(bool inValue) {
	bRaytracing = inValue;
}

GameResult Renderer::CreateRtvAndDsvDescriptorHeaps() {
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = SwapChainBufferCount + gNumFrameResources /* for UAVs */;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	ReturnIfFailed(md3dDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(mRtvHeap.GetAddressOf())));

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	ReturnIfFailed(md3dDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(mDsvHeap.GetAddressOf())));

	return GameResultOk;
}

GameResult Renderer::BuildGeometries() {
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData sphere = geoGen.CreateSphere(1.0f, 32, 32);

	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.IndexCount = static_cast<UINT>(sphere.Indices32.size());
	sphereSubmesh.BaseVertexLocation = 0;
	sphereSubmesh.StartIndexLocation = 0;

	std::vector<Vertex> vertices(sphere.Vertices.size());
	for (size_t i = 0, end = vertices.size(); i < end; ++i) {
		vertices[i].Pos = sphere.Vertices[i].Position;
		vertices[i].Normal = sphere.Vertices[i].Normal;
		vertices[i].TexC = sphere.Vertices[i].TexC;
		vertices[i].Tangent = sphere.Vertices[i].TangentU;
	}

	std::vector<std::uint32_t> indices;
	indices.insert(indices.end(), std::begin(sphere.Indices32), std::end(sphere.Indices32));

	const UINT vbByteSize = static_cast<UINT>(vertices.size() * sizeof(Vertex));
	const UINT ibByteSize = static_cast<UINT>(indices.size() * sizeof(std::uint32_t));

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "sphereGeo";

	ReturnIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);
	
	ReturnIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	CheckGameResult(D3D12Util::CreateDefaultBuffer(
		md3dDevice.Get(),
		mCommandList.Get(),
		vertices.data(),
		vbByteSize,
		geo->VertexBufferUploader,
		geo->VertexBufferGPU)
	);

	CheckGameResult(D3D12Util::CreateDefaultBuffer(
		md3dDevice.Get(),
		mCommandList.Get(),
		indices.data(),
		ibByteSize,
		geo->IndexBufferUploader,
		geo->IndexBufferGPU)
	);

	geo->VertexByteStride = static_cast<UINT>(sizeof(Vertex));
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R32_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	geo->DrawArgs["sphere"] = sphereSubmesh;

	mGeometries[geo->Name] = std::move(geo);

	return GameResultOk;
}

GameResult Renderer::CompileShaders() {
	{
		const std::wstring filePath = ShaderFilePathW + L"LearningDXR\\Default.hlsl";
		CheckGameResult(mShaderManager.CompileShader(filePath, nullptr, "VS", "vs_5_1", "defaultVS"));
		CheckGameResult(mShaderManager.CompileShader(filePath, nullptr, "PS", "ps_5_1", "defaultPS"));
	}
	{
		const std::wstring filePath = ShaderFilePathW + L"LearningDXR\\SampleToBackBuffer.hlsl";
		CheckGameResult(mShaderManager.CompileShader(filePath, nullptr, "VS", "vs_5_1", "backBufferVS"));
		CheckGameResult(mShaderManager.CompileShader(filePath, nullptr, "PS", "ps_5_1", "backBufferPS"));
	}

	return GameResultOk;
}

GameResult Renderer::BuildRootSignatures() {
	CD3DX12_ROOT_PARAMETER slotRootParameter[4];

	CD3DX12_DESCRIPTOR_RANGE uavTable;
	uavTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

	slotRootParameter[0].InitAsDescriptorTable(1, &uavTable);
	slotRootParameter[1].InitAsConstantBufferView(0);
	slotRootParameter[2].InitAsConstantBufferView(1);
	slotRootParameter[3].InitAsConstantBufferView(2);

	auto samplers = GetStaticSamplers();

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		_countof(slotRootParameter), slotRootParameter,
		static_cast<UINT>(samplers.size()), samplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	CheckGameResult(D3D12Util::CreateRootSignature(md3dDevice.Get(), rootSigDesc, mRootSignature.GetAddressOf()));

	return GameResultOk;
}

GameResult Renderer::BuildResources() {
	D3D12_RESOURCE_DESC rscDesc = {};
	ZeroMemory(&rscDesc, sizeof(D3D12_RESOURCE_DESC));
	rscDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	rscDesc.Alignment = 0;
	rscDesc.Width = mClientWidth;
	rscDesc.Height = mClientHeight;
	rscDesc.Format = mBackBufferFormat;
	rscDesc.DepthOrArraySize = 1;
	rscDesc.MipLevels = 1;
	rscDesc.SampleDesc.Count = 1;
	rscDesc.SampleDesc.Quality = 0;
	rscDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	rscDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	for (int i = 0; i < gNumFrameResources; ++i) {
		ReturnIfFailed(md3dDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&rscDesc,
			D3D12_RESOURCE_STATE_COPY_SOURCE,
			nullptr,
			IID_PPV_ARGS(mMainRenderTargets[i].GetAddressOf())
		));
	}

	return GameResultOk;
}

GameResult Renderer::BuildDescriptorHeaps() {
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = 32;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	md3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mDescriptorHeap));

	return GameResultOk;
}

GameResult Renderer::BuildDescriptors() {
	auto hRtvDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(mRtvHeap->GetCPUDescriptorHandleForHeapStart(), SwapChainBufferCount, mRtvDescriptorSize);
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Format = mBackBufferFormat;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;	

	auto hUavDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(mDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Format = mBackBufferFormat;
	uavDesc.Texture2D.MipSlice = 0;
	uavDesc.Texture2D.PlaneSlice = 0;

	for (int i = 0; i < gNumFrameResources; ++i) {
		md3dDevice->CreateRenderTargetView(mMainRenderTargets[i].Get(), &rtvDesc, hRtvDescriptor);
		hRtvDescriptor.Offset(1, mRtvDescriptorSize);

		md3dDevice->CreateUnorderedAccessView(mMainRenderTargets[i].Get(), nullptr, &uavDesc, hUavDescriptor);
		hUavDescriptor.Offset(1, mCbvSrvUavDescriptorSize);
	}

	return GameResultOk;
}

GameResult Renderer::BuildPSOs() {
	std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout = {
		{ "POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT,			0, 0,	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL",		0, DXGI_FORMAT_R32G32B32_FLOAT,			0, 12,	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD",	0, DXGI_FORMAT_R32G32_FLOAT,			0, 24,	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT",	0, DXGI_FORMAT_R32G32B32_FLOAT,			0, 32,	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC defaultPsoDesc;
	ZeroMemory(&defaultPsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	defaultPsoDesc.InputLayout = { inputLayout.data(), static_cast<UINT>(inputLayout.size()) };
	defaultPsoDesc.pRootSignature = mRootSignature.Get();
	{
		auto vs = mShaderManager.GetShader("defaultVS");
		auto ps = mShaderManager.GetShader("defaultPS");
		defaultPsoDesc.VS = {
			reinterpret_cast<BYTE*>(vs->GetBufferPointer()),
			vs->GetBufferSize()
		};
		defaultPsoDesc.PS = {
			reinterpret_cast<BYTE*>(ps->GetBufferPointer()),
			ps->GetBufferSize()
		};
	}
	defaultPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	defaultPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	defaultPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	defaultPsoDesc.SampleMask = UINT_MAX;
	defaultPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	defaultPsoDesc.NumRenderTargets = 1;
	defaultPsoDesc.RTVFormats[0] = mBackBufferFormat;
	defaultPsoDesc.SampleDesc.Count = 1;
	defaultPsoDesc.SampleDesc.Quality = 0;
	defaultPsoDesc.DSVFormat = mDepthStencilFormat;
	ReturnIfFailed(md3dDevice->CreateGraphicsPipelineState(&defaultPsoDesc, IID_PPV_ARGS(&mPSOs["Default"])));

	return GameResultOk;
}

GameResult Renderer::BuildMaterials() {
	auto defaultMat = std::make_unique<Material>();
	defaultMat->Name = "default";
	defaultMat->MatCBIndex = 0;
	defaultMat->DiffuseAlbedo = XMFLOAT4(0.556827f, 0.556827f, 0.556827f, 1.0f);
	defaultMat->FresnelR0 = XMFLOAT3(0.88725f, 0.88725f, 0.88725f);
	defaultMat->Roughness = 0.1f;

	mMaterials[defaultMat->Name] = std::move(defaultMat);

	return GameResultOk;
}

GameResult Renderer::BuildFrameResources() {
	for (int i = 0; i < gNumFrameResources; i++) {
		mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(), 1, 32, 32));
		CheckGameResult(mFrameResources.back()->Initialize());
	}

	return GameResultOk;
}

GameResult Renderer::BuildRenderItems() {
	auto sphereRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&sphereRitem->World, XMMatrixScaling(2.0f, 2.0f, 2.0f)*XMMatrixTranslation(0.0f, 0.5f, 0.0f));
	sphereRitem->ObjCBIndex = 0;
	sphereRitem->Geo = mGeometries["sphereGeo"].get();
	sphereRitem->Mat = mMaterials["default"].get();
	sphereRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	sphereRitem->IndexCount = sphereRitem->Geo->DrawArgs["sphere"].IndexCount;
	sphereRitem->StartIndexLocation = sphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
	sphereRitem->BaseVertexLocation = sphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;
	mAllRitems.push_back(std::move(sphereRitem));

	return GameResultOk;
}

//GameResult Renderer::CompileDXRShaders() {
//	{
//		const auto filePath = ShaderFilePathW + L"LearningDXR\\RayGen.hlsl";
//		auto rayGenInfo = D3D12ShaderInfo(filePath.c_str(), L"", L"lib_6_3");
//		CheckGameResult(mShaderManager.CompileShader(rayGenInfo, "RayGen"));
//	}
//	{
//		const auto filePath = ShaderFilePathW + L"LearningDXR\\Miss.hlsl";
//		auto rayGenInfo = D3D12ShaderInfo(filePath.c_str(), L"", L"lib_6_3");
//		CheckGameResult(mShaderManager.CompileShader(rayGenInfo, "Miss"));
//	}
//	{
//		const auto filePath = ShaderFilePathW + L"LearningDXR\\ClosestHit.hlsl";
//		auto rayGenInfo = D3D12ShaderInfo(filePath.c_str(), L"", L"lib_6_3");
//		CheckGameResult(mShaderManager.CompileShader(rayGenInfo, "Closest"));
//	}
//
//	return GameResultOk;
//}
//
//GameResult Renderer::BuildBLAS() {
//	auto geo = mGeometries["sphereGeo"].get();
//	D3D12_RAYTRACING_GEOMETRY_DESC sphereGeoDesc = {};
//	sphereGeoDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
//	sphereGeoDesc.Triangles.VertexBuffer.StartAddress = geo->VertexBufferGPU->GetGPUVirtualAddress();
//	sphereGeoDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);
//	sphereGeoDesc.Triangles.VertexCount = static_cast<UINT>(geo->VertexBufferCPU->GetBufferSize() / sizeof(Vertex));
//	sphereGeoDesc.Triangles.IndexBuffer = geo->IndexBufferGPU->GetGPUVirtualAddress();
//	sphereGeoDesc.Triangles.IndexFormat = DXGI_FORMAT_R16_UINT;
//	sphereGeoDesc.Triangles.IndexCount = static_cast<UINT>(geo->IndexBufferCPU->GetBufferSize() / sizeof(std::uint16_t));
//	sphereGeoDesc.Triangles.Transform3x4 = 0;
//	sphereGeoDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
//	
//	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
//
//	// Get the size requirements for the BLAS buffers
//	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
//	inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
//	inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
//	inputs.pGeometryDescs = &sphereGeoDesc;
//	inputs.NumDescs = 1;
//	inputs.Flags = buildFlags;
//
//	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO preBuildInfo = {};
//	md3dDevice->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &preBuildInfo);
//
//	preBuildInfo.ScratchDataSizeInBytes = Align(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, preBuildInfo.ScratchDataSizeInBytes);
//	preBuildInfo.ResultDataMaxSizeInBytes = Align(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, preBuildInfo.ResultDataMaxSizeInBytes);
//
//	// Create the BLAS scratch buffer
//	D3D12BufferCreateInfo bufferInfo(preBuildInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
//	bufferInfo.Alignment = std::max(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
//	CheckGameResult(D3D12Util::CreateBuffer(md3dDevice.Get(), bufferInfo, mBLAS.Scratch.GetAddressOf()));
//
//	// Create the BLAS buffer
//	bufferInfo.Size = preBuildInfo.ResultDataMaxSizeInBytes;
//	bufferInfo.State = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
//	CheckGameResult(D3D12Util::CreateBuffer(md3dDevice.Get(), bufferInfo, mBLAS.Result.GetAddressOf()));
//
//	// Describe and build the bottom level acceleration structure
//	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
//	buildDesc.Inputs = inputs;
//	buildDesc.ScratchAccelerationStructureData = mBLAS.Scratch->GetGPUVirtualAddress();
//	buildDesc.DestAccelerationStructureData = mBLAS.Result->GetGPUVirtualAddress();
//
//	mCommandList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);
//
//	// Wait for the BLAS build to complete
//	D3D12_RESOURCE_BARRIER uavBarrier;
//	uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
//	uavBarrier.UAV.pResource = mBLAS.Result.Get();
//	uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
//	mCommandList->ResourceBarrier(1, &uavBarrier);
//
//	return GameResultOk;
//}
//
//GameResult Renderer::BuildTLAS() {
//	// Describe the TLAS geometry instance(s)
//	D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};
//	instanceDesc.InstanceID = 0;
//	instanceDesc.InstanceContributionToHitGroupIndex = 0;
//	instanceDesc.InstanceMask = 0xFF;
//	instanceDesc.Transform[0][0] = instanceDesc.Transform[1][1] = instanceDesc.Transform[2][2] = 1.0f;
//	instanceDesc.AccelerationStructure = mBLAS.Result->GetGPUVirtualAddress();
//	instanceDesc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE;
//
//	// Create the TLAS instance buffer
//	D3D12BufferCreateInfo instanceBufferInfo;
//	instanceBufferInfo.Size = sizeof(instanceDesc);
//	instanceBufferInfo.HeapType = D3D12_HEAP_TYPE_UPLOAD;
//	instanceBufferInfo.Flags = D3D12_RESOURCE_FLAG_NONE;
//	instanceBufferInfo.State = D3D12_RESOURCE_STATE_GENERIC_READ;
//	CheckGameResult(D3D12Util::CreateBuffer(md3dDevice.Get(), instanceBufferInfo, mTLAS.InstanceDesc.GetAddressOf()));
//
//	// Copy the instance data to the buffer
//	UINT8* pData;
//	ReturnIfFailed(mTLAS.InstanceDesc->Map(0, nullptr, (void**)&pData));
//	std::memcpy(pData, &instanceDesc, sizeof(instanceDesc));
//	mTLAS.InstanceDesc->Unmap(0, nullptr);
//
//	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
//
//	// Get the size requirements for the TLAS buffers
//	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
//	inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
//	inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
//	inputs.InstanceDescs = mTLAS.InstanceDesc->GetGPUVirtualAddress();
//	inputs.NumDescs = 1;
//	inputs.Flags = buildFlags;
//
//	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO preBuildInfo = {};
//	md3dDevice->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &preBuildInfo);
//
//	preBuildInfo.ResultDataMaxSizeInBytes = Align(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, preBuildInfo.ResultDataMaxSizeInBytes);
//	preBuildInfo.ScratchDataSizeInBytes = Align(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, preBuildInfo.ScratchDataSizeInBytes);
//
//	// Set TLAS size
//	mTLAS.ResultDataMaxSizeInBytes = preBuildInfo.ResultDataMaxSizeInBytes;
//
//	// Create TLAS scratch buffer
//	D3D12BufferCreateInfo bufferInfo(preBuildInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
//	bufferInfo.Alignment = std::max(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
//	CheckGameResult(D3D12Util::CreateBuffer(md3dDevice.Get(), bufferInfo, mTLAS.Scratch.GetAddressOf()));
//
//	// Create the TLAS buffer
//	bufferInfo.Size = preBuildInfo.ResultDataMaxSizeInBytes;
//	bufferInfo.State = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
//	CheckGameResult(D3D12Util::CreateBuffer(md3dDevice.Get(), bufferInfo, mTLAS.Result.GetAddressOf()));
//
//	// Describe and build the TLAS
//	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
//	buildDesc.Inputs = inputs;
//	buildDesc.ScratchAccelerationStructureData = mTLAS.Scratch->GetGPUVirtualAddress();
//	buildDesc.DestAccelerationStructureData = mTLAS.Result->GetGPUVirtualAddress();
//
//	mCommandList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);
//
//	// Wait for the TLAS build to complete
//	D3D12_RESOURCE_BARRIER uavBarrier;
//	uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
//	uavBarrier.UAV.pResource = mTLAS.Result.Get();
//	uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
//	mCommandList->ResourceBarrier(1, &uavBarrier);
//
//	return GameResultOk;
//}
//
//GameResult Renderer::BuildLocalRootSignature() {
//	// Describe the ray generation root signature
//	D3D12_DESCRIPTOR_RANGE ranges[3];
//
//	ranges[0].BaseShaderRegister = 0;
//	ranges[0].NumDescriptors = 2;
//	ranges[0].RegisterSpace = 0;
//	ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
//	ranges[0].OffsetInDescriptorsFromTableStart = 0;
//
//	ranges[1].BaseShaderRegister = 0;
//	ranges[1].NumDescriptors = 1;
//	ranges[1].RegisterSpace = 0;
//	ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
//	ranges[1].OffsetInDescriptorsFromTableStart = 2;
//
//	ranges[2].BaseShaderRegister = 0;
//	ranges[2].NumDescriptors = 3;
//	ranges[2].RegisterSpace = 0;
//	ranges[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
//	ranges[2].OffsetInDescriptorsFromTableStart = 3;
//
//	D3D12_ROOT_PARAMETER params = {};
//	params.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
//	params.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
//	params.DescriptorTable.NumDescriptorRanges = _countof(ranges);
//	params.DescriptorTable.pDescriptorRanges = ranges;
//
//	D3D12_ROOT_PARAMETER rootParams[] = { params };
//
//	D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
//	rootDesc.NumParameters = _countof(rootParams);
//	rootDesc.pParameters = rootParams;
//	rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
//
//	CheckGameResult(D3D12Util::CreateRootSignature(md3dDevice.Get(), rootDesc, mLocalRootSignature.GetAddressOf()));
//
//	return GameResultOk;
//}
//
//GameResult Renderer::BuildGlobalRootSignature() {
//	D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
//	rootDesc.NumParameters = 0;
//	rootDesc.pParameters = nullptr;
//	rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
//
//	CheckGameResult(D3D12Util::CreateRootSignature(md3dDevice.Get(), rootDesc, mGlobalRootSignature.GetAddressOf()));
//
//	return GameResultOk;
//}

GameResult Renderer::BuildDXRShaders() {
	{
		const auto filePath = ShaderFilePathW + L"LearningDXR\\RayGen.hlsl";
		auto rayGenInfo = D3D12ShaderInfo(filePath.c_str(), L"", L"lib_6_3");
		CheckGameResult(mShaderManager.CompileShader(rayGenInfo, "RayGen"));
	}
	{
		const auto filePath = ShaderFilePathW + L"LearningDXR\\Miss.hlsl";
		auto rayGenInfo = D3D12ShaderInfo(filePath.c_str(), L"", L"lib_6_3");
		CheckGameResult(mShaderManager.CompileShader(rayGenInfo, "Miss"));
	}
	{
		const auto filePath = ShaderFilePathW + L"LearningDXR\\ClosestHit.hlsl";
		auto rayGenInfo = D3D12ShaderInfo(filePath.c_str(), L"", L"lib_6_3");
		CheckGameResult(mShaderManager.CompileShader(rayGenInfo, "ClosestHit"));
	}

	return GameResultOk;
}

GameResult Renderer::BuildGlobalRootSignatures() {
	// Global Root Signature
	// This is a root signature that is shared across all raytracing shaders invoked during a DispatchRays() call.
	CD3DX12_DESCRIPTOR_RANGE ranges_1[1];
	ranges_1[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

	CD3DX12_ROOT_PARAMETER params[static_cast<unsigned long long>(GlobalRootSignatureParams::Count)];
	params[static_cast<unsigned long long>(GlobalRootSignatureParams::EOutput)].InitAsDescriptorTable(1, ranges_1);
	params[static_cast<unsigned long long>(GlobalRootSignatureParams::EAcclerationStructure)].InitAsShaderResourceView(0);
	params[static_cast<unsigned long long>(GlobalRootSignatureParams::EPassCB)].InitAsConstantBufferView(0);

	CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(_countof(params), params);

	CheckGameResult(D3D12Util::CreateRootSignature(md3dDevice.Get(), globalRootSignatureDesc, mGlobalRootSignature.GetAddressOf()));

	return GameResultOk;
}

GameResult Renderer::BuildLocalRootSignatures() {
	// Local Root Signature
	// This is a root signature that enables a shader to have unique arguments that come from shader tables.
	CD3DX12_DESCRIPTOR_RANGE ranges_1[1];
	ranges_1[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 1);

	CD3DX12_ROOT_PARAMETER params[static_cast<unsigned long long>(LocalRootSignatureParams::Count)];
	params[static_cast<unsigned long long>(LocalRootSignatureParams::EGeometry)].InitAsDescriptorTable(1, ranges_1);
	params[static_cast<unsigned long long>(LocalRootSignatureParams::EMaterialCB)].InitAsConstantBufferView(1);
	
	CD3DX12_ROOT_SIGNATURE_DESC localRootSignatureDesc(_countof(params), params);
	localRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

	CheckGameResult(D3D12Util::CreateRootSignature(md3dDevice.Get(), localRootSignatureDesc, mLocalRootSignature.GetAddressOf()));

	return GameResultOk;
}

GameResult Renderer::BuildDXRResources() {
	// Describe the DXR output resource (texture)
	// Dimensions and format should match the swapchain
	// Initialize as a copy source, since we will copy this buffer's contents to the swapchain
	D3D12_RESOURCE_DESC desc = {};
	desc.DepthOrArraySize = 1;
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	desc.Width = mClientWidth;
	desc.Height = mClientHeight;
	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	desc.MipLevels = 1;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;

	// Create the buffer resource
	D3D12_HEAP_PROPERTIES heapProps = {
		D3D12_HEAP_TYPE_DEFAULT,
		D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
		D3D12_MEMORY_POOL_UNKNOWN,
		0, 0
	};

	ReturnIfFailed(md3dDevice->CreateCommittedResource(
		&heapProps, 
		D3D12_HEAP_FLAG_NONE, 
		&desc, 
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		nullptr, 
		IID_PPV_ARGS(&mDXROutput)
	));

	return GameResultOk;
}

GameResult Renderer::BuildDXRDescriptorHeaps() {
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	// 1 UAV for the RT output
	// 2 SRV for vertex and index buffers
	desc.NumDescriptors = 3;
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	desc.NodeMask = 0;

	ReturnIfFailed(md3dDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&mDXRDescriptorHeap)));

	return GameResultOk;
}

GameResult Renderer::BuildDXRDescriptors() {
	auto handle = mDXRDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

	// Create the DXR output buffer UAV
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

	md3dDevice->CreateUnorderedAccessView(mDXROutput.Get(), nullptr, &uavDesc, handle);

	// Create the vertex buffer SRV
	auto geo = mGeometries["sphereGeo"].get();

	D3D12_SHADER_RESOURCE_VIEW_DESC vertexSrvDesc = {};
	vertexSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	vertexSrvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	vertexSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
	vertexSrvDesc.Buffer.StructureByteStride = 0;
	vertexSrvDesc.Buffer.FirstElement = 0;
	vertexSrvDesc.Buffer.NumElements = static_cast<UINT>(geo->VertexBufferCPU->GetBufferSize() / sizeof(float));
	vertexSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	handle.ptr += mCbvSrvUavDescriptorSize;
	md3dDevice->CreateShaderResourceView(mVertexBufferSrv["sphereGeo"].Get(), &vertexSrvDesc, handle);

	D3D12_SHADER_RESOURCE_VIEW_DESC indexSrvDesc = {};
	indexSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	indexSrvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	indexSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
	indexSrvDesc.Buffer.StructureByteStride = 0;
	indexSrvDesc.Buffer.FirstElement = 0;
	indexSrvDesc.Buffer.NumElements = static_cast<UINT>(geo->IndexBufferCPU->GetBufferSize() / sizeof(std::uint32_t));
	indexSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	handle.ptr += mCbvSrvUavDescriptorSize;
	md3dDevice->CreateShaderResourceView(mIndexBufferSrv["sphereGeo"].Get(), &indexSrvDesc, handle);

	return GameResultOk;
}

GameResult Renderer::BuildBLASs() {
	auto geo = mGeometries["sphereGeo"].get();

	D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
	geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
	geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
	geometryDesc.Triangles.VertexCount = static_cast<UINT>(geo->VertexBufferCPU->GetBufferSize() / sizeof(Vertex));
	geometryDesc.Triangles.VertexBuffer.StartAddress = geo->VertexBufferGPU->GetGPUVirtualAddress();
	geometryDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);
	geometryDesc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
	geometryDesc.Triangles.IndexCount = static_cast<UINT>(geo->IndexBufferCPU->GetBufferSize() / sizeof(std::uint32_t));
	geometryDesc.Triangles.IndexBuffer = geo->IndexBufferGPU->GetGPUVirtualAddress();
	geometryDesc.Triangles.Transform3x4 = 0;
	// Mark the geometry as opaque. 
	// PERFORMANCE TIP: mark geometry as opaque whenever applicable as it can enable important ray processing optimizations.
	// Note: When rays encounter opaque geometry an any hit shader will not be executed whether it is present or not.
	geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

	// Get the size requirements for the BLAS buffers
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
	inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	inputs.pGeometryDescs = &geometryDesc;
	inputs.NumDescs = 1;
	inputs.Flags = buildFlags;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo = {};
	md3dDevice->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuildInfo);

	prebuildInfo.ScratchDataSizeInBytes = Align(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, prebuildInfo.ScratchDataSizeInBytes);
	prebuildInfo.ResultDataMaxSizeInBytes = Align(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, prebuildInfo.ResultDataMaxSizeInBytes);

	// Create the BLAS scratch buffer
	D3D12BufferCreateInfo bufferInfo(prebuildInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	bufferInfo.Alignment = std::max(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
	CheckGameResult(D3D12Util::CreateBuffer(md3dDevice.Get(), bufferInfo, mBLAS.Scratch.GetAddressOf()));

	// Create the BLAS buffer
	bufferInfo.Size = prebuildInfo.ResultDataMaxSizeInBytes;
	bufferInfo.State = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
	CheckGameResult(D3D12Util::CreateBuffer(md3dDevice.Get(), bufferInfo, mBLAS.Result.GetAddressOf()));

	// Describe and build the bottom level acceleration structure
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
	buildDesc.Inputs = inputs;
	buildDesc.ScratchAccelerationStructureData = mBLAS.Scratch->GetGPUVirtualAddress();
	buildDesc.DestAccelerationStructureData = mBLAS.Result->GetGPUVirtualAddress();

	mCommandList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

	// Wait for the BLAS build to complete
	D3D12_RESOURCE_BARRIER uavBarrier;
	uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	uavBarrier.UAV.pResource = mBLAS.Result.Get();
	uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	mCommandList->ResourceBarrier(1, &uavBarrier);

	return GameResultOk;
}

GameResult Renderer::BuildTLASs() {
	// Describe the TLAS geometry instance(s)
	D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};
	instanceDesc.InstanceID = 0;
	instanceDesc.InstanceContributionToHitGroupIndex = 0;
	instanceDesc.InstanceMask = 0xFF;
	instanceDesc.Transform[0][0] = instanceDesc.Transform[1][1] = instanceDesc.Transform[2][2] = 1.0f;
	instanceDesc.AccelerationStructure = mBLAS.Result->GetGPUVirtualAddress();
	instanceDesc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE;

	// Create the TLAS instance buffer
	D3D12BufferCreateInfo instanceBufferInfo;
	instanceBufferInfo.Size = sizeof(instanceDesc);
	instanceBufferInfo.HeapType = D3D12_HEAP_TYPE_UPLOAD;
	instanceBufferInfo.Flags = D3D12_RESOURCE_FLAG_NONE;
	instanceBufferInfo.State = D3D12_RESOURCE_STATE_GENERIC_READ;
	CheckGameResult(D3D12Util::CreateBuffer(md3dDevice.Get(), instanceBufferInfo, mTLAS.InstanceDesc.GetAddressOf()));

	// Copy the instance data to the buffer
	void* pData;
	ReturnIfFailed(mTLAS.InstanceDesc->Map(0, nullptr, &pData));
	std::memcpy(pData, &instanceDesc, sizeof(instanceDesc));
	mTLAS.InstanceDesc->Unmap(0, nullptr);

	// Get the size requirements for the TLAS buffers
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
	inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	inputs.InstanceDescs = mTLAS.InstanceDesc->GetGPUVirtualAddress();
	inputs.NumDescs = 1;
	inputs.Flags = buildFlags;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo = {};
	md3dDevice->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuildInfo);

	prebuildInfo.ResultDataMaxSizeInBytes = Align(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, prebuildInfo.ResultDataMaxSizeInBytes);
	prebuildInfo.ScratchDataSizeInBytes = Align(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, prebuildInfo.ScratchDataSizeInBytes);

	// Set TLAS size
	mTLAS.ResultDataMaxSizeInBytes = prebuildInfo.ResultDataMaxSizeInBytes;

	// Create TLAS sratch buffer
	D3D12BufferCreateInfo bufferInfo(prebuildInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	bufferInfo.Alignment = std::max(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
	CheckGameResult(D3D12Util::CreateBuffer(md3dDevice.Get(), bufferInfo, mTLAS.Scratch.GetAddressOf()));

	// Create the TLAS buffer
	bufferInfo.Size = prebuildInfo.ResultDataMaxSizeInBytes;
	bufferInfo.State = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
	CheckGameResult(D3D12Util::CreateBuffer(md3dDevice.Get(), bufferInfo, mTLAS.Result.GetAddressOf()));

	// Describe and build the TLAS
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
	buildDesc.Inputs = inputs;
	buildDesc.ScratchAccelerationStructureData = mTLAS.Scratch->GetGPUVirtualAddress();
	buildDesc.DestAccelerationStructureData = mTLAS.Result->GetGPUVirtualAddress();

	mCommandList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

	// Wait for the TLAS build to complete
	D3D12_RESOURCE_BARRIER uavBarrier;
	uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	uavBarrier.UAV.pResource = mTLAS.Result.Get();
	uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	mCommandList->ResourceBarrier(1, &uavBarrier);

	return GameResultOk;
}

GameResult Renderer::BuildDXRPSOs() {
	CD3DX12_STATE_OBJECT_DESC raytracingPipeline = { D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };

	// DXIL library
	// This contains the shaders and their entrypoints for the state object.
	// Since shaders are not considered a subobject, they need to be passed in via DXIL library subobjects.
	auto rayGenLib = raytracingPipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
	auto rayGenShader = mShaderManager.GetRTShader("RayGen");
	D3D12_SHADER_BYTECODE rayGenLibDxil = CD3DX12_SHADER_BYTECODE(rayGenShader->GetBufferPointer(), rayGenShader->GetBufferSize());
	rayGenLib->SetDXILLibrary(&rayGenLibDxil);
	rayGenLib->DefineExport(L"RayGen");

	auto missLib = raytracingPipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
	auto missShader = mShaderManager.GetRTShader("Miss");
	D3D12_SHADER_BYTECODE missLibDxil = CD3DX12_SHADER_BYTECODE(missShader->GetBufferPointer(), missShader->GetBufferSize());
	missLib->SetDXILLibrary(&missLibDxil);
	missLib->DefineExport(L"Miss");

	auto chitLib = raytracingPipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
	auto chitShader = mShaderManager.GetRTShader("ClosestHit");
	D3D12_SHADER_BYTECODE chitLibDxil = CD3DX12_SHADER_BYTECODE(chitShader->GetBufferPointer(), chitShader->GetBufferSize());
	chitLib->SetDXILLibrary(&chitLibDxil);
	chitLib->DefineExport(L"ClosestHit");

	// Triangle hit group
	// A hit group specifies closest hit, any hit and intersection shaders to be executed when a ray intersects the geometry's triangle/AABB.
	auto hitGroup = raytracingPipeline.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
	hitGroup->SetClosestHitShaderImport(L"ClosestHit");
	hitGroup->SetHitGroupExport(L"HitGroup");
	hitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);

	// Shader config
	// Defines the maximum sizes in bytes for the ray payload and attribute structure.
	auto shaderConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
	UINT payloadSize = 4 * sizeof(float);	// float4 color
	UINT attributeSize = 2 * sizeof(float);	// float2 barycentrics
	shaderConfig->Config(payloadSize, attributeSize);

	// Local root signature and shader association
	// This is a root signature that enables a shader to have unique arguments that come from shader tables.
	auto localRootSignature = raytracingPipeline.CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
	localRootSignature->SetRootSignature(mLocalRootSignature.Get());

	// Shader association
	// Create a list of the shader export names that use the root signature
	const WCHAR* localRootSignatureExports[] = { L"RayGen", L"HitGroup", L"Miss" };
	auto localRootSignatureAssociation = raytracingPipeline.CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
	localRootSignatureAssociation->SetSubobjectToAssociate(*localRootSignature);
	localRootSignatureAssociation->AddExports(localRootSignatureExports);

	// Global root signature
	// This is a root signature that is shared across all raytracing shaders invoked during a DispatchRays() call.
	auto globalRootSignature = raytracingPipeline.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
	globalRootSignature->SetRootSignature(mGlobalRootSignature.Get());

	// Pipeline config
	// Defines the maximum TraceRay() recursion depth.
	auto pipelineConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
	// PERFOMANCE TIP: Set max recursion depth as low as needed 
	// as drivers may apply optimization strategies for low recursion depths. 
	UINT maxRecursionDepth = 1;	// Primary rays only
	pipelineConfig->Config(maxRecursionDepth);

	// Create the state object
	ReturnIfFailedDxDebug(mInfoQueue, md3dDevice->CreateStateObject(raytracingPipeline, IID_PPV_ARGS(&mDXRPSOs["DXRDefault"])));

	// Get the state object properties
	ReturnIfFailedDxDebug(mInfoQueue, mDXRPSOs["DXRDefault"]->QueryInterface(IID_PPV_ARGS(&mDXRPSOProps["DXRDefaultProps"])));

	return GameResultOk;
}

GameResult Renderer::BuildShaderTables() {
	/*
		The Shader Table layout is as follows:
			Entry 0 - Ray Generation shader
			Entry 1 - Miss shader
			Entry 2 - Closest Hit shader
		All shader records in the Shader Table must have the same size, so shader record size will be based on the largest required entry.
		The ray generation program requires the largest entry:
			32 bytes - D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES
			+  8 bytes - a CBV/SRV/UAV descriptor table pointer (64-bits)
			= 40 bytes ->> aligns to 64 bytes
		The entry size must be aligned up to D3D12_RAYTRACING_SHADER_BINDING_TABLE_RECORD_BYTE_ALIGNMENT
	*/
	std::uint32_t shaderIdSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
	std::uint32_t shaderTableSize = 0;

	auto& shaderTable = mShaderTables["DXRDefault"];
	shaderTable.ShaderRecordSize = shaderIdSize;
	shaderTable.ShaderRecordSize += 8;
	shaderTable.ShaderRecordSize = Align(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT, shaderTable.ShaderRecordSize);

	shaderTableSize = (shaderTable.ShaderRecordSize * 3);	// 3 shader records in the table
	shaderTableSize = Align(D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT, shaderTableSize);

	// Create the shader table buffer
	D3D12BufferCreateInfo bufferInfo(shaderTableSize, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
	CheckGameResult(D3D12Util::CreateBuffer(md3dDevice.Get(), bufferInfo, shaderTable.Resource.GetAddressOf()));

	// Map the buffer
	std::uint8_t* pData;
	ReturnIfFailed(shaderTable.Resource->Map(0, nullptr, reinterpret_cast<void**>(&pData)));

	// Shader Record 0 -  Ray generation program and local root parameter data
	std::memcpy(pData, mDXRPSOProps["DXRDefaultProps"]->GetShaderIdentifier(L"RayGen"), shaderIdSize);



	// Unmap
	shaderTable.Resource->Unmap(0, nullptr);

	return GameResultOk;
}

//GameResult Renderer::BuildDXRDescriptorHeaps() {
//	// Describe the CBV/SRV/UAV heap
//	// Need 6 entries:
//	// 1 CBV for the ViewCB
//	// 1 CBV for the MaterialCB
//	// 1 UAV for the RT output
//	// 1 SRV for the Scene BVH
//	// 1 SRV for the index buffer
//	// 1 SRV for the vertex buffer
//	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
//	desc.NumDescriptors = 6;
//	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
//	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
//	
//	// Create the descriptor heap
//	ReturnIfFailed(md3dDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&mDXRDescriptorHeap)));
//	
//	// Get the descriptor heap handle and increment size
//	D3D12_CPU_DESCRIPTOR_HANDLE handle = mDXRDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
//	UINT handleIncrement = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
//	
//	// Create the ViewCB CBV
//	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
//	cbvDesc.SizeInBytes = Align(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, sizeof(DXRPassConstants));
//	cbvDesc.BufferLocation = mDXRPassCB->GetGPUVirtualAddress();
//	
//	md3dDevice->CreateConstantBufferView(&cbvDesc, handle);
//	
//	// Create the MaterialCB CBV
//	cbvDesc.SizeInBytes = Align(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, sizeof(DXRMaterialConstants));
//	cbvDesc.BufferLocation = mDXRMaterialCB->GetGPUVirtualAddress();
//	
//	handle.ptr += handleIncrement;
//	md3dDevice->CreateConstantBufferView(&cbvDesc, handle);
//	
//	// Create the DXR output buffer UAV
//	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
//	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
//	
//	handle.ptr += handleIncrement;
//	md3dDevice->CreateUnorderedAccessView(mDXROutput.Get(), nullptr, &uavDesc, handle);
//	
//	// Create the DXR Top Level Acceleration Structure SRV
//	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
//	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
//	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
//	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
//	srvDesc.RaytracingAccelerationStructure.Location = mTLAS.Result->GetGPUVirtualAddress();
//	
//	handle.ptr += handleIncrement;
//	md3dDevice->CreateShaderResourceView(nullptr, &srvDesc, handle);
//	
//	auto geo = mGeometries["sphereGeo"].get();
//	
//	// Create the index buffer SRV
//	D3D12_SHADER_RESOURCE_VIEW_DESC indexSRVDesc;
//	indexSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
//	indexSRVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
//	indexSRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
//	indexSRVDesc.Buffer.StructureByteStride = 0;
//	indexSRVDesc.Buffer.FirstElement = 0;
//	indexSRVDesc.Buffer.NumElements = static_cast<UINT>(geo->IndexBufferCPU->GetBufferSize() / sizeof(float));
//	indexSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
//	
//	handle.ptr += handleIncrement;
//	md3dDevice->CreateShaderResourceView(geo->IndexBufferGPU.Get(), &indexSRVDesc, handle);
//	
//	// Create the vertex buffer SRV
//	D3D12_SHADER_RESOURCE_VIEW_DESC vertexSRVDesc;
//	vertexSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
//	vertexSRVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
//	vertexSRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
//	vertexSRVDesc.Buffer.StructureByteStride = 0;
//	vertexSRVDesc.Buffer.FirstElement = 0;
//	vertexSRVDesc.Buffer.NumElements = static_cast<UINT>(geo->VertexBufferCPU->GetBufferSize() / sizeof(float));
//	vertexSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
//	
//	handle.ptr += handleIncrement;
//	md3dDevice->CreateShaderResourceView(geo->VertexBufferGPU.Get(), &vertexSRVDesc, handle);
//
//	return GameResultOk;
//}
//
//GameResult Renderer::BuildDXRPSOs() {
//	// Need 10 subobjects:
//	// 1 for RGS program
//	// 1 for Miss program
//	// 1 for CHS program
//	// 1 for Hit Group
//	// 2 for RayGen Root Signature (root-signature and association)
//	// 2 for Shader Config (config and association)
//	// 1 for Global Root Signature
//	// 1 for Pipeline Config	
//	UINT index = 0;
//	std::vector<D3D12_STATE_SUBOBJECT> subobjects;
//	subobjects.resize(10);
//
//	// Add state subobject for the RGS
//	D3D12_EXPORT_DESC rgsExportDesc = {};
//	rgsExportDesc.Name = L"RayGen_12";
//	rgsExportDesc.ExportToRename = L"RayGen";
//	rgsExportDesc.Flags = D3D12_EXPORT_FLAG_NONE;
//
//	auto& rayGenShader = mShaderManager.GetRTShader("RayGen");
//	D3D12_DXIL_LIBRARY_DESC	rgsLibDesc = {};
//	rgsLibDesc.DXILLibrary.BytecodeLength = rayGenShader->GetBufferSize();
//	rgsLibDesc.DXILLibrary.pShaderBytecode = rayGenShader->GetBufferPointer();
//	rgsLibDesc.NumExports = 1;
//	rgsLibDesc.pExports = &rgsExportDesc;
//
//	D3D12_STATE_SUBOBJECT rgs = {};
//	rgs.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
//	rgs.pDesc = &rgsLibDesc;
//
//	subobjects[index++] = rgs;
//
//	// Add state subobject for the Miss shader
//	D3D12_EXPORT_DESC msExportDesc = {};
//	msExportDesc.Name = L"Miss_5";
//	msExportDesc.ExportToRename = L"Miss";
//	msExportDesc.Flags = D3D12_EXPORT_FLAG_NONE;
//
//	auto& missShader = mShaderManager.GetRTShader("Miss");
//	D3D12_DXIL_LIBRARY_DESC	msLibDesc = {};
//	msLibDesc.DXILLibrary.BytecodeLength = missShader->GetBufferSize();
//	msLibDesc.DXILLibrary.pShaderBytecode = missShader->GetBufferPointer();
//	msLibDesc.NumExports = 1;
//	msLibDesc.pExports = &msExportDesc;
//
//	D3D12_STATE_SUBOBJECT ms = {};
//	ms.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
//	ms.pDesc = &msLibDesc;
//
//	subobjects[index++] = ms;
//
//	// Add state subobject for the Closest Hit shader
//	D3D12_EXPORT_DESC chsExportDesc = {};
//	chsExportDesc.Name = L"ClosestHit_76";
//	chsExportDesc.ExportToRename = L"ClosestHit";
//	chsExportDesc.Flags = D3D12_EXPORT_FLAG_NONE;
//
//	auto& closestShader = mShaderManager.GetRTShader("Closest");
//	D3D12_DXIL_LIBRARY_DESC	chsLibDesc = {};
//	chsLibDesc.DXILLibrary.BytecodeLength = closestShader->GetBufferSize();
//	chsLibDesc.DXILLibrary.pShaderBytecode = closestShader->GetBufferPointer();
//	chsLibDesc.NumExports = 1;
//	chsLibDesc.pExports = &chsExportDesc;
//
//	D3D12_STATE_SUBOBJECT chs = {};
//	chs.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
//	chs.pDesc = &chsLibDesc;
//
//	subobjects[index++] = chs;
//
//	// Add a state subobject for the hit group
//	D3D12_HIT_GROUP_DESC hitGroupDesc = {};
//	hitGroupDesc.ClosestHitShaderImport = L"ClosestHit_76";
//	hitGroupDesc.HitGroupExport = L"HitGroup";
//
//	D3D12_STATE_SUBOBJECT hitGroup = {};
//	hitGroup.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
//	hitGroup.pDesc = &hitGroupDesc;
//
//	subobjects[index++] = hitGroup;
//
//	// Add a state subobject for the shader payload configuration
//	D3D12_RAYTRACING_SHADER_CONFIG shaderDesc = {};
//	shaderDesc.MaxPayloadSizeInBytes = sizeof(XMFLOAT4);	// RGB and HitT
//	shaderDesc.MaxAttributeSizeInBytes = D3D12_RAYTRACING_MAX_ATTRIBUTE_SIZE_IN_BYTES;
//
//	D3D12_STATE_SUBOBJECT shaderConfigObject = {};
//	shaderConfigObject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
//	shaderConfigObject.pDesc = &shaderDesc;
//
//	subobjects[index++] = shaderConfigObject;
//
//	// Create a list of the shader export names that use the payload
//	const WCHAR* shaderExports[] = { L"RayGen_12", L"Miss_5", L"HitGroup" };
//
//	// Add a state subobject for the association between shaders and the payload
//	D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION shaderPayloadAssociation = {};
//	shaderPayloadAssociation.NumExports = _countof(shaderExports);
//	shaderPayloadAssociation.pExports = shaderExports;
//	shaderPayloadAssociation.pSubobjectToAssociate = &subobjects[(index - 1)];
//
//	D3D12_STATE_SUBOBJECT shaderPayloadAssociationObject = {};
//	shaderPayloadAssociationObject.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
//	shaderPayloadAssociationObject.pDesc = &shaderPayloadAssociation;
//
//	subobjects[index++] = shaderPayloadAssociationObject;
//
//	// Add a state subobject for the shared root signature 
//	D3D12_STATE_SUBOBJECT rayGenRootSigObject = {};
//	rayGenRootSigObject.Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
//	rayGenRootSigObject.pDesc = mLocalRootSignature.GetAddressOf();
//
//	subobjects[index++] = rayGenRootSigObject;
//
//	// Create a list of the shader export names that use the root signature
//	const WCHAR* rootSigExports[] = { L"RayGen_12", L"HitGroup", L"Miss_5" };
//
//	// Add a state subobject for the association between the RayGen shader and the local root signature
//	D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION rayGenShaderRootSigAssociation = {};
//	rayGenShaderRootSigAssociation.NumExports = _countof(rootSigExports);
//	rayGenShaderRootSigAssociation.pExports = rootSigExports;
//	rayGenShaderRootSigAssociation.pSubobjectToAssociate = &subobjects[(index - 1)];
//
//	D3D12_STATE_SUBOBJECT rayGenShaderRootSigAssociationObject = {};
//	rayGenShaderRootSigAssociationObject.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
//	rayGenShaderRootSigAssociationObject.pDesc = &rayGenShaderRootSigAssociation;
//
//	subobjects[index++] = rayGenShaderRootSigAssociationObject;
//
//	D3D12_STATE_SUBOBJECT globalRootSig;
//	globalRootSig.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
//	globalRootSig.pDesc = mGlobalRootSignature.GetAddressOf();
//
//	subobjects[index++] = globalRootSig;
//
//	// Add a state subobject for the ray tracing pipeline config
//	D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig = {};
//	pipelineConfig.MaxTraceRecursionDepth = 1;
//
//	D3D12_STATE_SUBOBJECT pipelineConfigObject = {};
//	pipelineConfigObject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
//	pipelineConfigObject.pDesc = &pipelineConfig;
//
//	subobjects[index++] = pipelineConfigObject;
//
//	// Describe the Ray Tracing Pipeline State Object
//	D3D12_STATE_OBJECT_DESC pipelineDesc = {};
//	pipelineDesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
//	pipelineDesc.NumSubobjects = static_cast<UINT>(subobjects.size());
//	pipelineDesc.pSubobjects = subobjects.data();
//
//	// Create the RT Pipeline State Object (RTPSO)
//	ReturnIfFailed(md3dDevice->CreateStateObject(&pipelineDesc, IID_PPV_ARGS(&mDXRPSOs["RTXDefault"])));
//
//	// Get the RTPSO properties
//	ReturnIfFailed(mDXRPSOs["RTXDefault"]->QueryInterface(IID_PPV_ARGS(&mDXRPSOProps["RTXDefaultProp"])));
//
//	return GameResultOk;
//}
//
//GameResult Renderer::BuildShaderBindingTables() {
//	/*
//	The Shader Table layout is as follows:
//		Entry 0 - Ray Generation shader
//		Entry 1 - Miss shader
//		Entry 2 - Closest Hit shader
//	All shader records in the Shader Table must have the same size, so shader record size will be based on the largest required entry.
//	The ray generation program requires the largest entry:
//		32 bytes - D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES
//	  +  8 bytes - a CBV/SRV/UAV descriptor table pointer (64-bits)
//	  = 40 bytes ->> aligns to 64 bytes
//	The entry size must be aligned up to D3D12_RAYTRACING_SHADER_BINDING_TABLE_RECORD_BYTE_ALIGNMENT
//	*/
//
//	uint32_t shaderIdSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
//	uint32_t shaderTableSize = 0;
//	
//	mShaderTableRecordSize = shaderIdSize;
//	mShaderTableRecordSize += 8;							// CBV/SRV/UAV descriptor table
//	mShaderTableRecordSize = Align(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT, mShaderTableRecordSize);
//	
//	shaderTableSize = (mShaderTableRecordSize * 3);		// 3 shader records in the table
//	shaderTableSize = Align(D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT, shaderTableSize);
//	
//	// Create the shader table buffer
//	D3D12BufferCreateInfo bufferInfo(shaderTableSize, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
//	CheckGameResult(D3D12Util::CreateBuffer(md3dDevice.Get(), bufferInfo, mShaderTable.GetAddressOf()));
//	
//	// Map the buffer
//	uint8_t* pData;
//	ReturnIfFailed(mShaderTable->Map(0, nullptr, (void**)&pData));
//	
//	// Shader Record 0 - Ray Generation program and local root parameter data (descriptor table with constant buffer and IB/VB pointers)
//	std::memcpy(pData, mDXRPSOProps["RTXDefaultProp"]->GetShaderIdentifier(L"RayGen_12"), shaderIdSize);
//	
//	// Set the root parameter data. Point to start of descriptor heap.
//	*reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(pData + shaderIdSize) = mDXRDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
//	
//	// Shader Record 1 - Miss program (no local root arguments to set)
//	pData += mShaderTableRecordSize;
//	std::memcpy(pData, mDXRPSOProps["RTXDefaultProp"]->GetShaderIdentifier(L"Miss_5"), shaderIdSize);
//	
//	// Shader Record 2 - Closest Hit program and local root parameter data (descriptor table with constant buffer and IB/VB pointers)
//	pData += mShaderTableRecordSize;
//	std::memcpy(pData, mDXRPSOProps["RTXDefaultProp"]->GetShaderIdentifier(L"HitGroup"), shaderIdSize);
//	
//	// Set the root parameter data. Point to start of descriptor heap.
//	*reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(pData + shaderIdSize) = mDXRDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
//	
//	// Unmap
//	mShaderTable->Unmap(0, nullptr);
//
//	return GameResultOk;
//}

GameResult Renderer::UpdateCamera(const GameTimer& gt) {
	mEyePos.x = mRadius * sinf(mPhi) * cosf(mTheta);
	mEyePos.z = mRadius * sinf(mPhi) * sinf(mTheta);
	mEyePos.y = mRadius * cosf(mPhi);

	XMVECTOR pos = XMVectorSet(mEyePos.x, mEyePos.y, mEyePos.z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&mView, view);

	return GameResultOk;
}

GameResult Renderer::UpdateObjectCB(const GameTimer& gt) {
	auto& currObjectCB = mCurrFrameResource->ObjectCB;
	for (auto& e : mAllRitems) {
		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		if (e->NumFramesDirty > 0) {
			XMMATRIX world = XMLoadFloat4x4(&e->World);
			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);
	
			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));
	
			currObjectCB.CopyData(e->ObjCBIndex, objConstants);
	
			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;
		}
	}

	return GameResultOk;
}

GameResult Renderer::UpdatePassCB(const GameTimer& gt) {
	XMMATRIX view = XMLoadFloat4x4(&mView);
	XMMATRIX proj = XMLoadFloat4x4(&mProj);
	XMMATRIX viewProj = XMMatrixMultiply(view, proj);

	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	mMainPassCB.EyePosW = mEyePos;
	mMainPassCB.AmbientLight = { 0.3f, 0.3f, 0.42f, 1.0f };
	mMainPassCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[0].Strength = { 0.6f, 0.6f, 0.6f };
	mMainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[1].Strength = { 0.3f, 0.3f, 0.3f };
	mMainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	mMainPassCB.Lights[2].Strength = { 0.15f, 0.15f, 0.15f };

	auto& currPassCB = mCurrFrameResource->PassCB;
	currPassCB.CopyData(0, mMainPassCB);

	return GameResultOk;
}

GameResult Renderer::UpdateMaterialCB(const GameTimer& gt) {
	auto& currMaterialCB = mCurrFrameResource->MaterialCB;
	for (auto& e : mMaterials) {
		// Only update the cbuffer data if the constants have changed.  If the cbuffer
		// data changes, it needs to be updated for each FrameResource.
		Material* mat = e.second.get();
		if (mat->NumFramesDirty > 0) {
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialConstants matConsts;
			matConsts.DiffuseAlbedo = mat->DiffuseAlbedo;
			matConsts.FresnelR0 = mat->FresnelR0;
			matConsts.Roughness = mat->Roughness;
			XMStoreFloat4x4(&matConsts.MatTransform, XMMatrixTranspose(matTransform));

			currMaterialCB.CopyData(mat->MatCBIndex, matConsts);

			// Next FrameResource need to be updated too.
			mat->NumFramesDirty--;
		}
	}

	return GameResultOk;
}

//GameResult Renderer::UpdateDXRPassCB(const GameTimer& gt) {
//	XMMATRIX view = XMLoadFloat4x4(&mView);
//	XMMATRIX proj = XMLoadFloat4x4(&mProj);
//	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
//
//	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
//	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
//	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);
//
//	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
//	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
//	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
//	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
//	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
//	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
//	
//	mDXRPassCBData.EyePosW = mEyePos;
//	mDXRPassCBData.Resolution = DirectX::XMFLOAT2(static_cast<float>(mClientWidth), static_cast<float>(mClientHeight));
//	
//	void* pData;
//	ReturnIfFailed(mDXRPassCB->Map(0, nullptr, reinterpret_cast<void**>(&pData)));
//	std::memcpy(pData, &mDXRPassCBData, sizeof(mDXRPassCBData));
//	mDXRPassCB->Unmap(0, nullptr);
//
//	return GameResultOk;
//}
//
//GameResult Renderer::UpdateDXRInstancesTransforms(const GameTimer& gt) {
//	 
//	return GameResultOk;
//}

GameResult Renderer::Rasterize() {
	ReturnIfFailed(mCommandList->Reset(mCurrFrameResource->CmdListAlloc.Get(), mPSOs["Default"].Get()));

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	ID3D12DescriptorHeap* descriptorHeaps[] = { mDescriptorHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	auto hDesc = CD3DX12_CPU_DESCRIPTOR_HANDLE(mRtvHeap->GetCPUDescriptorHandleForHeapStart(), SwapChainBufferCount + mCurrFrameResourceIndex, mRtvDescriptorSize);
	mCommandList->ClearRenderTargetView(hDesc, Colors::AliceBlue, 0, nullptr);
	mCommandList->ClearDepthStencilView(
		DepthStencilView(),
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
		1.0f,
		0,
		0,
		nullptr
	);

	mCommandList->OMSetRenderTargets(1, &hDesc, true, &DepthStencilView());

	auto& passCB = mCurrFrameResource->PassCB;
	mCommandList->SetGraphicsRootConstantBufferView(2, passCB.Resource()->GetGPUVirtualAddress());

	auto pMainRenderTarget = mMainRenderTargets[mCurrFrameResourceIndex].Get();

	mCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			pMainRenderTarget,
			D3D12_RESOURCE_STATE_COPY_SOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET)
	);

	CheckGameResult(DrawRenderItems());

	mCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			pMainRenderTarget,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_COPY_SOURCE)
	);

	mCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			CurrentBackBuffer(),
			D3D12_RESOURCE_STATE_PRESENT,
			D3D12_RESOURCE_STATE_COPY_DEST)
	);

	mCommandList->CopyResource(CurrentBackBuffer(), pMainRenderTarget);

	mCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			CurrentBackBuffer(),
			D3D12_RESOURCE_STATE_COPY_DEST,
			D3D12_RESOURCE_STATE_PRESENT)
	);

	return GameResultOk;
}

GameResult Renderer::Raytrace() {
	ReturnIfFailed(mCommandList->Reset(mCurrFrameResource->CmdListAlloc.Get(), nullptr));

	//// Set the UAV/SRV/CBV and sampler heaps
	//ID3D12DescriptorHeap* ppHeaps[] = { mDXRDescriptorHeap.Get() };
	//mCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	//
	//// Dispatch rays
	//D3D12_DISPATCH_RAYS_DESC desc = {};
	//desc.RayGenerationShaderRecord.StartAddress = mShaderTable->GetGPUVirtualAddress();
	//desc.RayGenerationShaderRecord.SizeInBytes = mShaderTableRecordSize;
	//
	//desc.MissShaderTable.StartAddress = mShaderTable->GetGPUVirtualAddress() + mShaderTableRecordSize;
	//desc.MissShaderTable.SizeInBytes = mShaderTableRecordSize; // Only a single Miss program entry
	//desc.MissShaderTable.StrideInBytes = mShaderTableRecordSize;
	//
	//desc.HitGroupTable.StartAddress = mShaderTable->GetGPUVirtualAddress() + (mShaderTableRecordSize * 2);
	//desc.HitGroupTable.SizeInBytes = mShaderTableRecordSize; // Only a single Hit program entry
	//desc.HitGroupTable.StrideInBytes = mShaderTableRecordSize;
	//
	//desc.Width = mClientWidth;
	//desc.Height = mClientHeight;
	//desc.Depth = 1;
	//
	//mCommandList->SetPipelineState1(mDXRPSOs["RTXDefault"].Get());
	//mCommandList->DispatchRays(&desc);
	//

	mCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			mDXROutput.Get(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			D3D12_RESOURCE_STATE_COPY_SOURCE)
	);

	mCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			CurrentBackBuffer(),
			D3D12_RESOURCE_STATE_PRESENT,
			D3D12_RESOURCE_STATE_COPY_DEST)
	);

	mCommandList->CopyResource(CurrentBackBuffer(), mDXROutput.Get());

	mCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			CurrentBackBuffer(),
			D3D12_RESOURCE_STATE_COPY_DEST,
			D3D12_RESOURCE_STATE_PRESENT)
	);

	mCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			mDXROutput.Get(),
			D3D12_RESOURCE_STATE_COPY_SOURCE,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
	);

	return GameResultOk;
}

GameResult Renderer::DrawRenderItems() {
	UINT objCBByteSize = D3D12Util::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT matCBByteSize = D3D12Util::CalcConstantBufferByteSize(sizeof(MaterialConstants));

	auto& objectCB = mCurrFrameResource->ObjectCB;
	auto& matCB = mCurrFrameResource->MaterialCB;

	// For each render item...
	for (size_t i = 0; i < mAllRitems.size(); ++i) {
		auto& ri = mAllRitems[i];

		mCommandList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
		mCommandList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
		mCommandList->IASetPrimitiveTopology(ri->PrimitiveType);
		
		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB.Resource()->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;
		D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB.Resource()->GetGPUVirtualAddress() + ri->Mat->MatCBIndex * matCBByteSize;

		mCommandList->SetGraphicsRootConstantBufferView(1, objCBAddress);
		mCommandList->SetGraphicsRootConstantBufferView(3, matCBAddress);

		mCommandList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}

	return GameResultOk;
}