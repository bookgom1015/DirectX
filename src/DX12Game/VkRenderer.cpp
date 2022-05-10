#include "DX12Game/VkRenderer.h"

VkVertexInputBindingDescription VkRenderer::Vertex::GetBindingDescription() {
	VkVertexInputBindingDescription bindingDescription = {};
	bindingDescription.binding = 0;
	bindingDescription.stride = sizeof(Vertex);
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	return bindingDescription;
}

std::array<VkVertexInputAttributeDescription, 2> VkRenderer::Vertex::GetAttributeDescriptions() {
	std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions = {};
	attributeDescriptions[0].binding = 0;
	attributeDescriptions[0].location = 0;
	attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
	attributeDescriptions[0].offset = offsetof(Vertex, mPos);
	attributeDescriptions[1].binding = 0;
	attributeDescriptions[1].location = 1;
	attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[1].offset = offsetof(Vertex, mColor);
	return attributeDescriptions;
}

VkRenderer::~VkRenderer() {
	if (!bIsCleaned)
		CleanUp();
}

GameResult VkRenderer::Initialize(
		UINT inClientWidth,
		UINT inClientHeight,
		UINT inNumThreads,
		HWND hMainWnd,
		GLFWwindow* inMainWnd,
		CVBarrier* inCV,
		SpinlockBarrier* inSpinlock) {
	CheckGameResult(VkLowRenderer::Initialize(inClientWidth, inClientHeight, inNumThreads, inMainWnd));

	CheckGameResult(CreateGraphicsPipeline());
	CheckGameResult(CreateVertexBuffer());
	CheckGameResult(CreateIndexBuffer());
	CheckGameResult(CreateCommandBuffers());
	CheckGameResult(CreateSyncObjects());

	return GameResultOk;
}

void VkRenderer::CleanUp() {
	vkDeviceWaitIdle(mDevice);

	for (size_t i = 0; i < mSwapChainImageCount; ++i) {
		vkDestroyFence(mDevice, mInFlightFences[i], nullptr);
		vkDestroySemaphore(mDevice, mRenderFinishedSemaphores[i], nullptr);
		vkDestroySemaphore(mDevice, mImageAvailableSemaphores[i], nullptr);
	}

	vkDestroyBuffer(mDevice, mIndexBuffer, nullptr);
	vkFreeMemory(mDevice, mIndexBufferMemory, nullptr);

	vkDestroyBuffer(mDevice, mVertexBuffer, nullptr);
	vkFreeMemory(mDevice, mVertexBufferMemory, nullptr);

	VkLowRenderer::CleanUp();

	bIsCleaned = true;
}

GameResult VkRenderer::Update(const GameTimer& gt, UINT inTid) {


	return GameResultOk;
}

