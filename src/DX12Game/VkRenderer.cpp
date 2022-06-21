#include "DX12Game/VkRenderer.h"

#ifdef UsingVulkan

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

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

		ReturnGameResult(E_NOINTERFACE, L"Failed to find suitable memory type");
	}

	VkCommandBuffer BeginSingleTimeCommands(VkDevice inDevice, VkCommandPool inCommandPool) {
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

		return commandBuffer;
	}

	void EndSingleTimeCommands(VkDevice inDevice, VkQueue inQueue, VkCommandPool inCommandPool, VkCommandBuffer inCommandBuffer) {
		vkEndCommandBuffer(inCommandBuffer);

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &inCommandBuffer;

		vkQueueSubmit(inQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(inQueue);

		vkFreeCommandBuffers(inDevice, inCommandPool, 1, &inCommandBuffer);
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
			ReturnGameResult(E_FAIL, L"Failed to create vertex buffer");

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
			ReturnGameResult(E_OUTOFMEMORY, L"Failed to allocate vertex buffer memory");

		vkBindBufferMemory(inDevice, outBuffer, outBufferMemory, 0);

		return GameResultOk;
	}

	void CopyBuffer(
		VkDevice inDevice,
		VkQueue inQueue,
		VkCommandPool inCommandPool,
		VkBuffer inSrcBuffer,
		VkBuffer inDstBuffer,
		VkDeviceSize inSize) {
		VkCommandBuffer commandBuffer = BeginSingleTimeCommands(inDevice, inCommandPool);

		VkBufferCopy copyRegion = {};
		copyRegion.srcOffset = 0;
		copyRegion.dstOffset = 0;
		copyRegion.size = inSize;
		vkCmdCopyBuffer(commandBuffer, inSrcBuffer, inDstBuffer, 1, &copyRegion);

		EndSingleTimeCommands(inDevice, inQueue, inCommandPool, commandBuffer);
	}

	GameResult CreateImage(
			VkPhysicalDevice inPhysicalDevice,
			VkDevice inDevice,
			std::uint32_t inWidth,
			std::uint32_t inHeight,
			std::uint32_t inMipLevels,
			VkSampleCountFlagBits inNumSamples,
			VkFormat inFormat,
			VkImageTiling inTiling,
			VkImageUsageFlags inUsage,
			VkMemoryPropertyFlags inProperties,
			VkImage& inImage,
			VkDeviceMemory& inImageMemory) {
		VkImageCreateInfo imageInfo = {};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = static_cast<std::uint32_t>(inWidth);
		imageInfo.extent.height = static_cast<std::uint32_t>(inHeight);
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = inMipLevels;
		imageInfo.arrayLayers = 1;
		imageInfo.format = inFormat;
		imageInfo.tiling = inTiling;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = inUsage;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.samples = inNumSamples;
		imageInfo.flags = 0;

		if (vkCreateImage(inDevice, &imageInfo, nullptr, &inImage) != VK_SUCCESS)
			ReturnGameResult(E_FAIL, L"Failed to create image");

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(inDevice, inImage, &memRequirements);

		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		CheckGameResult(FindMemoryType(inPhysicalDevice, memRequirements.memoryTypeBits, inProperties, allocInfo.memoryTypeIndex));

		if (vkAllocateMemory(inDevice, &allocInfo, nullptr, &inImageMemory) != VK_SUCCESS)
			ReturnGameResult(E_OUTOFMEMORY, L"Failed to allocate texture image memory");

		vkBindImageMemory(inDevice, inImage, inImageMemory, 0);

		return GameResultOk;
	}

	bool HasStencilComponent(VkFormat inFormat) {
		return inFormat == VK_FORMAT_D32_SFLOAT_S8_UINT || inFormat == VK_FORMAT_D24_UNORM_S8_UINT;
	}

	GameResult TransitionImageLayout(
			VkDevice inDevice,
			VkQueue inQueue,
			VkCommandPool inCommandPool,
			VkImage inImage,
			VkFormat inFormat,
			VkImageLayout inOldLayout,
			VkImageLayout inNewLayout,
			std::uint32_t inMipLevels) {
		VkCommandBuffer commandBuffer = BeginSingleTimeCommands(inDevice, inCommandPool);

		VkImageMemoryBarrier barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = inOldLayout;
		barrier.newLayout = inNewLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = inImage;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = inMipLevels;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;

		VkPipelineStageFlags sourceStage;
		VkPipelineStageFlags destinationStage;

		if (inNewLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

			if (HasStencilComponent(inFormat))
				barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
		else {
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		}

		if (inOldLayout == VK_IMAGE_LAYOUT_UNDEFINED && inNewLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (inOldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && inNewLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		else if (inOldLayout == VK_IMAGE_LAYOUT_UNDEFINED && inNewLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		}
		else {
			ReturnGameResult(E_NOINTERFACE, L"Unsupported layout transition");
		}

		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = 0;

		vkCmdPipelineBarrier(
			commandBuffer,
			sourceStage, destinationStage,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);

		EndSingleTimeCommands(inDevice, inQueue, inCommandPool, commandBuffer);

		return GameResultOk;
	}

	void CopyBufferToImage(
			VkDevice inDevice,
			VkQueue inQueue,
			VkCommandPool inCommandPool,
			VkBuffer inBuffer,
			VkImage inImage,
			std::uint32_t inWidth,
			std::uint32_t inHeight) {
		VkCommandBuffer commandBuffer = BeginSingleTimeCommands(inDevice, inCommandPool);

		VkBufferImageCopy region = {};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;

		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;

		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = { inWidth, inHeight, 1 };

		vkCmdCopyBufferToImage(
			commandBuffer,
			inBuffer,
			inImage,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&region);

		EndSingleTimeCommands(inDevice, inQueue, inCommandPool, commandBuffer);
	}

	GameResult CreateImageView(
			VkDevice inDevice, 
			VkImage inImage, 
			VkFormat inFormat, 
			std::uint32_t inMipLevles,
			VkImageAspectFlags inAspectFlags, 
			VkImageView& outImageView) {
		VkImageViewCreateInfo viewInfo = {};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = inImage;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = inFormat;
		viewInfo.subresourceRange.aspectMask = inAspectFlags;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = inMipLevles;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(inDevice, &viewInfo, nullptr, &outImageView) != VK_SUCCESS)
			ReturnGameResult(E_FAIL, L"Failed to create texture image view");

		return GameResultOk;
	}

	GameResult FindSupportedFormat(
			VkPhysicalDevice inDevice,
			const std::vector<VkFormat>& inCandidates,
			VkImageTiling inTiling,
			VkFormatFeatureFlags inFeatures,
			VkFormat& outFormat) {
		for (VkFormat format : inCandidates) {
			VkFormatProperties props;
			vkGetPhysicalDeviceFormatProperties(inDevice, format, &props);

			if (inTiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & inFeatures) == inFeatures) {
				outFormat = format;
				return GameResultOk;
			}
			else if (inTiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & inFeatures) == inFeatures) {
				outFormat = format;
				return GameResultOk;
			}
		}

		ReturnGameResult(E_NOINTERFACE, L"Failed to find suported format");
	}

	GameResult FindDepthFormat(VkPhysicalDevice inDevice, VkFormat& outFormat) {
		CheckGameResult(FindSupportedFormat(
			inDevice,
			{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT,
			outFormat
		));

		return GameResultOk;
	}

	GameResult GenerateMipmaps(
			VkPhysicalDevice inPhysicalDevice,
			VkDevice inDevice, 
			VkQueue inQueue,
			VkCommandPool inCommandPool, 
			VkImage inImage, 
			VkFormat inFormat,
			std::int32_t inTexWidth, 
			std::int32_t inTexHeight, 
			std::uint32_t inMipLevles) {
		VkFormatProperties formatProperties;
		vkGetPhysicalDeviceFormatProperties(inPhysicalDevice, inFormat, &formatProperties);

		if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
			ReturnGameResult(E_NOINTERFACE, L"Texture image format does not support linear bliting");

		VkCommandBuffer commandBuffer = BeginSingleTimeCommands(inDevice, inCommandPool);

		VkImageMemoryBarrier barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.image = inImage;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.subresourceRange.levelCount = 1;

		std::int32_t mipWidth = inTexWidth;
		std::int32_t mipHeight = inTexHeight;

		for (std::uint32_t i = 1; i < inMipLevles; ++i) {
			barrier.subresourceRange.baseMipLevel = i - 1;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

			vkCmdPipelineBarrier(
				commandBuffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				0,
				0, nullptr,
				0, nullptr,
				1, &barrier);

			VkImageBlit blit = {};
			blit.srcOffsets[0] = { 0, 0, 0 };
			blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
			blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.srcSubresource.mipLevel = i - 1;
			blit.srcSubresource.baseArrayLayer = 0;
			blit.srcSubresource.layerCount = 1;
			blit.dstOffsets[0] = { 0, 0, 0 };
			blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth >> 1 : 1, mipHeight > 1 ? mipHeight >> 1 : 1, 1 };
			blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.dstSubresource.mipLevel = i;
			blit.dstSubresource.baseArrayLayer = 0;
			blit.dstSubresource.layerCount = 1;

			vkCmdBlitImage(
				commandBuffer,
				inImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				inImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1, &blit,
				VK_FILTER_LINEAR);

			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			vkCmdPipelineBarrier(
				commandBuffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0,
				0, nullptr,
				0, nullptr,
				1, &barrier);

			if (mipWidth > 1) mipWidth = mipWidth >> 1;
			if (mipHeight > 1) mipHeight = mipHeight >> 1;;
		}

		barrier.subresourceRange.baseMipLevel = inMipLevles - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(
			commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier);

		EndSingleTimeCommands(inDevice, inQueue, inCommandPool, commandBuffer);

		return GameResultOk;
	}
}

