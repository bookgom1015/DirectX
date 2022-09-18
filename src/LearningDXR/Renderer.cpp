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

	// Shared
	CheckGameResult(BuildFrameResources());
	CheckGameResult(BuildGeometries());
	CheckGameResult(BuildMaterials());

	// Raterization
	CheckGameResult(CompileShaders());
	CheckGameResult(BuildRootSignatures());
	CheckGameResult(BuildResources());
	CheckGameResult(BuildDescriptorHeaps());
	CheckGameResult(BuildDescriptors());
	CheckGameResult(BuildPSOs());
	CheckGameResult(BuildRenderItems());

	// Ray-tracing
	CheckGameResult(CompileDXRShaders());
	CheckGameResult(BuildDXRRootSignatures());
	CheckGameResult(BuildDXRResources());
	CheckGameResult(BuildDXRDescriptorHeaps());
	CheckGameResult(BuildDXRDescriptors());
	CheckGameResult(BuildBLAS());
	CheckGameResult(BuildTLAS());
	CheckGameResult(BuildDXRPSOs());
	CheckGameResult(BuildDXRObjectCB());
	CheckGameResult(BuildShaderTables());

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

	CheckGameResult(UpdateDXRPassCB(gt));

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

GameResult Renderer::BuildFrameResources() {
	for (int i = 0; i < gNumFrameResources; i++) {
		mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(), 1, 32, 32));
		CheckGameResult(mFrameResources.back()->Initialize());
	}

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

GameResult Renderer::BuildRenderItems() {
	auto sphereRitem = std::make_unique<RenderItem>();
	sphereRitem->World = MathHelper::Identity4x4();
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

GameResult Renderer::CompileDXRShaders() {
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

GameResult Renderer::BuildDXRRootSignatures() {
	//
	// Global root signature
	//
	{
		CD3DX12_DESCRIPTOR_RANGE ranges[2];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0); // 1 output texture
		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 1); // 2 static vertex and index buffers

		CD3DX12_ROOT_PARAMETER params[static_cast<UINT>(GlobalRootSignatureParams::Count)];
		params[static_cast<UINT>(GlobalRootSignatureParams::EOutput)].InitAsDescriptorTable(1, &ranges[0]);
		params[static_cast<UINT>(GlobalRootSignatureParams::EAccelerationStructure)].InitAsShaderResourceView(0);
		params[static_cast<UINT>(GlobalRootSignatureParams::EPassCB)].InitAsConstantBufferView(0);
		params[static_cast<UINT>(GlobalRootSignatureParams::EGeometryBuffers)].InitAsDescriptorTable(1, &ranges[1]);

		CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(_countof(params), params);
		CheckGameResult(D3D12Util::CreateRootSignature(md3dDevice.Get(), globalRootSignatureDesc, mGlobalRootSignature.GetAddressOf()));
	}

	//
	// Local root signature
	//
	{
		CD3DX12_ROOT_PARAMETER params[static_cast<UINT>(LocalRootSignatureParams::Count)];
		params[static_cast<UINT>(LocalRootSignatureParams::EObjectCB)].InitAsConstants(D3D12Util::CalcNumUintValues<DXRObjectCB>(), 1);
		
		CD3DX12_ROOT_SIGNATURE_DESC localRootSignatureDesc(_countof(params), params);
		localRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
		CheckGameResult(D3D12Util::CreateRootSignature(md3dDevice.Get(), localRootSignatureDesc, mLocalRootSignature.GetAddressOf()));
	}

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
	md3dDevice->CreateShaderResourceView(geo->VertexBufferGPU.Get(), &vertexSrvDesc, handle);
	
	D3D12_SHADER_RESOURCE_VIEW_DESC indexSrvDesc = {};
	indexSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	indexSrvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	indexSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
	indexSrvDesc.Buffer.StructureByteStride = 0;
	indexSrvDesc.Buffer.FirstElement = 0;
	indexSrvDesc.Buffer.NumElements = static_cast<UINT>(geo->IndexBufferCPU->GetBufferSize() / sizeof(std::uint32_t));
	indexSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	
	handle.ptr += mCbvSrvUavDescriptorSize;
	md3dDevice->CreateShaderResourceView(geo->IndexBufferGPU.Get(), &indexSrvDesc, handle);

	return GameResultOk;
}