GameResult VkRenderer::Draw(const GameTimer& gt, UINT inTid) {
	vkWaitForFences(mDevice, 1, &mInFlightFences[mCurrentFrame], VK_TRUE, UINT64_MAX);

	std::uint32_t imageIndex;
	vkAcquireNextImageKHR(
		mDevice,
		mSwapChain, 
		UINT64_MAX,
		mImageAvailableSemaphores[mCurrentFrame],
		VK_NULL_HANDLE,
		&imageIndex
	);

	if (mImagesInFlight[imageIndex] != VK_NULL_HANDLE)
		vkWaitForFences(mDevice, 1, &mImagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);

	mImagesInFlight[imageIndex] = mInFlightFences[mCurrentFrame];

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	
	VkSemaphore waitSemaphores[] = {
		mImageAvailableSemaphores[mCurrentFrame]
	};
	VkPipelineStageFlags waitStages[] = {
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
	};
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;

	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &mCommandBuffers[imageIndex];

	VkSemaphore signalSemaphores[] = {
		mRenderFinishedSemaphores[mCurrentFrame]
	};
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	vkResetFences(mDevice, 1, &mInFlightFences[mCurrentFrame]);

	if (vkQueueSubmit(mGraphicsQueue, 1, &submitInfo, mInFlightFences[imageIndex]) != VK_SUCCESS)
		ReturnGameResult(S_FALSE, L"Failed to submit draw command buffer"); 

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;

	VkSwapchainKHR swapChains[] = { mSwapChain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.pResults = nullptr;

	vkQueuePresentKHR(mPresentQueue, &presentInfo);

	mCurrentFrame = (mCurrentFrame + 1) % mSwapChainImageCount;

	return GameResultOk;
}

GameResult VkRenderer::OnResize(UINT inClientWidth, UINT inClientHeight) {
	CheckGameResult(VkLowRenderer::OnResize(inClientWidth, inClientHeight));

	return GameResultOk;
}

GameResult VkRenderer::CreateGraphicsPipeline() {
	VkShaderModule vertShaderModule;
	VkShaderModule fragShaderModule;

	{
		std::vector<char> vertShaderCode;
		CheckGameResult(ReadFile("./../../../../Assets/Shaders/vert.spv", vertShaderCode));
		std::vector<char> fragShaderCode;
		CheckGameResult(ReadFile("./../../../../Assets/Shaders/frag.spv", fragShaderCode));

		CheckGameResult(CreateShaderModule(vertShaderCode, vertShaderModule));
		CheckGameResult(CreateShaderModule(fragShaderCode, fragShaderModule));
	}

	VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vertShaderModule;
	vertShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = fragShaderModule;
	fragShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStages[] = {
		vertShaderStageInfo, fragShaderStageInfo
	};

	auto bindingDescription = Vertex::GetBindingDescription();
	auto attributeDescriptioins = Vertex::GetAttributeDescriptions();
	VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributeDescriptioins.size());
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptioins.data();

	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(mSwapChainExtent.width);
	viewport.height = static_cast<float>(mSwapChainExtent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = mSwapChainExtent;

	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizer = {};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f;
	rasterizer.depthBiasClamp = 0.0f;
	rasterizer.depthBiasSlopeFactor = 0.0f;

	VkPipelineMultisampleStateCreateInfo multisampling = {};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 1.0f;
	multisampling.pSampleMask = nullptr;
	multisampling.alphaToCoverageEnable = VK_FALSE;
	multisampling.alphaToOneEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT |
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_TRUE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo colorBlending = {};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f;
	colorBlending.blendConstants[1] = 0.0f;
	colorBlending.blendConstants[2] = 0.0f;
	colorBlending.blendConstants[3] = 0.0f;

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 0;
	pipelineLayoutInfo.pSetLayouts = nullptr;
	pipelineLayoutInfo.pushConstantRangeCount = 0;
	pipelineLayoutInfo.pPushConstantRanges = nullptr;

	if (vkCreatePipelineLayout(mDevice, &pipelineLayoutInfo, nullptr, &mPipelineLayout) != VK_SUCCESS)
		ReturnGameResult(S_FALSE, L"Failed to create pipeline layout");

	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = nullptr;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = nullptr;
	pipelineInfo.layout = mPipelineLayout;
	pipelineInfo.renderPass = mRenderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = -1;

	if (vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &mGraphicsPipeline) != VK_SUCCESS)
		ReturnGameResult(S_FALSE, L"Failed to create graphics pipeline");

	vkDestroyShaderModule(mDevice, fragShaderModule, nullptr);
	vkDestroyShaderModule(mDevice, vertShaderModule, nullptr);

	return GameResultOk;
}

namespace {
	GameResult FindMemoryType(VkPhysicalDevice inPhysicalDevice, std::uint32_t inTypeFilter, VkMemoryPropertyFlags inProperties, std::uint32_t& outIndex) {
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(inPhysicalDevice, &memProperties);

		for (std::uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
			if ((inTypeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & inProperties) == inProperties) {
				outIndex = i;
				return GameResultOk;
			}
		}

		ReturnGameResult(S_FALSE, L"Failed to find suitable memory type");
	}

