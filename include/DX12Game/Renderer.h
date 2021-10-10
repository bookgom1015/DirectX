#pragma once

#include "DX12Game/GameCore.h"

class GameCamera;
class Mesh;
class Animation;

const size_t gNumBones = 512;

class Renderer {
protected:
	Renderer() = default;

public:
	virtual ~Renderer() = default;

private:
	Renderer(const Renderer& inRef) = delete;
	Renderer(Renderer&& inRVal) = delete;
	Renderer& operator=(const Renderer& inRef) = delete;
	Renderer& operator=(Renderer&& inRVal) = delete;

public:
	virtual void UpdateWorldTransform(const std::string& inRenderItemName, 
				const DirectX::XMMATRIX& inTransform, bool inIsSkeletal) = 0;
	virtual void UpdateInstanceAnimationData(const std::string& inRenderItemName,
				UINT inAnimClipIdx, float inTimePos, bool inIsSkeletal) = 0;

	virtual void SetVisible(const std::string& inRenderItemName, bool inState) = 0;
	virtual void SetSkeletonVisible(const std::string& inRenderItemName, bool inState) = 0;

	virtual GameResult AddGeometry(const Mesh* inMesh) = 0;
	virtual void AddRenderItem(std::string& ioRenderItemName, const Mesh* inMesh) = 0;
	virtual GameResult AddMaterials(const std::unordered_map<std::string, MaterialIn>& inMaterials) = 0;

	virtual UINT AddAnimations(const std::string& inClipName, const Animation& inAnim) = 0;
	virtual GameResult UpdateAnimationsMap() = 0;

	void AddOutputText(const std::string& inNameId, const std::wstring& inText,
			float inX = 0.0f, float inY = 0.0f, float inScale = 1.0f, float inLifeTime = -1.0f);
	void RemoveOutputText(const std::string& inName);

	GameCamera* GetMainCamera() const;
	void SetMainCamerea(GameCamera* inCamera);

	bool IsValid() const;

protected:
	bool bIsValid = false;

	GameCamera* mMainCamera = nullptr;

	std::unordered_map<std::string /* Name id */,
		std::pair<std::wstring /* Output text */, DirectX::SimpleMath::Vector4>> mOutputTexts;
};