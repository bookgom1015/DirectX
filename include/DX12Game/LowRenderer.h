#pragma once

#include "DX12Game/GameCore.h"

class LowRenderer {
protected:
	LowRenderer() = default;

public:
	virtual ~LowRenderer() = default;

private:
	LowRenderer(const LowRenderer& inRef) = delete;
	LowRenderer(LowRenderer&& inRVal) = delete;
	LowRenderer& operator=(const LowRenderer& inRef) = delete;
	LowRenderer& operator=(LowRenderer&& inRVal) = delete;

protected:
	float AspectRatio() const;

	virtual GameResult Initialize(
		UINT inClientWidth, 
		UINT inClientHeight,
		UINT inNumThreads = 1,
		HWND hMainWnd = NULL,
		GLFWwindow* inMainWnd = nullptr) = 0;

	virtual void CleanUp() = 0;
	virtual GameResult Update(const GameTimer& gt, UINT inTid = 0) = 0;
	virtual GameResult Draw(const GameTimer& gt, UINT inTid = 0) = 0;

	virtual GameResult OnResize(UINT inClientWidth, UINT inClientHeight) = 0;

protected:
	UINT mClientWidth = 0;
	UINT mClientHeight = 0;

	UINT mNumThreads;
};