GameResult Renderer::BuildBLAS() {
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
	CheckGameResult(D3D12Util::CreateBuffer(md3dDevice.Get(), bufferInfo, mBLAS.Scratch.GetAddressOf(), mInfoQueue.Get()));

	// Create the BLAS buffer
	bufferInfo.Size = prebuildInfo.ResultDataMaxSizeInBytes;
	bufferInfo.State = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
	CheckGameResult(D3D12Util::CreateBuffer(md3dDevice.Get(), bufferInfo, mBLAS.Result.GetAddressOf(), mInfoQueue.Get()));

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

GameResult Renderer::BuildTLAS() {
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
	CheckGameResult(D3D12Util::CreateBuffer(md3dDevice.Get(), instanceBufferInfo, mTLAS.InstanceDesc.GetAddressOf(), mInfoQueue.Get()));

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
	CheckGameResult(D3D12Util::CreateBuffer(md3dDevice.Get(), bufferInfo, mTLAS.Scratch.GetAddressOf(), mInfoQueue.Get()));

	// Create the TLAS buffer
	bufferInfo.Size = prebuildInfo.ResultDataMaxSizeInBytes;
	bufferInfo.State = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
	CheckGameResult(D3D12Util::CreateBuffer(md3dDevice.Get(), bufferInfo, mTLAS.Result.GetAddressOf(), mInfoQueue.Get()));

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
	// Subobjects need to be associated with DXIL exports (i.e. shaders) either by way of default or explicit associations.
	// Default association applies to every exported shader entrypoint that doesn't have any of the same type of subobject associated with it.
	// This simple sample utilizes default shader association except for local root signature subobject
	// which has an explicit association specified purely for demonstration purposes.
	CD3DX12_STATE_OBJECT_DESC dxrPso = { D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };

	auto rayGenLib = dxrPso.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
	auto rayGenShader = mShaderManager.GetRTShader("RayGen");
	D3D12_SHADER_BYTECODE rayGenLibDxil = CD3DX12_SHADER_BYTECODE(rayGenShader->GetBufferPointer(), rayGenShader->GetBufferSize());
	rayGenLib->SetDXILLibrary(&rayGenLibDxil);
	rayGenLib->DefineExport(L"RayGen");

	auto chitLib = dxrPso.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
	auto chitShader = mShaderManager.GetRTShader("ClosestHit");
	D3D12_SHADER_BYTECODE chitLibDxil = CD3DX12_SHADER_BYTECODE(chitShader->GetBufferPointer(), chitShader->GetBufferSize());
	chitLib->SetDXILLibrary(&chitLibDxil);
	chitLib->DefineExport(L"ClosestHit");

	auto missLib = dxrPso.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
	auto missShader = mShaderManager.GetRTShader("Miss");
	D3D12_SHADER_BYTECODE missLibDxil = CD3DX12_SHADER_BYTECODE(missShader->GetBufferPointer(), missShader->GetBufferSize());
	missLib->SetDXILLibrary(&missLibDxil);
	missLib->DefineExport(L"Miss");

	auto higGroup = dxrPso.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
	higGroup->SetClosestHitShaderImport(L"ClosestHit");
	higGroup->SetHitGroupExport(L"HitGroup");
	higGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);

	auto shaderConfig = dxrPso.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
	UINT payloadSize = sizeof(XMFLOAT4);	// for pixel color
	UINT attribSize = sizeof(XMFLOAT2);		// for barycentrics
	shaderConfig->Config(payloadSize, attribSize);
	
	auto localRootSig = dxrPso.CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
	localRootSig->SetRootSignature(mLocalRootSignature.Get());
	{
		auto rootSigAssociation = dxrPso.CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
		rootSigAssociation->SetSubobjectToAssociate(*localRootSig);
		rootSigAssociation->AddExport(L"HitGroup");
	}

	auto glbalRootSig = dxrPso.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
	glbalRootSig->SetRootSignature(mGlobalRootSignature.Get());

	auto pipelineConfig = dxrPso.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
	UINT maxRecursionDepth = 1;
	pipelineConfig->Config(maxRecursionDepth);

	ReturnIfFailedDxDebug(mInfoQueue.Get(), md3dDevice->CreateStateObject(dxrPso, IID_PPV_ARGS(&mDXRPSOs["DXRDefault"])));
	ReturnIfFailedDxDebug(mInfoQueue.Get(), mDXRPSOs["DXRDefault"]->QueryInterface(IID_PPV_ARGS(&mDXRPSOProps["DXRDefaultProps"])));

	return GameResultOk;
}

