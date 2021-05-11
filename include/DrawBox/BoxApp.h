#include "common/d3dApp.h"
#include "common/MathHelper.h"
#include "common/UploadBuffer.h"

//#define Ex2
//#define Ex3A
//#define Ex3B
//#define Ex3C
//#define Ex3D
//#define Ex3E
//#define Ex4
//#define Ex6
//#define Ex7
//#define Ex8
//#define Ex9N
//#define Ex9F
//#define Ex10
//#define Ex11a
//#define Ex11b
//#define Ex12
//#define Ex13
//#define Ex14
//#define Ex15
//#define Ex16

struct ObjectConstants;

struct BoxGeometry {
	Microsoft::WRL::ComPtr<ID3DBlob> PositionBufferCPU = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> ColorBufferCPU = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> IndexBufferCPU = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> PositionBufferGPU = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> ColorBufferGPU = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> PositionBufferUploader = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> ColorBufferUploader = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;

	UINT PosByteStride = 0;
	UINT ColorByteStride = 0;
	UINT PosBufferByteSize = 0;
	UINT ColorBufferByteSize = 0;
	DXGI_FORMAT IndexFormat = DXGI_FORMAT_R16_UINT;
	UINT IndexBufferByteSize = 0;
	UINT IndexCount;
};

class BoxApp : public D3DApp {
public:
	BoxApp(HINSTANCE hInstance);
	BoxApp(const BoxApp& src) = delete;
	BoxApp& operator=(const BoxApp& rhs) = delete;
	BoxApp& operator=(const BoxApp&& rhs) = delete;
	virtual ~BoxApp();

	virtual bool Initialize() override;

private:
	virtual void OnResize() override;
	virtual void Update(const GameTimer& gt) override;
	virtual void Draw(const GameTimer& gt) override;

	virtual void OnMouseDown(WPARAM btnState, int x, int y) override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y) override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y) override;

	void BuildDescriptorHeaps();
	void BuildConstantBuffers();
	void BuildRootSignature();
	void BuildShadersAndInputLayout();
	void BuildBoxGeometry();
	void BuildPSO();

private:
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mCbvHeap = nullptr;

	std::unique_ptr<UploadBuffer<ObjectConstants>> mObjectCB = nullptr;

	std::unique_ptr<MeshGeometry> mBoxGeo = nullptr;

	Microsoft::WRL::ComPtr<ID3DBlob> mvsByteCode = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> mpsByteCode = nullptr;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO = nullptr;

	DirectX::XMFLOAT4X4 mWorld = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 mView = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 mProj = MathHelper::Identity4x4();

	float mTheta = 1.5f * DirectX::XM_PI;
	float mPhi = DirectX::XM_PIDIV4;
	float mRadius = 5.0f;

	POINT mLastMousePos;

	BoxGeometry meBoxGeo;
};