	GameResult CreateBuffer(
			VkPhysicalDevice inPhysicalDevice, 
			VkDevice inDevice, 
			VkDeviceSize inSize, 
			VkBufferUsageFlags inUsage, 
			VkMemoryPropertyFlags inProperties, 
			VkBuffer& outBuffer,
			VkDeviceMemory& outBufferMemory) {
		VkBufferCreateInfo bufferInfo = {};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = inSize;
		bufferInfo.usage = inUsage;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		bufferInfo.flags = 0;

		if (vkCreateBuffer(inDevice, &bufferInfo, nullptr, &outBuffer) != VK_SUCCESS)
			ReturnGameResult(S_FALSE, L"Failed to create vertex buffer");

		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(inDevice, outBuffer, &memRequirements);

		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		CheckGameResult(FindMemoryType(
			inPhysicalDevice,
			memRequirements.memoryTypeBits,
			inProperties,
			allocInfo.memoryTypeIndex));

		if (vkAllocateMemory(inDevice, &allocInfo, nullptr, &outBufferMemory) != VK_SUCCESS)
			ReturnGameResult(S_FALSE, L"Failed to allocate vertex buffer memory");

		vkBindBufferMemory(inDevice, outBuffer, outBufferMemory, 0);

		return GameResultOk;
	}