VkVertexInputBindingDescription Vertex::GetBindingDescription() {
	VkVertexInputBindingDescription bindingDescription = {};
	bindingDescription.binding = 0;
	bindingDescription.stride = sizeof(Vertex);
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	return bindingDescription;
}

std::array<VkVertexInputAttributeDescription, 3> Vertex::GetAttributeDescriptions() {
	std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions = {};
	attributeDescriptions[0].binding = 0;
	attributeDescriptions[0].location = 0;
	attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[0].offset = offsetof(Vertex, mPos);

	attributeDescriptions[1].binding = 0;
	attributeDescriptions[1].location = 1;
	attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[1].offset = offsetof(Vertex, mColor);

	attributeDescriptions[2].binding = 0;
	attributeDescriptions[2].location = 2;
	attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
	attributeDescriptions[2].offset = offsetof(Vertex, mTexCoord);
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
	GLFWwindow* inMainWnd,
	CVBarrier* inCV,
	SpinlockBarrier* inSpinlock) {
	CheckGameResult(VkLowRenderer::Initialize(inClientWidth, inClientHeight, inNumThreads, inMainWnd));

	CheckGameResult(CreateImageViews());
	CheckGameResult(CreateRenderPass());
	CheckGameResult(CreateCommandPool());
	CheckGameResult(CreateColorResources());
	CheckGameResult(CreateDepthResources());
	CheckGameResult(CreateFramebuffers());
	CheckGameResult(CreateTextureImage());
	CheckGameResult(CreateTextureImageView());
	CheckGameResult(CreateTextureSampler());
	CheckGameResult(CreateDescriptorSetLayout());
	CheckGameResult(CreateGraphicsPipeline());
	CheckGameResult(LoadModel());
	CheckGameResult(CreateVertexBuffer());
	CheckGameResult(CreateIndexBuffer());
	CheckGameResult(CreateUniformBuffers());
	CheckGameResult(CreateDescriptorPool());
	CheckGameResult(CreateDescriptorSets());
	CheckGameResult(CreateCommandBuffers());
	CheckGameResult(CreateSyncObjects());

	return GameResultOk;
}