GameResult Renderer::BuildDXRObjectCB() {
	mDXRObjectCB.Albedo = { 1.0f, 1.0f, 1.0f, 1.0f };

	return GameResultOk;
}

GameResult Renderer::BuildShaderTables() {
	UINT shaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
	void* rayGenShaderIdentifier = mDXRPSOProps["DXRDefaultProps"]->GetShaderIdentifier(L"RayGen");
	void* hitGroupShaderIdentifier = mDXRPSOProps["DXRDefaultProps"]->GetShaderIdentifier(L"HitGroup");
	void* missShaderIdentifier = mDXRPSOProps["DXRDefaultProps"]->GetShaderIdentifier(L"Miss");

	// Ray gen shader table
	ShaderTable rayGenShaderTable(md3dDevice.Get(), 1, shaderIdentifierSize);
	CheckGameResult(rayGenShaderTable.Initialze());
	rayGenShaderTable.push_back(ShaderRecord(rayGenShaderIdentifier, shaderIdentifierSize));
	mRayGenShaderTable = rayGenShaderTable.GetResource();
	
	// Miss shader table
	ShaderTable missShaderTable(md3dDevice.Get(), 1, shaderIdentifierSize);
	CheckGameResult(missShaderTable.Initialze());
	missShaderTable.push_back(ShaderRecord(missShaderIdentifier, shaderIdentifierSize));
	mMissShaderTable = missShaderTable.GetResource();
	
	// Hit group shader table
	ShaderTable hitGroupTable(md3dDevice.Get(), 1, shaderIdentifierSize);
	CheckGameResult(hitGroupTable.Initialze());
	{
		struct RootArguments {
			DXRObjectCB CB;
		} rootArguments;
		rootArguments.CB = mDXRObjectCB;
		hitGroupTable.push_back(ShaderRecord(hitGroupShaderIdentifier, shaderIdentifierSize, &rootArguments, sizeof(rootArguments)));
	}
	mHitGroupShaderTable = hitGroupTable.GetResource();

	return GameResultOk;
}

