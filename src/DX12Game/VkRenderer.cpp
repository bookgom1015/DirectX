#include "DX12Game/VkRenderer.h"

VkRenderer::VkRenderer()
	: VkLowRenderer() {

}

VkRenderer::~VkRenderer() {
	if (!bIsCleaned)
		CleanUp();
}

GameResult VkRenderer::Initialize(GLFWwindow* inMainWnd, UINT inClientWidth, UINT inClientHeight) {
	CheckGameResult(VkLowRenderer::Initialize(inMainWnd, inClientWidth, inClientHeight));

	return GameResult(S_OK);
}

void VkRenderer::CleanUp() {
	VkLowRenderer::CleanUp();

	bIsCleaned = true;
}

GameResult VkRenderer::Update(const GameTimer& gt) {


	return GameResult(S_OK);
}

GameResult VkRenderer::Draw(const GameTimer& gt) {


	return GameResult(S_OK);
}

GameResult VkRenderer::OnResize(UINT inClientWidth, UINT inClientHeight) {
	CheckGameResult(VkLowRenderer::OnResize(inClientWidth, inClientHeight));

	return GameResult(S_OK);
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


	return GameResult(S_OK);
}

void VkRenderer::AddRenderItem(std::string& ioRenderItemName, const Mesh* inMesh) {

}

GameResult VkRenderer::AddMaterials(const GUnorderedMap<std::string, MaterialIn>& inMaterials) {


	return GameResult(S_OK);
}

UINT VkRenderer::AddAnimations(const std::string& inClipName, const Animation& inAnim) {

	return 0;
}

GameResult VkRenderer::UpdateAnimationsMap() {


	return GameResult(S_OK);
}