void VkRenderer::CleanUp() {
	vkDeviceWaitIdle(mDevice);

	vkDestroySampler(mDevice, mTextureSampler, nullptr);
	vkDestroyImageView(mDevice, mTextureImageView, nullptr);

	vkDestroyImage(mDevice, mTextureImage, nullptr);
	vkFreeMemory(mDevice, mTextureImageMemory, nullptr);

	for (size_t i = 0; i < SwapChainImageCount; ++i) {
		vkDestroyFence(mDevice, mInFlightFences[i], nullptr);
		vkDestroySemaphore(mDevice, mRenderFinishedSemaphores[i], nullptr);
		vkDestroySemaphore(mDevice, mImageAvailableSemaphores[i], nullptr);
	}

	vkDestroyBuffer(mDevice, mObjIndexBuffer, nullptr);
	vkFreeMemory(mDevice, mObjIndexBufferMemory, nullptr);

	vkDestroyBuffer(mDevice, mObjVertexBuffer, nullptr);
	vkFreeMemory(mDevice, mObjVertexBufferMemory, nullptr);

	vkDestroyDescriptorSetLayout(mDevice, mDescriptorSetLayout, nullptr);

	CleanUpSwapChain();

	vkDestroyCommandPool(mDevice, mCommandPool, nullptr);

	VkLowRenderer::CleanUp();

	bIsCleaned = true;
}

GameResult VkRenderer::Update(const GameTimer& gt, UINT inTid) {

	return GameResultOk;
}