//GameResult Renderer::BuildGlobalRootSignatures() {
//	// Global Root Signature
//	// This is a root signature that is shared across all raytracing shaders invoked during a DispatchRays() call.
//	CD3DX12_DESCRIPTOR_RANGE ranges_1[1];
//	ranges_1[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
//
//	CD3DX12_ROOT_PARAMETER params[static_cast<unsigned long long>(GlobalRootSignatureParams::Count)];
//	params[static_cast<unsigned long long>(GlobalRootSignatureParams::EOutput)].InitAsDescriptorTable(1, ranges_1);
//	params[static_cast<unsigned long long>(GlobalRootSignatureParams::EAcclerationStructure)].InitAsShaderResourceView(0);
//	params[static_cast<unsigned long long>(GlobalRootSignatureParams::EPassCB)].InitAsConstantBufferView(0);
//
//	CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(_countof(params), params);
//
//	CheckGameResult(D3D12Util::CreateRootSignature(md3dDevice.Get(), globalRootSignatureDesc, mGlobalRootSignature.GetAddressOf()));
//
//	return GameResultOk;
//}
//
//GameResult Renderer::BuildLocalRootSignatures() {
//	// Local Root Signature
//	// This is a root signature that enables a shader to have unique arguments that come from shader tables.
//	CD3DX12_DESCRIPTOR_RANGE ranges_1[1];
//	ranges_1[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 1);
//
//	CD3DX12_DESCRIPTOR_RANGE ranges_2[1];
//	ranges_2[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);
//
//	CD3DX12_ROOT_PARAMETER params[static_cast<unsigned long long>(LocalRootSignatureParams::Count)];
//	params[static_cast<unsigned long long>(LocalRootSignatureParams::EGeometry)].InitAsDescriptorTable(_countof(ranges_1), ranges_1);
//	params[static_cast<unsigned long long>(LocalRootSignatureParams::EMaterialCB)].InitAsDescriptorTable(_countof(ranges_2), ranges_2);
//	
//	CD3DX12_ROOT_SIGNATURE_DESC localRootSignatureDesc(_countof(params), params);
//	localRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
//
//	CheckGameResult(D3D12Util::CreateRootSignature(md3dDevice.Get(), localRootSignatureDesc, mLocalRootSignature.GetAddressOf()));
//
//	return GameResultOk;
//}
//
//GameResult Renderer::BuildDXRPassCB() {
//	D3D12BufferCreateInfo bufferInfo(D3D12Util::CalcConstantBufferByteSize(sizeof(DXRPassConstants)), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
//	CheckGameResult(D3D12Util::CreateBuffer(md3dDevice.Get(), bufferInfo, mDXRPassCB.GetAddressOf(), mInfoQueue.Get()));
//
//	void* pData;
//	ReturnIfFailed(mDXRPassCB->Map(0, nullptr, &pData));
//
//	DXRPassConstants passConsts;
//	passConsts.Resolution = XMFLOAT2(static_cast<float>(mClientWidth), static_cast<float>(mClientHeight));
//
//	std::memcpy(pData, &passConsts, sizeof(DXRPassConstants));
//
//	mDXRPassCB->Unmap(0, nullptr);
//
//	return GameResultOk;
//}
//
//GameResult Renderer::BuildDXRMaterialCB() {
//	D3D12BufferCreateInfo bufferInfo(D3D12Util::CalcConstantBufferByteSize(sizeof(MaterialConstants)), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
//	CheckGameResult(D3D12Util::CreateBuffer(md3dDevice.Get(), bufferInfo, mDXRMaterialCB.GetAddressOf(), mInfoQueue.Get()));
//
//	void* pData;
//	ReturnIfFailed(mDXRMaterialCB->Map(0, nullptr, &pData));
//
//	auto mat = mMaterials["default"].get();
//
//	XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);
//
//	MaterialConstants matConsts;
//	matConsts.DiffuseAlbedo = mat->DiffuseAlbedo;
//	matConsts.FresnelR0 = mat->FresnelR0;
//	matConsts.Roughness = mat->Roughness;
//	XMStoreFloat4x4(&matConsts.MatTransform, XMMatrixTranspose(matTransform));
//
//	std::memcpy(pData, &matConsts, sizeof(MaterialConstants));
//
//	mDXRMaterialCB->Unmap(0, nullptr);
//
//	return GameResultOk;
//}
//
//GameResult Renderer::BuildDXRDescriptorHeaps() {
//	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
//	// 1 UAV for the RT output
//	// 2 SRV for vertex and index buffers
//	// 1 CBV for the material CB
//	desc.NumDescriptors = 4;
//	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
//	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
//	desc.NodeMask = 0;
//
//	ReturnIfFailed(md3dDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&mDXRDescriptorHeap)));
//
//	return GameResultOk;
//}
//
//GameResult Renderer::BuildDXRDescriptors() {
//	auto handle = mDXRDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
//
//	// Create the DXR output buffer UAV
//	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
//	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
//
//	md3dDevice->CreateUnorderedAccessView(mDXROutput.Get(), nullptr, &uavDesc, handle);
//
//	// Create the vertex buffer SRV
//	auto geo = mGeometries["sphereGeo"].get();
//
//	D3D12_SHADER_RESOURCE_VIEW_DESC vertexSrvDesc = {};
//	vertexSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
//	vertexSrvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
//	vertexSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
//	vertexSrvDesc.Buffer.StructureByteStride = 0;
//	vertexSrvDesc.Buffer.FirstElement = 0;
//	vertexSrvDesc.Buffer.NumElements = static_cast<UINT>(geo->VertexBufferCPU->GetBufferSize() / sizeof(float));
//	vertexSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
//
//	handle.ptr += mCbvSrvUavDescriptorSize;
//	md3dDevice->CreateShaderResourceView(mVertexBufferSrv["sphereGeo"].Get(), &vertexSrvDesc, handle);
//
//	D3D12_SHADER_RESOURCE_VIEW_DESC indexSrvDesc = {};
//	indexSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
//	indexSrvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
//	indexSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
//	indexSrvDesc.Buffer.StructureByteStride = 0;
//	indexSrvDesc.Buffer.FirstElement = 0;
//	indexSrvDesc.Buffer.NumElements = static_cast<UINT>(geo->IndexBufferCPU->GetBufferSize() / sizeof(std::uint32_t));
//	indexSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
//
//	handle.ptr += mCbvSrvUavDescriptorSize;
//	md3dDevice->CreateShaderResourceView(mIndexBufferSrv["sphereGeo"].Get(), &indexSrvDesc, handle);
//
//	D3D12_CONSTANT_BUFFER_VIEW_DESC matCbvDesc = {};
//	matCbvDesc.BufferLocation = mDXRMaterialCB->GetGPUVirtualAddress();
//	matCbvDesc.SizeInBytes = D3D12Util::CalcConstantBufferByteSize(static_cast<UINT>(sizeof(MaterialConstants)));
//
//	handle.ptr += mCbvSrvUavDescriptorSize;
//	md3dDevice->CreateConstantBufferView(&matCbvDesc, handle);
//
//	return GameResultOk;
//}
//
//GameResult Renderer::BuildBLASs() {
//	auto geo = mGeometries["sphereGeo"].get();
//
//	D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
//	geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
//	geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
//	geometryDesc.Triangles.VertexCount = static_cast<UINT>(geo->VertexBufferCPU->GetBufferSize() / sizeof(Vertex));
//	geometryDesc.Triangles.VertexBuffer.StartAddress = geo->VertexBufferGPU->GetGPUVirtualAddress();
//	geometryDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);
//	geometryDesc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
//	geometryDesc.Triangles.IndexCount = static_cast<UINT>(geo->IndexBufferCPU->GetBufferSize() / sizeof(std::uint32_t));
//	geometryDesc.Triangles.IndexBuffer = geo->IndexBufferGPU->GetGPUVirtualAddress();
//	geometryDesc.Triangles.Transform3x4 = 0;
//	// Mark the geometry as opaque. 
//	// PERFORMANCE TIP: mark geometry as opaque whenever applicable as it can enable important ray processing optimizations.
//	// Note: When rays encounter opaque geometry an any hit shader will not be executed whether it is present or not.
//	geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
//
//	// Get the size requirements for the BLAS buffers
//	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
//	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
//	inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
//	inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
//	inputs.pGeometryDescs = &geometryDesc;
//	inputs.NumDescs = 1;
//	inputs.Flags = buildFlags;
//
//	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo = {};
//	md3dDevice->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuildInfo);
//
//	prebuildInfo.ScratchDataSizeInBytes = Align(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, prebuildInfo.ScratchDataSizeInBytes);
//	prebuildInfo.ResultDataMaxSizeInBytes = Align(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, prebuildInfo.ResultDataMaxSizeInBytes);
//
//	// Create the BLAS scratch buffer
//	D3D12BufferCreateInfo bufferInfo(prebuildInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
//	bufferInfo.Alignment = std::max(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
//	CheckGameResult(D3D12Util::CreateBuffer(md3dDevice.Get(), bufferInfo, mBLAS.Scratch.GetAddressOf(), mInfoQueue.Get()));
//
//	// Create the BLAS buffer
//	bufferInfo.Size = prebuildInfo.ResultDataMaxSizeInBytes;
//	bufferInfo.State = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
//	CheckGameResult(D3D12Util::CreateBuffer(md3dDevice.Get(), bufferInfo, mBLAS.Result.GetAddressOf(), mInfoQueue.Get()));
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
//GameResult Renderer::BuildTLASs() {
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
//	CheckGameResult(D3D12Util::CreateBuffer(md3dDevice.Get(), instanceBufferInfo, mTLAS.InstanceDesc.GetAddressOf(), mInfoQueue.Get()));
//
//	// Copy the instance data to the buffer
//	void* pData;
//	ReturnIfFailed(mTLAS.InstanceDesc->Map(0, nullptr, &pData));
//	std::memcpy(pData, &instanceDesc, sizeof(instanceDesc));
//	mTLAS.InstanceDesc->Unmap(0, nullptr);
//
//	// Get the size requirements for the TLAS buffers
//	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
//	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
//	inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
//	inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
//	inputs.InstanceDescs = mTLAS.InstanceDesc->GetGPUVirtualAddress();
//	inputs.NumDescs = 1;
//	inputs.Flags = buildFlags;
//
//	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo = {};
//	md3dDevice->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuildInfo);
//
//	prebuildInfo.ResultDataMaxSizeInBytes = Align(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, prebuildInfo.ResultDataMaxSizeInBytes);
//	prebuildInfo.ScratchDataSizeInBytes = Align(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, prebuildInfo.ScratchDataSizeInBytes);
//
//	// Set TLAS size
//	mTLAS.ResultDataMaxSizeInBytes = prebuildInfo.ResultDataMaxSizeInBytes;
//
//	// Create TLAS sratch buffer
//	D3D12BufferCreateInfo bufferInfo(prebuildInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
//	bufferInfo.Alignment = std::max(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
//	CheckGameResult(D3D12Util::CreateBuffer(md3dDevice.Get(), bufferInfo, mTLAS.Scratch.GetAddressOf(), mInfoQueue.Get()));
//
//	// Create the TLAS buffer
//	bufferInfo.Size = prebuildInfo.ResultDataMaxSizeInBytes;
//	bufferInfo.State = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
//	CheckGameResult(D3D12Util::CreateBuffer(md3dDevice.Get(), bufferInfo, mTLAS.Result.GetAddressOf(), mInfoQueue.Get()));
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
//GameResult Renderer::BuildDXRPSOs() {
//	CD3DX12_STATE_OBJECT_DESC raytracingPipeline = { D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };
//
//	// DXIL library
//	// This contains the shaders and their entrypoints for the state object.
//	// Since shaders are not considered a subobject, they need to be passed in via DXIL library subobjects.
//	auto rayGenLib = raytracingPipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
//	auto rayGenShader = mShaderManager.GetRTShader("RayGen");
//	D3D12_SHADER_BYTECODE rayGenLibDxil = CD3DX12_SHADER_BYTECODE(rayGenShader->GetBufferPointer(), rayGenShader->GetBufferSize());
//	rayGenLib->SetDXILLibrary(&rayGenLibDxil);
//	rayGenLib->DefineExport(L"RayGen_12", L"RayGen");
//
//	auto missLib = raytracingPipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
//	auto missShader = mShaderManager.GetRTShader("Miss");
//	D3D12_SHADER_BYTECODE missLibDxil = CD3DX12_SHADER_BYTECODE(missShader->GetBufferPointer(), missShader->GetBufferSize());
//	missLib->SetDXILLibrary(&missLibDxil);
//	missLib->DefineExport(L"Miss_34", L"Miss");
//
//	auto chitLib = raytracingPipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
//	auto chitShader = mShaderManager.GetRTShader("ClosestHit");
//	D3D12_SHADER_BYTECODE chitLibDxil = CD3DX12_SHADER_BYTECODE(chitShader->GetBufferPointer(), chitShader->GetBufferSize());
//	chitLib->SetDXILLibrary(&chitLibDxil);
//	chitLib->DefineExport(L"ClosestHit_56", L"ClosestHit");
//
//	// Triangle hit group
//	// A hit group specifies closest hit, any hit and intersection shaders to be executed when a ray intersects the geometry's triangle/AABB.
//	auto hitGroup = raytracingPipeline.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
//	hitGroup->SetClosestHitShaderImport(L"ClosestHit_56");
//	hitGroup->SetHitGroupExport(L"HitGroup");
//	hitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);
//
//	// Shader config
//	// Defines the maximum sizes in bytes for the ray payload and attribute structure.
//	auto shaderConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
//	UINT payloadSize = 4 * sizeof(float);	// float4 color
//	UINT attributeSize = 2 * sizeof(float);	// float2 barycentrics
//	shaderConfig->Config(payloadSize, attributeSize);
//
//	// Local root signature and shader association
//	// This is a root signature that enables a shader to have unique arguments that come from shader tables.
//	auto localRootSignature = raytracingPipeline.CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
//	localRootSignature->SetRootSignature(mLocalRootSignature.Get());
//
//	// Shader association
//	// Create a list of the shader export names that use the root signature
//	const WCHAR* localRootSignatureExports[] = { L"RayGen_12", L"HitGroup", L"Miss_34" };
//	auto localRootSignatureAssociation = raytracingPipeline.CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
//	localRootSignatureAssociation->SetSubobjectToAssociate(*localRootSignature);
//	localRootSignatureAssociation->AddExports(localRootSignatureExports);
//
//	// Global root signature
//	// This is a root signature that is shared across all raytracing shaders invoked during a DispatchRays() call.
//	auto globalRootSignature = raytracingPipeline.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
//	globalRootSignature->SetRootSignature(mGlobalRootSignature.Get());
//
//	// Pipeline config
//	// Defines the maximum TraceRay() recursion depth.
//	auto pipelineConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
//	// PERFOMANCE TIP: Set max recursion depth as low as needed 
//	// as drivers may apply optimization strategies for low recursion depths. 
//	UINT maxRecursionDepth = 1;	// Primary rays only
//	pipelineConfig->Config(maxRecursionDepth);
//
//	// Create the state object
//	ReturnIfFailedDxDebug(mInfoQueue, md3dDevice->CreateStateObject(raytracingPipeline, IID_PPV_ARGS(&mDXRPSOs["DXRDefault"])));
//
//	// Get the state object properties
//	ReturnIfFailedDxDebug(mInfoQueue, mDXRPSOs["DXRDefault"]->QueryInterface(IID_PPV_ARGS(&mDXRPSOProps["DXRDefaultProps"])));
//
//	return GameResultOk;
//}
//
//GameResult Renderer::BuildShaderTables() {
//	/*
//		The Shader Table layout is as follows:
//			Entry 0 - Ray Generation shader
//			Entry 1 - Miss shader
//			Entry 2 - Closest Hit shader
//		All shader records in the Shader Table must have the same size, so shader record size will be based on the largest required entry.
//		The ray generation program requires the largest entry:
//			32 bytes - D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES
//			+  8 bytes - a CBV/SRV/UAV descriptor table pointer (64-bits)
//			+  8 bytes - a CBV/SRV/UAV descriptor table pointer (64-bits)
//			= 48 bytes ->> aligns to 64 bytes
//		The entry size must be aligned up to D3D12_RAYTRACING_SHADER_BINDING_TABLE_RECORD_BYTE_ALIGNMENT
//	*/
//	std::uint32_t shaderIdSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
//	std::uint32_t shaderTableSize = 0;
//
//	auto& shaderTable = mShaderTables["DXRDefault"];
//	shaderTable.ShaderRecordSize = shaderIdSize;
//	shaderTable.ShaderRecordSize += 8;
//	shaderTable.ShaderRecordSize += 8;
//	shaderTable.ShaderRecordSize = Align(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT, shaderTable.ShaderRecordSize);
//
//	shaderTableSize = (shaderTable.ShaderRecordSize * 3);	// 3 shader records in the table
//	shaderTableSize = Align(D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT, shaderTableSize);
//	
//	// Create the shader table buffer
//	D3D12BufferCreateInfo bufferInfo(shaderTableSize, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
//	CheckGameResult(D3D12Util::CreateBuffer(md3dDevice.Get(), bufferInfo, shaderTable.Resource.GetAddressOf(), mInfoQueue.Get()));
//
//	// Map the buffer
//	std::uint8_t* pData;
//	ReturnIfFailed(shaderTable.Resource->Map(0, nullptr, reinterpret_cast<void**>(&pData)));
//
//	auto props = mDXRPSOProps["DXRDefaultProps"].Get();
//	
//	// Shader Record 0 -  Ray generation program and local root parameter data
//	std::memcpy(pData, props->GetShaderIdentifier(L"RayGen_12"), shaderIdSize);
//	*reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(pData + shaderIdSize) = D3D12Util::GetGpuHandle(
//		mDXRDescriptorHeap.Get(),
//		static_cast<INT>(DXRDescriptorHeapIndex::EGeometry),
//		mCbvSrvUavDescriptorSize
//	);
//	*reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(pData + shaderIdSize + 8) = D3D12Util::GetGpuHandle(
//		mDXRDescriptorHeap.Get(),
//		static_cast<INT>(DXRDescriptorHeapIndex::EMaterial),
//		mCbvSrvUavDescriptorSize
//	);
//	
//	// Shader Record 1 - Miss program (no local root arguments to set)
//	pData += shaderTable.ShaderRecordSize;
//	std::memcpy(pData, props->GetShaderIdentifier(L"Miss_34"), shaderIdSize);
//	//*reinterpret_cast<D3D12_GPU_VIRTUAL_ADDRESS*>(pData + shaderIdSize) = mDXRMaterialCB->GetGPUVirtualAddress();
//	
//	// Shader Record 2 - Closest Hit program and local root parameter data (descriptor table with constant buffer and IB/VB pointers)
//	pData += shaderTable.ShaderRecordSize;
//	std::memcpy(pData, props->GetShaderIdentifier(L"HitGroup"), shaderIdSize);
//	*reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(pData + shaderIdSize) = D3D12Util::GetGpuHandle(
//		mDXRDescriptorHeap.Get(),
//		static_cast<INT>(DXRDescriptorHeapIndex::EGeometry),
//		mCbvSrvUavDescriptorSize
//	);
//	*reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(pData + shaderIdSize + 8) = D3D12Util::GetGpuHandle(
//		mDXRDescriptorHeap.Get(),
//		static_cast<INT>(DXRDescriptorHeapIndex::EMaterial),
//		mCbvSrvUavDescriptorSize
//	);
//
//	// Unmap
//	shaderTable.Resource->Unmap(0, nullptr);
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
	mMainPassCB.Lights[1].Strength = { 0.0f, 0.0f, 0.0f };
	mMainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	mMainPassCB.Lights[2].Strength = { 0.0f, 0.0f, 0.0f };

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

