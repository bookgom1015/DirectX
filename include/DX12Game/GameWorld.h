#pragma once

#if defined(_DEBUG)
#define _CRTCDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include "DX12Game/SoundEvent.h"

class Renderer;
// Forward declarations.
#ifdef UsingVulkan
	class VkRenderer;
#else
	class DxRenderer;
#endif
class AudioSystem;
class InputSystem;
class Actor;
class Mesh;

class GameWorld final {
public:
	enum GameState {
		EPlay,
		EPaused
	};

public:
	GameWorld(HINSTANCE hInstance);
	virtual ~GameWorld();

private:
	///
	// GameWorld doesn't allow substitution and replication.
	///
	GameWorld(const GameWorld& src) = delete;
	GameWorld& operator=(const GameWorld& rhs) = delete;
	GameWorld(GameWorld&& src) = delete;
	GameWorld& operator=(GameWorld&& rhs) = delete;

public:
	GameResult Initialize(INT inWidth = 800, UINT inHeight = 600);
	void CleanUp();
	bool LoadData();
	void UnloadData();
	int RunLoop();

#ifndef MT_World
	int GameLoop();
#else
	//* Multi-threaded version of the fuction GameLoop.
	int MTGameLoop();
#endif

	void AddActor(Actor* inActor);
	void RemoveActor(Actor* inActor);

	Mesh* AddMesh(const std::string& inFileName, bool inIsSkeletal = false, 
		bool inNeedToBeAligned = false, bool bMultiThreading = false);
	void RemoveMesh(const std::string& inFileName);

	//* Returns single-tone for GameWorld.
	static GameWorld* GetWorld();

	Renderer* GetRenderer() const;
	InputSystem* GetInputSystem() const;

	UINT GetPrimaryMonitorWidth() const;
	UINT GetPrimaryMonitorHeight() const;

	UINT GetClientWidth() const;
	UINT GetClientHeight() const;

	HWND GetMainWindowsHandle() const;

	//* Processes window messages.
	LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
#ifndef MT_World
	void ProcessInput();
	void UpdateGame(const GameTimer& gt);
#else
	//* After InputSystem handles input data, actor or widget handle it.
	void MTProcessInput(UINT tid, ThreadBarrier& inBarrier);
	//* Multi-threaded version of the function UpdateGame.
	void MTUpdateGame(const GameTimer& gt, UINT tid, ThreadBarrier& inBarrier);
#endif 
	void Draw(const GameTimer& gt);

	GameResult InitMainWindow();
	GameResult OnResize();
	void CalculateFrameStats();

	///
	// Call-back functions for mouse state.
	///
	void OnMouseDown(WPARAM inBtnState, int inX, int inY);
	void OnMouseUp(WPARAM inBtnState, int inX, int inY);
	void OnMouseMove(WPARAM inBtnState, int inX, int inY);

	///
	// Call-back functions for keyboard state.
	///
	void OnKeyboardInput(UINT msg, WPARAM wParam, LPARAM lParam);

private:
	static GameWorld* sWorld;

	bool bIsCleaned = false;

#ifdef UsingVulkan
	GLFWwindow* mMainGLFWWindow;

	std::unique_ptr<VkRenderer> mRenderer = nullptr;
#else
	std::unique_ptr<DxRenderer> mRenderer = nullptr;
#endif
	std::unique_ptr<AudioSystem> mAudioSystem = nullptr;
	std::unique_ptr<InputSystem> mInputSystem = nullptr;

	HINSTANCE mhInst = nullptr;		// Application instance handle
	HWND mhMainWnd = nullptr;		// Main window handle
	bool mAppPaused = false;		// Is the application paused?
	bool mMinimized = false;		// Is the application minimized?
	bool mMaximized = false;		// Is the application maximized?
	bool mResizing = false;			// Are the resize bars being dragged?
	bool mFullscreenState = false;	// Fullscreen enabled

	std::wstring mMainWndCaption = L"D3D12 Game";
	
	UINT mClientWidth;
	UINT mClientHeight;
	UINT mPrimaryMonitorWidth = 0;
	UINT mPrimaryMonitorHeight = 0;

	GameWorld::GameState mGameState = GameWorld::GameState::EPlay;

	GameTimer mTimer;
	GameTimer::LimitFrameRate mLimitFrameRate;

#ifdef MT_World
	GVector<std::thread> mThreads;
	GVector<GVector<Actor*>> mMTActors;
	GVector<GVector<Actor*>> mMTPendingActors;

	GVector<bool> bMTUpdatingActors;
	UINT mNextThreadId = 0;

	std::mutex mAddingActorMutex;
#else
	GVector<Actor*> mActors;
	GVector<Actor*> mPendingActors;
	bool bUpdatingActors = false;
#endif

	GUnorderedMap<std::string, std::unique_ptr<Mesh>> mMeshes;

	SoundEvent mMusicEvent;
	float mPrevBusVolume = 0.0f;
};