GameResult VkRenderer::Draw(const GameTimer& gt, UINT inTid) {
	vkWaitForFences(mDevice, 1, &mInFlightFences[mCurrentFrame], VK_TRUE, UINT64_MAX);

	std::uint32_t imageIndex;
	VkResult result = vkAcquireNextImageKHR(
		mDevice,
		mSwapChain,
		UINT64_MAX,
		mImageAvailableSemaphores[mCurrentFrame],
		VK_NULL_HANDLE,
		&imageIndex
	);

	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		RecreateSwapChain();
		return GameResultOk;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		ReturnGameResult(E_FAIL, L"Failed to acquire swap chain image");
	}

	if (mImagesInFlight[imageIndex] != VK_NULL_HANDLE)
		vkWaitForFences(mDevice, 1, &mImagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);

	mImagesInFlight[imageIndex] = mInFlightFences[mCurrentFrame];

	CheckGameResult(UpdateUniformBuffer(gt, imageIndex));

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

	if (vkQueueSubmit(mGraphicsQueue, 1, &submitInfo, mInFlightFences[mCurrentFrame]) != VK_SUCCESS)
		ReturnGameResult(E_FAIL, L"Failed to submit draw command buffer");

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;

	VkSwapchainKHR swapChains[] = { mSwapChain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.pResults = nullptr;

	result = vkQueuePresentKHR(mPresentQueue, &presentInfo);
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || bFramebufferResized) {
		bFramebufferResized = false;
		RecreateSwapChain();
	}
	else if (result != VK_SUCCESS) {
		ReturnGameResult(E_FAIL, L"Failed to present swap chain image");
	}

	mCurrentFrame = (mCurrentFrame + 1) % SwapChainImageCount;

	return GameResultOk;
}

GameResult VkRenderer::OnResize(UINT inClientWidth, UINT inClientHeight) {
	CheckGameResult(VkLowRenderer::OnResize(inClientWidth, inClientHeight));

	bFramebufferResized = true;

	return GameResultOk;
}

