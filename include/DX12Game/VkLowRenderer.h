#pragma once

#include "DX12Game/LowRenderer.h"

class VkLowRenderer : public LowRenderer {
protected:
	VkLowRenderer();

public:
	virtual ~VkLowRenderer();

public:
	virtual GameResult Initialize(GLFWwindow* inMainWnd, UINT inWidth, UINT inHeight) override;
	virtual void CleanUp() override;

	virtual GameResult OnResize(UINT inClientWidth, UINT inClientHeight) override;

private:
	virtual GameResult Initialize(HWND hMainWnd, UINT inClientWidth, UINT inClientHeight) override;

	GameResult InitVulkan();

	std::vector<const char*> GetRequiredExtensions();
	GameResult CreateInstance();


private:
	bool bIsCleaned = false;

	VkInstance mInstance;
};