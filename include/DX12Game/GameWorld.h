#pragma once

#if defined(_DEBUG)
#define _CRTCDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include "DX12Game/SoundEvent.h"

// Forward declarations.
class Renderer;
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
	DxResult Initialize();
	bool LoadData();
	void UnloadData();
	int RunLoop();

#if !defined(MT_World)
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
#if !defined(MT_World)
	void ProcessInput();
	void UpdateGame(const GameTimer& gt);
#else
	//* After InputSystem handles input data, actor or widget handle it.
	void MTProcessInput(UINT tid, ThreadBarrier& inBarrier);
	//* Multi-threaded version of the function UpdateGame.
	void MTUpdateGame(const GameTimer& gt, UINT tid, ThreadBarrier& inBarrier);
#endif 
	void Draw(const GameTimer& gt);

	DxResult InitMainWindow();
	void OnResize();
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

	std::unique_ptr<Renderer> mRenderer = nullptr;
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
	
	UINT mClientWidth = 800;
	UINT mClientHeight = 600;
	UINT mPrimaryMonitorWidth = 0;
	UINT mPrimaryMonitorHeight = 0;

	GameWorld::GameState mGameState = GameWorld::GameState::EPlay;

	GameTimer mTimer;
	GameTimer::LimitFrameRate mLimitFrameRate;

#if defined(MT_World)
	std::vector<std::thread> mThreads;
	std::vector<std::vector<Actor*>> mMTActors;
	std::vector<std::vector<Actor*>> mMTPendingActors;

	std::vector<bool> bMTUpdatingActors;
	UINT mNextThreadId = 0;

	std::mutex mAddingActorMutex;
#else
	std::vector<Actor*> mActors;
	std::vector<Actor*> mPendingActors;
	bool bUpdatingActors = false;
#endif

	std::unordered_map<std::string, std::unique_ptr<Mesh>> mMeshes;

	SoundEvent mMusicEvent;
	float mPrevBusVolume = 0.0f;
};