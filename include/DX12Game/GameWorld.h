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
	//
	//* GameWorld doesn't allow substitution and replication.
	//

	GameWorld(const GameWorld& src) = delete;
	GameWorld& operator=(const GameWorld& rhs) = delete;
	GameWorld(GameWorld&& src) = delete;
	GameWorld& operator=(GameWorld&& rhs) = delete;

public:
	//* Initializes window and other important systems(rendering, audio...).
	bool Initialize();
	//* Loads mesh, audio, etc. data.
	bool LoadData();
	//*
	void UnloadData();
	//* Start the game engine loop.
	int RunLoop();

#if !defined(MT_World)
	//* Ticks timer, process input, updates game, and draws screen.
	int GameLoop();
#else
	//* Multi-threaded version of the fuction GameLoop.
	int MTGameLoop();
#endif

	//* Registers the actor in the game engine(Actor automatically registers themself).
	void AddActor(Actor* inActor);
	//* Unregisters the actor in the game engine.
	//* When you call this function, the actor is no longer updated.
	void RemoveActor(Actor* inActor);

	//* Creates and registers the mesh.
	Mesh* AddMesh(const std::string& inFileName, bool inIsSkeletal = false, bool inNeedToBeAligned = false);
	//* Multi-threaded version of the function AddMesh.
	Mesh* MTAddMesh(const std::string& inFileName, bool inIsSkeletal = false, bool inNeedToBeAligned = false);
	//* Unregisters the mesh.
	void RemoveMesh(const std::string& inFileName);

	//* Returns single-tone for GameWorld.
	static GameWorld* GetWorld();

	Renderer* GetRenderer() const;
	InputSystem* GetInputSystem() const;

	//* Returns horizontal resolution of the primary monitor.
	int GetPrimaryMonitorWidth() const;
	//* Returns vertical resolution of the primary monitor.
	int GetPrimaryMonitorHeight() const;

	HWND GetMainWindowsHandle() const;

	//* Processes window messages.
	LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	//* Temporary function that show visible object count as text.
	void SetVCount(UINT inCount);

private:
#if !defined(MT_World)
	//* After InputSystem handles input data, actor or widget handle it.
	void ProcessInput();
	//* Updates all actors, handle pending actors and dead actors.
	//* After that, updates rendering and audio systems.
	void UpdateGame(const GameTimer& gt);
#else
	//* After InputSystem handles input data, actor or widget handle it.
	void MTProcessInput(UINT tid, ThreadBarrier& inBarrier);
	//* Multi-threaded version of the function UpdateGame.
	void MTUpdateGame(const GameTimer& gt, UINT tid, ThreadBarrier& inBarrier);
#endif 
	//* Rendering system wiill draw all rendering items.
	void Draw(const GameTimer& gt);

	//* Initializes main window.
	bool InitMainWindow();
	//* Reapply rendering system when window is resized.
	void OnResize();
	//* Displays the frame rate and the frame time in the caption bar.
	void CalculateFrameStats();

	//
	//* Call-back functions for mouse state.
	//

	void OnMouseDown(WPARAM inBtnState, int inX, int inY);
	void OnMouseUp(WPARAM inBtnState, int inX, int inY);
	void OnMouseMove(WPARAM inBtnState, int inX, int inY);

	//
	//* Call-back functions for keyboard state.
	//

	void OnKeyboardInput(UINT msg, WPARAM wParam, LPARAM lParam);

private:
	static GameWorld* mWorld;

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
	
	int mClientWidth = 800;
	int mClientHeight = 600;

	int mPrimaryMonitorWidth = 0;
	int mPrimaryMonitorHeight = 0;

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

	UINT mVCount = 0;
};