#pragma once

#include "DX12Game/GameCore.h"

class LowRenderer {
protected:
	LowRenderer() = default;

private:
	LowRenderer(const LowRenderer& inRef) = delete;
	LowRenderer(LowRenderer&& inRVal) = delete;
	LowRenderer& operator=(const LowRenderer& inRef) = delete;
	LowRenderer& operator=(LowRenderer&& inRVal) = delete;

public:
	virtual ~LowRenderer() = default;

protected:
	float AspectRatio() const;

	virtual GameResult Initialize(HWND hMainWnd, UINT inClientWidth, UINT inClientHeight) = 0;
	virtual GameResult Initialize(GLFWwindow* inMainWnd, UINT inClientWidth, UINT inClientHeight) = 0;
	virtual void CleanUp() = 0;
	virtual GameResult Update(const GameTimer& gt) = 0;
	virtual GameResult Draw(const GameTimer& gt) = 0;

	virtual GameResult OnResize(UINT inClientWidth, UINT inClientHeight) = 0;

protected:
	UINT mClientWidth = 0;
	UINT mClientHeight = 0;
};