	void CopyBuffer(
			VkDevice inDevice, 
			VkQueue mQueue,
			VkCommandPool inCommandPool, 
			VkBuffer inSrcBuffer, 
			VkBuffer inDstBuffer, 
			VkDeviceSize inSize) {
		VkCommandBufferAllocateInfo allocInfo = {};		
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = inCommandPool;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer;
		vkAllocateCommandBuffers(inDevice, &allocInfo, &commandBuffer);

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(commandBuffer, &beginInfo);

		VkBufferCopy copyRegion = {};
		copyRegion.srcOffset = 0;
		copyRegion.dstOffset = 0;
		copyRegion.size = inSize;
		vkCmdCopyBuffer(commandBuffer, inSrcBuffer, inDstBuffer, 1, &copyRegion);

		vkEndCommandBuffer(commandBuffer);

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		vkQueueSubmit(mQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(mQueue);

		vkFreeCommandBuffers(inDevice, inCommandPool, 1, &commandBuffer);
	}
}

GameResult VkRenderer::CreateVertexBuffer() {
	VkDeviceSize bufferSize = sizeof(mVertices[0]) * mVertices.size();

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	CheckGameResult(CreateBuffer(
		mPhysicalDevice,
		mDevice,
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer,
		stagingBufferMemory));

	void* data;
	vkMapMemory(mDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
	std::memcpy(data, mVertices.data(), static_cast<size_t>(bufferSize));
	vkUnmapMemory(mDevice, stagingBufferMemory);

	CheckGameResult(CreateBuffer(
		mPhysicalDevice,
		mDevice,
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		mVertexBuffer,
		mVertexBufferMemory));

	CopyBuffer(mDevice, mGraphicsQueue, mCommandPool, stagingBuffer, mVertexBuffer, bufferSize);

	vkDestroyBuffer(mDevice, stagingBuffer, nullptr);
	vkFreeMemory(mDevice, stagingBufferMemory, nullptr);

	return GameResultOk;
}

GameResult VkRenderer::CreateIndexBuffer() {
	VkDeviceSize bufferSize = sizeof(mIndices[0]) * mIndices.size();

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	CheckGameResult(CreateBuffer(
		mPhysicalDevice,
		mDevice,
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer,
		stagingBufferMemory));

	void* data;
	vkMapMemory(mDevice, stagingBufferMemory, 0, bufferSize, 0, &data);
	std::memcpy(data, mIndices.data(), bufferSize);
	vkUnmapMemory(mDevice, stagingBufferMemory);

	CheckGameResult(CreateBuffer(
		mPhysicalDevice,
		mDevice,
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		mIndexBuffer,
		mIndexBufferMemory));

	CopyBuffer(mDevice, mGraphicsQueue, mCommandPool, stagingBuffer, mIndexBuffer, bufferSize);

	vkDestroyBuffer(mDevice, stagingBuffer, nullptr);
	vkFreeMemory(mDevice, stagingBufferMemory, nullptr);

	return GameResultOk;
}

GameResult VkRenderer::CreateCommandBuffers() {
	mCommandBuffers.resize(mSwapChainFramebuffers.size());

	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = mCommandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = static_cast<std::uint32_t>(mCommandBuffers.size());

	if (vkAllocateCommandBuffers(mDevice, &allocInfo, mCommandBuffers.data()) != VK_SUCCESS)
		ReturnGameResult(S_FALSE, L"Failed to create command buffers");

	for (size_t i = 0, end = mCommandBuffers.size(); i < end; ++i) {
		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = 0;
		beginInfo.pInheritanceInfo = nullptr;

		if (vkBeginCommandBuffer(mCommandBuffers[i], &beginInfo) != VK_SUCCESS)
			ReturnGameResult(S_FALSE, L"Failed to begin recording command buffer");

		VkRenderPassBeginInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = mRenderPass;
		renderPassInfo.framebuffer = mSwapChainFramebuffers[i];
		renderPassInfo.renderArea.offset = { 0,0 };
		renderPassInfo.renderArea.extent = mSwapChainExtent;

		VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
		renderPassInfo.clearValueCount = 1;
		renderPassInfo.pClearValues = &clearColor;

		vkCmdBeginRenderPass(mCommandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(mCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, mGraphicsPipeline);

		VkBuffer vertexBuffers[] = { mVertexBuffer };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(mCommandBuffers[i], 0, 1, vertexBuffers, offsets);

		vkCmdBindIndexBuffer(mCommandBuffers[i], mIndexBuffer, 0, VK_INDEX_TYPE_UINT16);

		//vkCmdDraw(mCommandBuffers[i], static_cast<std::uint32_t>(mVertices.size()), 1, 0, 0);
		vkCmdDrawIndexed(mCommandBuffers[i], static_cast<std::uint32_t>(mIndices.size()), 1, 0, 0, 0);

		vkCmdEndRenderPass(mCommandBuffers[i]);

		if (vkEndCommandBuffer(mCommandBuffers[i]) != VK_SUCCESS)
			ReturnGameResult(S_FALSE, L"Failed to record command buffer");
	}

	return GameResultOk;
}

GameResult VkRenderer::CreateSyncObjects() {
	mImageAvailableSemaphores.resize(mSwapChainImageCount);
	mRenderFinishedSemaphores.resize(mSwapChainImageCount);
	mInFlightFences.resize(mSwapChainImageCount);
	mImagesInFlight.resize(mSwapChainImages.size(), VK_NULL_HANDLE);

	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (size_t i = 0; i < mSwapChainImageCount; ++i) {
		if (vkCreateSemaphore(mDevice, &semaphoreInfo, nullptr, &mImageAvailableSemaphores[i]) != VK_SUCCESS ||
			vkCreateSemaphore(mDevice, &semaphoreInfo, nullptr, &mRenderFinishedSemaphores[i]) != VK_SUCCESS ||
			vkCreateFence(mDevice, &fenceInfo, nullptr, &mInFlightFences[i]) != VK_SUCCESS)
			ReturnGameResult(S_FALSE, L"Failed to create synchronization object(s) for a frame");
	}

	return GameResultOk;
}

void VkRenderer::UpdateWorldTransform(const std::string& inRenderItemName,
	const DirectX::XMMATRIX& inTransform, bool inIsSkeletal) {

}

void VkRenderer::UpdateInstanceAnimationData(const std::string& inRenderItemName,
	UINT inAnimClipIdx, float inTimePos, bool inIsSkeletal) {

}

void VkRenderer::SetVisible(const std::string& inRenderItemName, bool inState) {

}

void VkRenderer::SetSkeletonVisible(const std::string& inRenderItemName, bool inState) {

}

GameResult VkRenderer::AddGeometry(const Mesh* inMesh) {


	return GameResultOk;
}

void VkRenderer::AddRenderItem(std::string& ioRenderItemName, const Mesh* inMesh) {

}

GameResult VkRenderer::AddMaterials(const std::unordered_map<std::string, MaterialIn>& inMaterials) {


	return GameResultOk;
}

UINT VkRenderer::AddAnimations(const std::string& inClipName, const Animation& inAnim) {

	return 0;
}

GameResult VkRenderer::UpdateAnimationsMap() {


	return GameResultOk;
}