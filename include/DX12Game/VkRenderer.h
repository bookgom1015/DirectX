#pragma once

#include "DX12Game/VkLowRenderer.h"
#include "DX12Game/Renderer.h"

class VkRenderer : public VkLowRenderer, public Renderer {
protected:
	struct Vertex {
		glm::vec2 mPos;
		glm::vec3 mColor;

		static VkVertexInputBindingDescription GetBindingDescription();
		static std::array<VkVertexInputAttributeDescription, 2> GetAttributeDescriptions();
	};

public:
	VkRenderer() = default;
	virtual ~VkRenderer();

private:
	VkRenderer(const VkRenderer& inRef) = delete;
	VkRenderer(VkRenderer&& inRVal) = delete;
	VkRenderer& operator=(const VkRenderer& inRef) = delete;
	VkRenderer& operator=(VkRenderer&& inRVal) = delete;

public:
	virtual GameResult Initialize(
		UINT inClientWidth,
		UINT inClientHeight,
		UINT inNumThreads = 1,
		HWND hMainWnd = NULL,
		GLFWwindow* inMainWnd = nullptr,
		CVBarrier* inCV = nullptr,
		SpinlockBarrier* inSpinlock = nullptr) override;

	virtual void CleanUp() override;
	virtual GameResult Update(const GameTimer& gt, UINT inTid = 0) override;
	virtual GameResult Draw(const GameTimer& gt, UINT inTid = 0) override;
	virtual GameResult OnResize(UINT inClientWidth, UINT inClientHeight) override;

	GameResult CreateGraphicsPipeline();
	GameResult CreateVertexBuffer();
	GameResult CreateIndexBuffer();
	GameResult CreateCommandBuffers();
	GameResult CreateSyncObjects();

	virtual void UpdateWorldTransform(const std::string& inRenderItemName,
		const DirectX::XMMATRIX& inTransform, bool inIsSkeletal) override;
	virtual void UpdateInstanceAnimationData(const std::string& inRenderItemName,
		UINT inAnimClipIdx, float inTimePos, bool inIsSkeletal) override;

	virtual void SetVisible(const std::string& inRenderItemName, bool inState) override;
	virtual void SetSkeletonVisible(const std::string& inRenderItemName, bool inState) override;

	virtual GameResult AddGeometry(const Mesh* inMesh) override;
	virtual void AddRenderItem(std::string& ioRenderItemName, const Mesh* inMesh) override;
	virtual GameResult AddMaterials(const std::unordered_map<std::string, MaterialIn>& inMaterials) override;

	virtual UINT AddAnimations(const std::string& inClipName, const Animation& inAnim) override;
	virtual GameResult UpdateAnimationsMap() override;

private:
	bool bIsCleaned = false;

	std::vector<VkSemaphore> mImageAvailableSemaphores;
	std::vector<VkSemaphore> mRenderFinishedSemaphores;
	std::vector<VkFence> mInFlightFences;
	std::vector<VkFence> mImagesInFlight;
	size_t mCurrentFrame = 0;

	const std::vector<Vertex> mVertices = {
		{{ -0.5f, -0.5f}, { 1.0f, 0.0f, 0.0f }},
		{{  0.5f, -0.5f}, { 0.0f, 1.0f, 0.0f }},
		{{  0.5f,  0.5f}, { 0.0f, 0.0f, 1.0f }},
		{{ -0.5f,  0.5f}, { 1.0f, 1.0f, 1.0f }}
	};

	const std::vector<std::uint16_t> mIndices = { 0, 1, 2, 2, 3, 0 };

	VkBuffer mVertexBuffer;
	VkDeviceMemory mVertexBufferMemory;

	VkBuffer mIndexBuffer;
	VkDeviceMemory mIndexBufferMemory;
};