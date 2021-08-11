#pragma once

#include "LearningDXR/LowRenderer.h"
#include "common/GameTimer.h"

class Renderer : public LowRenderer {
public:
	Renderer();
	virtual ~Renderer();

private:
	Renderer(const Renderer& inRef) = delete;
	Renderer(Renderer&& inRVal) = delete;
	Renderer& operator=(const Renderer& inRef) = delete;
	Renderer& operator=(Renderer&& inRVal) = delete;

public:
	virtual bool Initialize(HWND hMainWnd, UINT inClientWidth = 800, UINT inClientHeight = 600) override;
	virtual void CleanUp() override;

	virtual bool Update(const GameTimer& gt);
	virtual bool Draw();

	virtual bool OnResize(UINT inClientWidth, UINT inClientHeight) override;

private:
};