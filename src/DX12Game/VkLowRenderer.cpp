#include "DX12Game/VkLowRenderer.h"

VkLowRenderer::VkLowRenderer() {

}

VkLowRenderer::~VkLowRenderer() {
	if (!bIsCleaned)
		CleanUp();
}

GameResult VkLowRenderer::Initialize(GLFWwindow* inMainWnd, UINT inWidth, UINT inHeight) {
	CheckGameResult(InitVulkan());

	return GameResult(S_OK);
}

void VkLowRenderer::CleanUp() {
	vkDestroyInstance(mInstance, nullptr);

	bIsCleaned = true;
}

GameResult VkLowRenderer::OnResize(UINT inClientWidth, UINT inClientHeight) {


	return GameResult(S_OK);
}

GameResult VkLowRenderer::Initialize(HWND hMainWnd, UINT inClientWidth, UINT inClientHeight) {
	// Do nothing.
	// This is for D3D12.

	return GameResult(S_OK);
}

GameResult VkLowRenderer::InitVulkan() {
	CheckGameResult(CreateInstance());

	return GameResult(S_OK);
}

std::vector<const char*> VkLowRenderer::GetRequiredExtensions() {
	std::uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	WLogln(L"Required extensions by GLFW:");
	for (std::uint32_t i = 0; i < glfwExtensionCount; ++i)
		Logln("  ", glfwExtensions[i]);

	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
	//if (gEnableValidationLayers)
	//	extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

	return extensions;
}

GameResult VkLowRenderer::CreateInstance() {
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "VkGame";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	std::uint32_t availableExtensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(availableExtensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionCount, availableExtensions.data());

	auto requiredExtensions = GetRequiredExtensions();
	std::vector<const char*> missingExtensions;

	for (const auto& requiredExt : requiredExtensions) {
		bool supported = false;

		for (const auto& availableExt : availableExtensions) {
			if (std::strcmp(requiredExt, availableExt.extensionName) == 0) {
				supported = true;
				break;
			}
		}

		if (!supported)
			missingExtensions.push_back(requiredExt);
	}

	WLogln(L"Not supported extensions:")
	if (missingExtensions.size() > 0) {
		for (const auto& missingExt : missingExtensions)
			Logln("  ", missingExt);
		ReturnGameResult(S_FALSE, L"that extensions are not supported");
	}
	else {
		Logln("  None");
	}

	createInfo.enabledExtensionCount = static_cast<std::uint32_t>(requiredExtensions.size());
	createInfo.ppEnabledExtensionNames = requiredExtensions.data();
	createInfo.enabledLayerCount = 0;

	if (vkCreateInstance(&createInfo, nullptr, &mInstance) != VK_SUCCESS)
		ReturnGameResult(S_FALSE, L"Failed to create instance");

	return GameResult(S_OK);
}