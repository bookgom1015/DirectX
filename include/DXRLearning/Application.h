#pragma once

#include "DXRLearning/Renderer.h"

class Application {
public:
	Application();
	virtual ~Application();

private:
	Application(const Application& inRef) = delete;
	Application(Application&& inRVal) = delete;
	Application& operator=(const Application& inRef) = delete;
	Application& operator=(Application&& inRVal) = delete;

public:
	bool Initialize();
	int RunLoop();
	void CleanUp();

	static Application* GetApp();

	LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
	bool InitMainWindow();
	void OnResize();

	bool Update();
	bool Draw();

private:
	static Application* sApp;

	bool bIsCleanedUp;

	HINSTANCE mhInst = nullptr;		// Application instance handle
	HWND mhMainWnd = nullptr;		// Main window handle
	bool mAppPaused = false;		// Is the application paused?
	bool mMinimized = false;		// Is the application minimized?
	bool mMaximized = false;		// Is the application maximized?
	bool mResizing = false;			// Are the resize bars being dragged?
	bool mFullscreenState = false;	// Fullscreen enabled

	UINT mClientWidth;
	UINT mClientHeight;

	std::unique_ptr<Renderer> mRenderer;

	GameTimer mTimer;

	UINT mPrimaryMonitorWidth;
	UINT mPrimaryMonitorHeight;
};