GameResult Renderer::UpdateDXRPassCB(const GameTimer& gt) {
	XMMATRIX view = XMLoadFloat4x4(&mView);
	XMMATRIX proj = XMLoadFloat4x4(&mProj);
	XMMATRIX viewProj = XMMatrixMultiply(view, proj);

	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMStoreFloat4x4(&mDXRPassCB.ProjectionToWorld, XMMatrixTranspose(invViewProj));
	mDXRPassCB.EyePosW = { mEyePos.x, mEyePos.y, mEyePos.z, 1.0f };
	mDXRPassCB.LightPosW = { -100.0f, 100.0f, 100.0f, 0.0f };
	mDXRPassCB.LightAmbientColor = { 0.3f, 0.3f, 0.42f, 1.0f };
	mDXRPassCB.LightDiffuseColor = { 0.6f, 0.6f, 0.6f, 1.0f };

	auto& currDXRPassCB = mCurrFrameResource->DXRPassCB;
	currDXRPassCB.CopyData(0, mDXRPassCB);

	return GameResultOk;
}

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

	mCommandList->SetComputeRootSignature(mGlobalRootSignature.Get());
	
	auto& dxrPassCB = mCurrFrameResource->DXRPassCB;
	mCommandList->SetComputeRootConstantBufferView(static_cast<UINT>(GlobalRootSignatureParams::EPassCB), dxrPassCB.Resource()->GetGPUVirtualAddress());
	
	ID3D12DescriptorHeap* descriptorHeaps[] = { mDXRDescriptorHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	
	mCommandList->SetComputeRootDescriptorTable(
		static_cast<UINT>(GlobalRootSignatureParams::EOutput),
		D3D12Util::GetGpuHandle(mDXRDescriptorHeap.Get(), static_cast<INT>(DXRDescriptors::EOutputUAV), mCbvSrvUavDescriptorSize)
	);
	
	mCommandList->SetComputeRootDescriptorTable(
		static_cast<UINT>(GlobalRootSignatureParams::EGeometryBuffers),
		D3D12Util::GetGpuHandle(mDXRDescriptorHeap.Get(), static_cast<INT>(DXRDescriptors::EGeomtrySRVs), mCbvSrvUavDescriptorSize)
	);
	
	mCommandList->SetComputeRootShaderResourceView(static_cast<UINT>(GlobalRootSignatureParams::EAccelerationStructure), mTLAS.Result->GetGPUVirtualAddress());
	
	D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
	dispatchDesc.HitGroupTable.StartAddress = mHitGroupShaderTable->GetGPUVirtualAddress();
	dispatchDesc.HitGroupTable.SizeInBytes = mHitGroupShaderTable->GetDesc().Width;
	dispatchDesc.HitGroupTable.StrideInBytes = dispatchDesc.HitGroupTable.SizeInBytes;
	dispatchDesc.MissShaderTable.StartAddress = mMissShaderTable->GetGPUVirtualAddress();
	dispatchDesc.MissShaderTable.SizeInBytes = mMissShaderTable->GetDesc().Width;
	dispatchDesc.MissShaderTable.StrideInBytes = dispatchDesc.MissShaderTable.SizeInBytes;
	dispatchDesc.RayGenerationShaderRecord.StartAddress = mRayGenShaderTable->GetGPUVirtualAddress();
	dispatchDesc.RayGenerationShaderRecord.SizeInBytes = mRayGenShaderTable->GetDesc().Width;
	dispatchDesc.Width = mClientWidth;
	dispatchDesc.Height = mClientHeight;
	dispatchDesc.Depth = 1;
	
	mCommandList->SetPipelineState1(mDXRPSOs["DXRDefault"].Get());
	mCommandList->DispatchRays(&dispatchDesc);

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