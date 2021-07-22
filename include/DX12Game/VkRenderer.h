#pragma once

#include "DX12Game/VkLowRenderer.h"
#include "DX12Game/Renderer.h"

class VkRenderer : public VkLowRenderer, public Renderer {
public:
	VkRenderer();
	virtual ~VkRenderer();

private:
	VkRenderer(const VkRenderer& inRef) = delete;
	VkRenderer(VkRenderer&& inRVal) = delete;
	VkRenderer& operator=(const VkRenderer& inRef) = delete;
	VkRenderer& operator=(VkRenderer&& inRVal) = delete;

public:
	virtual GameResult Initialize(GLFWwindow* inMainWnd, UINT inClientWidth, UINT inClientHeight) override;
	virtual void CleanUp() override;
	virtual GameResult Update(const GameTimer& gt) override;
	virtual GameResult Draw(const GameTimer& gt) override;
	virtual GameResult OnResize(UINT inClientWidth, UINT inClientHeight) override;

	virtual void UpdateWorldTransform(const std::string& inRenderItemName,
		const DirectX::XMMATRIX& inTransform, bool inIsSkeletal) override;
	virtual void UpdateInstanceAnimationData(const std::string& inRenderItemName,
		UINT inAnimClipIdx, float inTimePos, bool inIsSkeletal) override;

	virtual void SetVisible(const std::string& inRenderItemName, bool inState) override;
	virtual void SetSkeletonVisible(const std::string& inRenderItemName, bool inState) override;

	virtual GameResult AddGeometry(const Mesh* inMesh) override;
	virtual void AddRenderItem(std::string& ioRenderItemName, const Mesh* inMesh) override;
	virtual GameResult AddMaterials(const GUnorderedMap<std::string, MaterialIn>& inMaterials) override;

	virtual UINT AddAnimations(const std::string& inClipName, const Animation& inAnim) override;
	virtual GameResult UpdateAnimationsMap() override;

private:
	bool bIsCleaned = false;
};