GameResult VkRenderer::GetDeviceRemovedReason() {
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

UINT VkRenderer::AddAnimations(const std::string& inClipName, const Game::Animation& inAnim) {

	return 0;
}

GameResult VkRenderer::UpdateAnimationsMap() {


	return GameResultOk;
}

GameResult VkRenderer::CreateImageViews() {
	mSwapChainImageViews.resize(mSwapChainImages.size());

	for (size_t i = 0, end = mSwapChainImages.size(); i < end; ++i) {
		CheckGameResult(CreateImageView(mDevice, mSwapChainImages[i],  mSwapChainImageFormat, 1, VK_IMAGE_ASPECT_COLOR_BIT, mSwapChainImageViews[i]));
	}

	return GameResultOk;
}

GameResult VkRenderer::CreateRenderPass() {
	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = mSwapChainImageFormat;
	colorAttachment.samples = mMsaaSamples;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription depthAttachment = {};
	CheckGameResult(FindDepthFormat(mPhysicalDevice, depthAttachment.format));
	depthAttachment.samples = mMsaaSamples;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentRef = {};
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription colorAttachmentResolve = {};
	colorAttachmentResolve.format = mSwapChainImageFormat;
	colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentResolveRef = {};
	colorAttachmentResolveRef.attachment = 2;
	colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;
	subpass.pDepthStencilAttachment = &depthAttachmentRef;
	subpass.pResolveAttachments = &colorAttachmentResolveRef;

	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	std::array<VkAttachmentDescription, 3> attachments = { colorAttachment, depthAttachment, colorAttachmentResolve };

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = static_cast<std::uint32_t>(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	if (vkCreateRenderPass(mDevice, &renderPassInfo, nullptr, &mRenderPass) != VK_SUCCESS)
		ReturnGameResult(E_FAIL, L"Failed to create render pass");

	return GameResultOk;
}

GameResult VkRenderer::CreateFramebuffers() {
	mSwapChainFramebuffers.resize(mSwapChainImageViews.size());

	for (size_t i = 0, end = mSwapChainImageViews.size(); i < end; ++i) {
		std::array<VkImageView, 3> attachments = {
			mColorImageView,
			mDepthImageView,
			mSwapChainImageViews[i],
		};

		VkFramebufferCreateInfo framebufferInfo = {};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = mRenderPass;
		framebufferInfo.attachmentCount = static_cast<std::uint32_t>(attachments.size());
		framebufferInfo.pAttachments = attachments.data();
		framebufferInfo.width = mSwapChainExtent.width;
		framebufferInfo.height = mSwapChainExtent.height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(mDevice, &framebufferInfo, nullptr, &mSwapChainFramebuffers[i]) != VK_SUCCESS)
			ReturnGameResult(E_FAIL, L"Failed to create framebuffer");
	}

	return GameResultOk;
}

GameResult VkRenderer::CreateCommandPool() {
	QueueFamilyIndices indices = FindQueueFamilies(mPhysicalDevice);

	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = indices.GetGraphicsFamilyIndex();
	poolInfo.flags = 0;

	if (vkCreateCommandPool(mDevice, &poolInfo, nullptr, &mCommandPool) != VK_SUCCESS)
		ReturnGameResult(E_FAIL, L"Failed to create command pool");

	return GameResultOk;
}

GameResult VkRenderer::CreateColorResources() {
	VkFormat colorFormat = mSwapChainImageFormat;

	CheckGameResult(CreateImage(
		mPhysicalDevice,
		mDevice,
		mSwapChainExtent.width,
		mSwapChainExtent.height,
		1,
		mMsaaSamples,
		colorFormat,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		mColorImage,
		mColorImageMemory));

	CheckGameResult(CreateImageView(
		mDevice,
		mColorImage,
		colorFormat,
		1,
		VK_IMAGE_ASPECT_COLOR_BIT,
		mColorImageView));

	return GameResultOk;
}

GameResult VkRenderer::CreateDepthResources() {
	VkFormat depthFormat;
	CheckGameResult(FindDepthFormat(mPhysicalDevice, depthFormat));

	CheckGameResult(CreateImage(
		mPhysicalDevice,
		mDevice,
		mSwapChainExtent.width,
		mSwapChainExtent.height,
		1,
		mMsaaSamples,
		depthFormat,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		mDepthImage,
		mDepthImageMemory
	));

	CheckGameResult(CreateImageView(mDevice, mDepthImage, depthFormat, 1, VK_IMAGE_ASPECT_DEPTH_BIT, mDepthImageView));

	CheckGameResult(TransitionImageLayout(
		mDevice,
		mGraphicsQueue,
		mCommandPool,
		mDepthImage,
		depthFormat,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		1
	));

	return GameResultOk;
}

GameResult VkRenderer::CreateTextureImage() {
	int texWidth, texHeight, texChannels;

	//std::stringstream sstream;
	//sstream << TextureFilePath << "/89290801_p0_master1200.jpg";
	//std::string filePath = sstream.str();

	stbi_uc* pixels = stbi_load(TexturePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	//stbi_load(filePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

	mMipLevels = static_cast<std::uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

	VkDeviceSize imageSize = texWidth * texHeight * 4;

	if (!pixels) ReturnGameResult(E_FAIL, L"Failed to load texture image");

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	CreateBuffer(
		mPhysicalDevice,
		mDevice,
		imageSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer,
		stagingBufferMemory);

	void* data;
	vkMapMemory(mDevice, stagingBufferMemory, 0, imageSize, 0, &data);
	std::memcpy(data, pixels, static_cast<size_t>(imageSize));
	vkUnmapMemory(mDevice, stagingBufferMemory);

	stbi_image_free(pixels);

	CheckGameResult(CreateImage(
		mPhysicalDevice,
		mDevice,
		texWidth,
		texHeight,
		mMipLevels,
		VK_SAMPLE_COUNT_1_BIT,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		mTextureImage,
		mTextureImageMemory));

	CheckGameResult(TransitionImageLayout(
		mDevice,
		mGraphicsQueue,
		mCommandPool,
		mTextureImage,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		mMipLevels));

	CopyBufferToImage(
		mDevice,
		mGraphicsQueue,
		mCommandPool,
		stagingBuffer,
		mTextureImage,
		static_cast<std::uint32_t>(texWidth),
		static_cast<std::uint32_t>(texHeight));

	//CheckGameResult(TransitionImageLayout(
	//	mDevice,
	//	mGraphicsQueue,
	//	mCommandPool,
	//	mTextureImage,
	//	VK_FORMAT_R8G8B8A8_SRGB,
	//	VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	//	VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	//	mMipLevels));
	GenerateMipmaps(
		mPhysicalDevice,
		mDevice, 
		mGraphicsQueue, 
		mCommandPool, 
		mTextureImage,
		VK_FORMAT_R8G8B8A8_SRGB,
		texWidth, 
		texHeight, 
		mMipLevels);

	vkDestroyBuffer(mDevice, stagingBuffer, nullptr);
	vkFreeMemory(mDevice, stagingBufferMemory, nullptr);

	return GameResultOk;
}

GameResult VkRenderer::CreateTextureImageView() {
	CheckGameResult(CreateImageView(mDevice, mTextureImage, VK_FORMAT_R8G8B8A8_SRGB, mMipLevels, VK_IMAGE_ASPECT_COLOR_BIT, mTextureImageView));

	return GameResultOk;
}

GameResult VkRenderer::CreateTextureSampler() {
	VkSamplerCreateInfo samplerInfo = {};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.anisotropyEnable = VK_TRUE;
	samplerInfo.maxAnisotropy = 16;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = static_cast<float>(mMipLevels >> 1);
	samplerInfo.maxLod = static_cast<float>(mMipLevels);

	if (vkCreateSampler(mDevice, &samplerInfo, nullptr, &mTextureSampler) != VK_SUCCESS)
		ReturnGameResult(E_FAIL, L"Failed to create texture sampler");

	return GameResultOk;
}

GameResult VkRenderer::CreateDescriptorSetLayout() {
	VkDescriptorSetLayoutBinding uboLayoutBinding = {};
	uboLayoutBinding.binding = 0;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	uboLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
	samplerLayoutBinding.binding = 1;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.pImmutableSamplers = nullptr;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	std::array<VkDescriptorSetLayoutBinding, 2> bindings = { uboLayoutBinding, samplerLayoutBinding };

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(mDevice, &layoutInfo, nullptr, &mDescriptorSetLayout) != VK_SUCCESS)
		ReturnGameResult(E_FAIL, L"Failed to create descriptor set layout");

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
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f;
	rasterizer.depthBiasClamp = 0.0f;
	rasterizer.depthBiasSlopeFactor = 0.0f;

	VkPipelineMultisampleStateCreateInfo multisampling = {};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_TRUE;
	multisampling.rasterizationSamples = mMsaaSamples;
	multisampling.minSampleShading = 0.2f;
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
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &mDescriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 0;
	pipelineLayoutInfo.pPushConstantRanges = nullptr;

	if (vkCreatePipelineLayout(mDevice, &pipelineLayoutInfo, nullptr, &mPipelineLayout) != VK_SUCCESS)
		ReturnGameResult(E_FAIL, L"Failed to create pipeline layout");

	VkPipelineDepthStencilStateCreateInfo depthStencil = {};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.minDepthBounds = 0.0f;
	depthStencil.maxDepthBounds = 1.0f;
	depthStencil.stencilTestEnable = VK_FALSE;
	depthStencil.front = {};
	depthStencil.back = {};

	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = nullptr;
	pipelineInfo.layout = mPipelineLayout;
	pipelineInfo.renderPass = mRenderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = -1;

	if (vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &mGraphicsPipeline) != VK_SUCCESS)
		ReturnGameResult(E_FAIL, L"Failed to create graphics pipeline");

	vkDestroyShaderModule(mDevice, fragShaderModule, nullptr);
	vkDestroyShaderModule(mDevice, vertShaderModule, nullptr);

	return GameResultOk;
}

