#pragma once

#if defined(_DEBUG)
#define _CRTCDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include "DX12Game/SoundEvent.h"
#include "DX12Game/PerfAnalyzer.h"

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
		EPaused,
		ETerminated
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
	GameResult RunLoop();
	GameResult GameLoop();

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
	void ProcessInput(const GameTimer& gt, UINT inTid = 0);
	GameResult UpdateGame(const GameTimer& gt, UINT inTid = 0);
	GameResult Draw(const GameTimer& gt, UINT inTid = 0);

	GameResult InitMainWindow();
	GameResult OnResize();

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
	bool bFinishedInit = false;

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

	PerfAnalyzer mPerfAnalyzer;

#ifdef MT_World
	std::vector<std::thread> mThreads;
	std::vector<std::vector<Actor*>> mActors;
	std::vector<std::vector<Actor*>> mPendingActors;

	std::vector<bool> bUpdatingActors;
	UINT mNextThreadId = 0;

	std::mutex mAddingActorMutex;

	UINT mNumProcessors = 1;

	std::unique_ptr<CVBarrier> mCVBarrier;
	std::unique_ptr<SpinlockBarrier> mSpinlockBarrier;
#else
	GVector<Actor*> mActors;
	GVector<Actor*> mPendingActors;
	bool bUpdatingActors = false;
#endif

	std::unordered_map<std::string, std::unique_ptr<Mesh>> mMeshes;

	SoundEvent mMusicEvent;
	float mPrevBusVolume = 0.0f;
};