#pragma once

#include "DX12Game/VkLowRenderer.h"
#include "DX12Game/Renderer.h"

#ifdef UsingVulkan

struct Vertex {
	glm::vec3 mPos;
	glm::vec3 mColor;
	glm::vec2 mTexCoord;

	static VkVertexInputBindingDescription GetBindingDescription();
	static std::array<VkVertexInputAttributeDescription, 3> GetAttributeDescriptions();

	bool operator==(const Vertex& other) const {
		return mPos == other.mPos && mColor == other.mColor && mTexCoord == other.mTexCoord;
	}
};

namespace std {
	template<> struct hash<Vertex> {
		size_t operator()(Vertex const& vertex) const {
			return ((hash<glm::vec3>()(vertex.mPos) ^
				(hash<glm::vec3>()(vertex.mColor) << 1)) >> 1) ^
				(hash<glm::vec2>()(vertex.mTexCoord) << 1);
		}
	};
}

class VkRenderer : public VkLowRenderer, public Renderer {
	struct UniformBufferObject {
		alignas(16) glm::mat4 mModel;
		alignas(16) glm::mat4 mView;
		alignas(16) glm::mat4 mProj;
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
		GLFWwindow* inMainWnd = nullptr,
		CVBarrier* inCV = nullptr,
		SpinlockBarrier* inSpinlock = nullptr) override;

	virtual void CleanUp() override;
	virtual GameResult Update(const GameTimer& gt, UINT inTid = 0) override;
	virtual GameResult Draw(const GameTimer& gt, UINT inTid = 0) override;
	virtual GameResult OnResize(UINT inClientWidth, UINT inClientHeight) override;

	virtual GameResult GetDeviceRemovedReason() const override;

	virtual void UpdateWorldTransform(const std::string& inRenderItemName,
		const DirectX::XMMATRIX& inTransform, bool inIsSkeletal) override;
	virtual void UpdateInstanceAnimationData(const std::string& inRenderItemName,
		UINT inAnimClipIdx, float inTimePos, bool inIsSkeletal) override;

	virtual void SetVisible(const std::string& inRenderItemName, bool inState) override;
	virtual void SetSkeletonVisible(const std::string& inRenderItemName, bool inState) override;

	virtual GameResult AddGeometry(const Mesh* inMesh) override;
	virtual void AddRenderItem(std::string& ioRenderItemName, const Mesh* inMesh) override;
	virtual GameResult AddMaterials(const std::unordered_map<std::string, MaterialIn>& inMaterials) override;

	virtual UINT AddAnimations(const std::string& inClipName, const Game::Animation& inAnim) override;
	virtual GameResult UpdateAnimationsMap() override;

protected:
	GameResult CreateImageViews();
	GameResult CreateRenderPass();
	GameResult CreateFramebuffers();
	GameResult CreateCommandPool();
	GameResult CreateColorResources();
	GameResult CreateDepthResources();
	GameResult CreateTextureImage();
	GameResult CreateTextureImageView();
	GameResult CreateTextureSampler();
	GameResult CreateDescriptorSetLayout();
	GameResult CreateGraphicsPipeline();
	GameResult CreateVertexBuffer();
	GameResult CreateIndexBuffer();
	GameResult CreateUniformBuffers();
	GameResult CreateDescriptorPool();
	GameResult CreateDescriptorSets();
	GameResult CreateCommandBuffers();
	GameResult CreateSyncObjects();

	virtual GameResult RecreateSwapChain() override;
	virtual GameResult CleanUpSwapChain() override;

	GameResult UpdateUniformBuffer(const GameTimer& gt, std::uint32_t inCurrIndex);

	GameResult LoadModel();

private:
	bool bIsCleaned = false;

	std::vector<VkImageView> mSwapChainImageViews;

	VkRenderPass mRenderPass;

	std::vector<VkFramebuffer> mSwapChainFramebuffers;

	VkCommandPool mCommandPool;
	std::vector<VkCommandBuffer> mCommandBuffers;

	VkDescriptorSetLayout mDescriptorSetLayout;
	VkPipelineLayout mPipelineLayout;
	VkPipeline mGraphicsPipeline;

	std::vector<VkSemaphore> mImageAvailableSemaphores;
	std::vector<VkSemaphore> mRenderFinishedSemaphores;
	std::vector<VkFence> mInFlightFences;
	std::vector<VkFence> mImagesInFlight;
	size_t mCurrentFrame = 0;

	const std::vector<Vertex> mQuadVertices = {
		{{ -0.64f,  0.36f + 0.25f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f }},
		{{  0.64f,  0.36f + 0.25f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f }},
		{{  0.64f, -0.36f + 0.25f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f }},
		{{ -0.64f, -0.36f + 0.25f, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f }}
	};

	const std::vector<std::uint16_t> mQuadIndices = { 0, 1, 2, 2, 3, 0 };

	std::unordered_map<Vertex, std::uint32_t> mObjUniqueVertices;
	std::vector<Vertex> mObjVertices;
	std::vector<std::uint32_t> mObjIndices;

	VkBuffer mObjVertexBuffer;
	VkDeviceMemory mObjVertexBufferMemory;

	VkBuffer mObjIndexBuffer;
	VkDeviceMemory mObjIndexBufferMemory;

	std::vector<VkBuffer> mUniformBuffers;
	std::vector<VkDeviceMemory> mUniformBufferMemories;

	VkDescriptorPool mDescriptorPool;
	std::vector<VkDescriptorSet> mDescriptorSets;

	bool bFramebufferResized = false;

	std::uint32_t mMipLevels;
	VkImage mTextureImage;
	VkDeviceMemory mTextureImageMemory;

	VkImageView mTextureImageView;
	VkSampler mTextureSampler;

	VkImage mDepthImage;
	VkDeviceMemory mDepthImageMemory;
	VkImageView mDepthImageView;

	const std::string ModelPath = ModelFilePath + "viking_room.obj";
	const std::string TexturePath = TextureFilePath + "viking_room.png";

	VkImage mColorImage;
	VkDeviceMemory mColorImageMemory;
	VkImageView mColorImageView;
};

#endif // UsingVulkan