GameResult VkRenderer::CreateVertexBuffer() {
	VkDeviceSize bufferSize = sizeof(mObjVertices[0]) * mObjVertices.size();

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
	std::memcpy(data, mObjVertices.data(), static_cast<size_t>(bufferSize));
	vkUnmapMemory(mDevice, stagingBufferMemory);

	CheckGameResult(CreateBuffer(
		mPhysicalDevice,
		mDevice,
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		mObjVertexBuffer,
		mObjVertexBufferMemory));

	CopyBuffer(mDevice, mGraphicsQueue, mCommandPool, stagingBuffer, mObjVertexBuffer, bufferSize);

	vkDestroyBuffer(mDevice, stagingBuffer, nullptr);
	vkFreeMemory(mDevice, stagingBufferMemory, nullptr);

	return GameResultOk;
}

GameResult VkRenderer::CreateIndexBuffer() {
	VkDeviceSize bufferSize = sizeof(mObjIndices[0]) * mObjIndices.size();

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
	std::memcpy(data, mObjIndices.data(), bufferSize);
	vkUnmapMemory(mDevice, stagingBufferMemory);

	CheckGameResult(CreateBuffer(
		mPhysicalDevice,
		mDevice,
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		mObjIndexBuffer,
		mObjIndexBufferMemory));

	CopyBuffer(mDevice, mGraphicsQueue, mCommandPool, stagingBuffer, mObjIndexBuffer, bufferSize);

	vkDestroyBuffer(mDevice, stagingBuffer, nullptr);
	vkFreeMemory(mDevice, stagingBufferMemory, nullptr);

	return GameResultOk;
}

GameResult VkRenderer::CreateUniformBuffers() {
	VkDeviceSize bufferSize = sizeof(UniformBufferObject);

	mUniformBuffers.resize(SwapChainImageCount);
	mUniformBufferMemories.resize(SwapChainImageCount);

	for (size_t i = 0; i < SwapChainImageCount; ++i) {
		CreateBuffer(
			mPhysicalDevice,
			mDevice,
			bufferSize,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			mUniformBuffers[i],
			mUniformBufferMemories[i]);
	}

	return GameResultOk;
}

GameResult VkRenderer::CreateDescriptorPool() {
	std::array<VkDescriptorPoolSize, 2> poolSizes = {};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = static_cast<std::uint32_t>(SwapChainImageCount);

	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = static_cast<std::uint32_t>(SwapChainImageCount);

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = static_cast<std::uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = static_cast<std::uint32_t>(SwapChainImageCount);
	poolInfo.flags = 0;

	if (vkCreateDescriptorPool(mDevice, &poolInfo, nullptr, &mDescriptorPool) != VK_SUCCESS)
		ReturnGameResult(E_FAIL, L"Failed to create descriptor pool");

	return GameResultOk;
}

GameResult VkRenderer::CreateDescriptorSets() {
	std::vector<VkDescriptorSetLayout> layouts(SwapChainImageCount, mDescriptorSetLayout);

	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = mDescriptorPool;
	allocInfo.descriptorSetCount = static_cast<std::uint32_t>(SwapChainImageCount);
	allocInfo.pSetLayouts = layouts.data();

	mDescriptorSets.resize(SwapChainImageCount);
	if (vkAllocateDescriptorSets(mDevice, &allocInfo, mDescriptorSets.data()) != VK_SUCCESS)
		ReturnGameResult(E_FAIL, L"Failed to allocate descriptor sets");

	for (size_t i = 0; i < SwapChainImageCount; ++i) {
		VkDescriptorBufferInfo bufferInfo = {};
		bufferInfo.buffer = mUniformBuffers[i];
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(UniformBufferObject);

		VkDescriptorImageInfo imageInfo = {};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = mTextureImageView;
		imageInfo.sampler = mTextureSampler;

		std::array<VkWriteDescriptorSet, 2> descriptorWrites = {};
		descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[0].dstSet = mDescriptorSets[i];
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pBufferInfo = &bufferInfo;

		descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[1].dstSet = mDescriptorSets[i];
		descriptorWrites[1].dstBinding = 1;
		descriptorWrites[1].dstArrayElement = 0;
		descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[1].descriptorCount = 1;
		descriptorWrites[1].pImageInfo = &imageInfo;

		vkUpdateDescriptorSets(mDevice, static_cast<std::uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
	}

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
		ReturnGameResult(E_FAIL, L"Failed to create command buffers");

	for (size_t i = 0, end = mCommandBuffers.size(); i < end; ++i) {
		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = 0;
		beginInfo.pInheritanceInfo = nullptr;

		if (vkBeginCommandBuffer(mCommandBuffers[i], &beginInfo) != VK_SUCCESS)
			ReturnGameResult(E_FAIL, L"Failed to begin recording command buffer");

		VkRenderPassBeginInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = mRenderPass;
		renderPassInfo.framebuffer = mSwapChainFramebuffers[i];
		renderPassInfo.renderArea.offset = { 0,0 };
		renderPassInfo.renderArea.extent = mSwapChainExtent;

		// Note: that the order of clearValues should be identical to the order of attachments.
		std::array<VkClearValue, 2> clearValues = {};
		clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
		clearValues[1].depthStencil = { 1.0f, 0 };

		renderPassInfo.clearValueCount = static_cast<std::uint32_t>(clearValues.size());
		renderPassInfo.pClearValues = clearValues.data();

		vkCmdBeginRenderPass(mCommandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(mCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, mGraphicsPipeline);

		VkBuffer vertexBuffers[] = { mObjVertexBuffer };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(mCommandBuffers[i], 0, 1, vertexBuffers, offsets);

		vkCmdBindIndexBuffer(mCommandBuffers[i], mObjIndexBuffer, 0, VK_INDEX_TYPE_UINT32);

		vkCmdBindDescriptorSets(mCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, 1, &mDescriptorSets[i], 0, nullptr);

		//vkCmdDraw(mCommandBuffers[i], static_cast<std::uint32_t>(mVertices.size()), 1, 0, 0);
		vkCmdDrawIndexed(mCommandBuffers[i], static_cast<std::uint32_t>(mObjIndices.size()), 1, 0, 0, 0);

		vkCmdEndRenderPass(mCommandBuffers[i]);

		if (vkEndCommandBuffer(mCommandBuffers[i]) != VK_SUCCESS)
			ReturnGameResult(E_FAIL, L"Failed to record command buffer");
	}

	return GameResultOk;
}

GameResult VkRenderer::CreateSyncObjects() {
	mImageAvailableSemaphores.resize(SwapChainImageCount);
	mRenderFinishedSemaphores.resize(SwapChainImageCount);
	mInFlightFences.resize(SwapChainImageCount);
	mImagesInFlight.resize(SwapChainImageCount, VK_NULL_HANDLE);

	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (size_t i = 0; i < SwapChainImageCount; ++i) {
		if (vkCreateSemaphore(mDevice, &semaphoreInfo, nullptr, &mImageAvailableSemaphores[i]) != VK_SUCCESS ||
			vkCreateSemaphore(mDevice, &semaphoreInfo, nullptr, &mRenderFinishedSemaphores[i]) != VK_SUCCESS ||
			vkCreateFence(mDevice, &fenceInfo, nullptr, &mInFlightFences[i]) != VK_SUCCESS)
			ReturnGameResult(E_FAIL, L"Failed to create synchronization object(s) for a frame");
	}

	return GameResultOk;
}

GameResult VkRenderer::RecreateSwapChain() {
	vkDeviceWaitIdle(mDevice);

	CleanUpSwapChain();

	VkLowRenderer::RecreateSwapChain();

	CheckGameResult(CreateImageViews());
	CheckGameResult(CreateRenderPass());
	CheckGameResult(CreateGraphicsPipeline());
	CheckGameResult(CreateColorResources());
	CheckGameResult(CreateDepthResources());
	CheckGameResult(CreateFramebuffers());
	CheckGameResult(CreateUniformBuffers());
	CheckGameResult(CreateDescriptorPool());
	CheckGameResult(CreateDescriptorSets());
	CheckGameResult(CreateCommandBuffers());

	return GameResultOk;
}

GameResult VkRenderer::CleanUpSwapChain() {
	vkDestroyImageView(mDevice, mColorImageView, nullptr);
	vkDestroyImage(mDevice, mColorImage, nullptr);
	vkFreeMemory(mDevice, mColorImageMemory, nullptr);

	vkDestroyImageView(mDevice, mDepthImageView, nullptr);
	vkDestroyImage(mDevice, mDepthImage, nullptr);
	vkFreeMemory(mDevice, mDepthImageMemory, nullptr);

	for (size_t i = 0; i < SwapChainImageCount; ++i) {
		vkDestroyBuffer(mDevice, mUniformBuffers[i], nullptr);
		vkFreeMemory(mDevice, mUniformBufferMemories[i], nullptr);
	}

	vkDestroyDescriptorPool(mDevice, mDescriptorPool, nullptr);

	for (size_t i = 0; i < SwapChainImageCount; ++i) {
		vkDestroyFramebuffer(mDevice, mSwapChainFramebuffers[i], nullptr);
	}

	vkFreeCommandBuffers(mDevice, mCommandPool, static_cast<std::uint32_t>(mCommandBuffers.size()), mCommandBuffers.data());

	vkDestroyPipeline(mDevice, mGraphicsPipeline, nullptr);
	vkDestroyPipelineLayout(mDevice, mPipelineLayout, nullptr);
	vkDestroyRenderPass(mDevice, mRenderPass, nullptr);

	for (size_t i = 0; i < SwapChainImageCount; ++i) {
		vkDestroyImageView(mDevice, mSwapChainImageViews[i], nullptr);
	}

	CheckGameResult(VkLowRenderer::CleanUpSwapChain());

	return GameResultOk;
}

GameResult VkRenderer::UpdateUniformBuffer(const GameTimer& gt, std::uint32_t inCurrIndex) {
	UniformBufferObject ubo = {};
	ubo.mModel = glm::rotate(glm::mat4(1.0f), glm::sin(gt.TotalTime() * 0.5f) * glm::radians(15.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	ubo.mView = glm::lookAt(
		glm::vec3(1.0f, 1.0f, 0.75f),
		glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)
	);
	ubo.mProj = glm::perspective(glm::radians(90.0f), static_cast<float>(mSwapChainExtent.width) / static_cast<float>(mSwapChainExtent.height), 0.1f, 10.0f);
	ubo.mProj[1][1] *= -1.0f;

	void* data;
	const auto& bufferMemory = mUniformBufferMemories[inCurrIndex];
	vkMapMemory(mDevice, bufferMemory, 0, sizeof(ubo), 0, &data);
	std::memcpy(data, &ubo, sizeof(ubo));
	vkUnmapMemory(mDevice, bufferMemory);

	return GameResultOk;
}

GameResult VkRenderer::LoadModel() {
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warn, err;

	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, ModelPath.c_str())) {
		std::wstringstream wsstream;
		wsstream << warn.c_str() << err.c_str();
		ReturnGameResult(E_FAIL, wsstream.str());
	}

	for (const auto& shape : shapes) {
		for (const auto& index : shape.mesh.indices) {
			Vertex vertex = {};

			vertex.mPos = {
				attrib.vertices[3 * index.vertex_index + 0],
				attrib.vertices[3 * index.vertex_index + 1],
				attrib.vertices[3 * index.vertex_index + 2]
			};

			vertex.mTexCoord = {
				attrib.texcoords[2 * index.texcoord_index + 0],
				1.0f - attrib.texcoords[2 * index.texcoord_index + 1],
			};

			vertex.mColor = { 1.0f, 1.0f, 1.0f };

			if (mObjUniqueVertices.count(vertex) == 0) {
				mObjUniqueVertices[vertex] = static_cast<std::uint32_t>(mObjVertices.size());
				mObjVertices.push_back(vertex);
			}

			mObjIndices.push_back(static_cast<std::uint32_t>(mObjUniqueVertices[vertex]));
		}
	}

	return GameResultOk;
}

#endif // UsingVulkan