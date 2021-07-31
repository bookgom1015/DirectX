#include "DX12Game/GameWorld.h"
#ifndef UsingVulkan
	#include "DX12Game/DxRenderer.h"	
#else
	#include "DX12Game/VkRenderer.h"
#endif
#include "DX12Game/AudioSystem.h"
#include "DX12Game/InputSystem.h"
#include "DX12Game/GameCamera.h"
#include "DX12Game/Mesh.h"
#include "DX12Game/SkeletalMeshComponent.h"
#include "DX12Game/FpsActor.h"
#include "DX12Game/TpsActor.h"

using namespace DirectX;
using namespace DirectX::PackedVector;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd) {
	// Enable run-time memory check for debug builds
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF || _CRTDBG_LEAK_CHECK_DF);
#endif

	try {
		GameWorld game(hInstance);

		GameResult result = game.Initialize(1600, 900);
		if (result.hr != S_OK) {
			WLogln(result.msg.c_str());
			return static_cast<int>(result.hr);
		}

		int status = game.RunLoop();

		game.CleanUp();

		return status;
	}
	catch (std::exception& e) {
		std::wstringstream wsstream;
		wsstream << e.what();
		WErrln(wsstream.str());
		return -1;
	}
}

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	// Forward hwnd on because we can get messages (e.g., WM_CREATE)
	// before CreateWindow returns, and thus before mhMainWnd is valid
	return GameWorld::GetWorld()->MsgProc(hwnd, msg, wParam, lParam);
}

GameWorld* GameWorld::sWorld = nullptr;

GameWorld::GameWorld(HINSTANCE hInstance) 
	: mhInst(hInstance) {
	if (sWorld == nullptr)
		sWorld = this;

#ifdef UsingVulkan
	mRenderer = std::make_unique<VkRenderer>();
#else
	mRenderer = std::make_unique<DxRenderer>();
#endif
	mAudioSystem = std::make_unique<AudioSystem>();
	mInputSystem = std::make_unique<InputSystem>();
}

GameWorld::~GameWorld() {
	if (!bIsCleaned)
		CleanUp();
}

GameResult GameWorld::Initialize(INT inWidth /* = 800 */, UINT inHeight /* = 600 */) {
	mClientWidth = inWidth;
	mClientHeight = inHeight;
	
	CheckGameResult(InitMainWindow());
	
#ifdef UsingVulkan
	CheckGameResult(mRenderer->Initialize(mMainGLFWWindow, mClientWidth, mClientHeight));
#else
	CheckGameResult(mRenderer->Initialize(mhMainWnd, mClientWidth, mClientHeight));
#endif
	
	if (!mAudioSystem->Initialize())
		ReturnGameResult(S_FALSE, L"Failed to initialize AudioSystem");
	mAudioSystem->SetBusVolume("bus:/", 0.0f);
	
	if (!mInputSystem->Initialize(mhMainWnd))
		ReturnGameResult(S_FALSE, L"Failed to initialize InputSystem");

	mLimitFrameRate = GameTimer::LimitFrameRate::ELimitFrameRateNone;
	mTimer.SetLimitFrameRate(mLimitFrameRate);

#ifdef MT_World
	size_t numProcessors = (size_t)ThreadUtil::GetNumberOfProcessors();
	mThreads.resize(numProcessors);
	mMTActors.resize(numProcessors);
	mMTPendingActors.resize(numProcessors);
	bMTUpdatingActors.resize(numProcessors);
#endif 

	bFinishedInit = true;

	return GameResult(S_OK);
}

void GameWorld::CleanUp() {
	if (mInputSystem != nullptr)
		mInputSystem->CleanUp();
	if (mAudioSystem != nullptr)
		mAudioSystem->CleanUp();
	if (mRenderer != nullptr)
		mRenderer->CleanUp();

#ifdef UsingVulkan
	glfwDestroyWindow(mMainGLFWWindow);

	glfwTerminate();
#endif

	bIsCleaned = true;
}

bool GameWorld::LoadData() {
	mMusicEvent = mAudioSystem->PlayEvent("event:/Over the Waves");

#ifndef UsingVulkan
	TpsActor* tpsActor = new TpsActor();
	tpsActor->SetPosition(0.0f, 0.0f, -5.0f);
	
	XMVECTOR rotateYPi = XMQuaternionRotationAxis(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), XM_PI);
	
	Actor* monkeyActor = new Actor();
	monkeyActor->SetPosition(0.0f, 4.0f, 0.0f);
	monkeyActor->SetQuaternion(rotateYPi);
	std::function<void(const GameTimer&, Actor*)> funcTurnY = [](const GameTimer& gt, Actor* actor) -> void {
		XMVECTOR rot = XMQuaternionRotationAxis(XMVector4Normalize(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)), gt.TotalTime() * 8.0f);
		actor->SetQuaternion(rot);
	};
	monkeyActor->AddFunction(std::make_shared<std::function<void(const GameTimer&, Actor*)>>(funcTurnY));
	MeshComponent* monkeyMeshComp = new MeshComponent(monkeyActor);
	if (!monkeyMeshComp->LoadMesh("monkey", "monkey.fbx")) 
		return false;
	
	monkeyActor = new Actor();
	monkeyActor->SetQuaternion(rotateYPi);
	monkeyMeshComp = new MeshComponent(monkeyActor);
	std::function<void(const GameTimer&, Actor*)> funcTurnCCW = [](const GameTimer& gt, Actor* actor) -> void {
		XMVECTOR rot = XMQuaternionRotationAxis(XMVector4Normalize(XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f)), gt.TotalTime() * 8.0f);
		actor->SetPosition(3.0f + cosf(4.0f * gt.TotalTime()), 2.5f + sinf(4.0f * gt.TotalTime()), 0.0f);
		actor->SetQuaternion(rot);
	};
	monkeyActor->AddFunction(std::make_shared<std::function<void(const GameTimer&, Actor*)>>(funcTurnCCW));
	if (!monkeyMeshComp->LoadMesh("monkey", "monkey.fbx")) 
		return false;
	
	monkeyActor = new Actor();
	monkeyActor->SetQuaternion(rotateYPi);
	std::function<void(const GameTimer&, Actor*)> funcTurnCW = [](const GameTimer& gt, Actor* actor) -> void {
		actor->SetPosition(-3.0f + -cosf(5.0f * gt.TotalTime()), 2.5f + sinf(5.0f * gt.TotalTime()), 0.0f);
	};
	monkeyActor->AddFunction(std::make_shared<std::function<void(const GameTimer&, Actor*)>>(funcTurnCW));
	monkeyMeshComp = new MeshComponent(monkeyActor);
	if (!monkeyMeshComp->LoadMesh("monkey", "monkey.fbx")) 
		return false;
	
	Actor* leoniActor = new Actor();
	leoniActor->SetPosition(0.0f, 0.0f, -2.0f);
	leoniActor->SetQuaternion(rotateYPi);
	SkeletalMeshComponent* leoniMeshComp = new SkeletalMeshComponent(leoniActor);
	if (!leoniMeshComp->LoadMesh("leoni", "leoni.fbx", true))
		return false;
	leoniMeshComp->SetClipName("Idle");
	leoniMeshComp->SetSkeleletonVisible(false);
	
	Actor* treeActor;
	MeshComponent* treeMeshComp;
	int numTrees = 5;
	for (int i = -numTrees; i <= numTrees; ++i) {
		for (int j = -numTrees; j <= numTrees; ++j) {
			if (i == 0 && j == 0)
				continue;
	
			treeActor = new Actor();
	
			XMVECTOR pos = XMVectorSet(static_cast<float>(16 * i), 0.0f, static_cast<float>(16 * j), 1.0f);
			XMVECTOR offset = XMVectorSet(8.0f * MathHelper::RandF() - 4.0f, 0.0f, 8.0f * MathHelper::RandF() - 4.0f, 0.0f);
	
			treeActor->SetPosition(pos + offset);
			treeActor->SetQuaternion(XMQuaternionRotationAxis(
				XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), 2.0f * MathHelper::RandF() * MathHelper::Pi - MathHelper::Pi));
	
			treeMeshComp = new MeshComponent(treeActor);
			if (!treeMeshComp->LoadMesh("tree", "tree_a.fbx", true))
				return false;
		}
	}
#endif

#ifdef MT_World
	for (auto& actors : mMTActors) {
		for (auto& actor : actors)
			actor->OnLoadingData();
	}
#else
	for (auto& actor : mActors) {
		if (!actor->OnLoadingData())
			return false;
	}
#endif

	return true;
}

void GameWorld::UnloadData() {
#ifdef MT_World
	for (auto& actors : mMTActors) {
		for (auto actor : actors)
			actor->OnUnloadingData();

		while (!actors.empty())
			actors.pop_back();
	}
#else
	for (auto& actor : mActors)
		actor->OnUnloadingData();

	while (!mActors.empty())
		mActors.pop_back();
#endif
}

int GameWorld::RunLoop() {
	if (!LoadData()) {
		WErrln(L"LoadData() returned an error code");
		return -1;
	}

	GameResult resizeStatus = OnResize();
	if (resizeStatus.hr != S_OK) {
		WErrln(L"OnResize() returned an error code from RunLoop()");
		WErrln(resizeStatus.msg);
		return -1;
	}

#ifdef MT_World
	int status = MTGameLoop();
#else
	int status = GameLoop();
#endif

	UnloadData();

	return status;
}

#ifndef MT_World
int GameWorld::GameLoop() {
	float beginTime = 0.0f;
	float endTime;
	float elapsedTime;

	mTimer.Reset();

#ifndef UsingVulkan
	MSG msg = { 0 };

	while (msg.message != WM_QUIT) {
		// If there are Window messages then process them
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		// Otherwise, do animation/game stuff
		else {
			mTimer.Tick();

			endTime = mTimer.TotalTime();
			elapsedTime = endTime - beginTime;

			if (elapsedTime > mTimer.GetLimitFrameRate()) {
				beginTime = endTime;
				if (!mAppPaused) {
					CalculateFrameStats();
					ProcessInput();
				}
				UpdateGame(mTimer);
				if (!mAppPaused)
					Draw(mTimer);
			}
		}
	}

	return static_cast<int>(msg.wParam);
#else // UsingVulkan
	while (!glfwWindowShouldClose(mMainGLFWWindow)) {
		glfwPollEvents();
		if (glfwGetKey(mMainGLFWWindow, GLFW_KEY_ESCAPE))
			break;;

		mTimer.Tick();

		endTime = mTimer.TotalTime();
		elapsedTime = endTime - beginTime;

		if (elapsedTime > mTimer.GetLimitFrameRate()) {
			beginTime = endTime;
			if (!mAppPaused) {
				CalculateFrameStats();
				ProcessInput();
			}
			UpdateGame(mTimer);
			if (!mAppPaused)
				Draw(mTimer);
		}
	}

	return 0;
#endif // UsingVulkan
}
#else // MT_World
int GameWorld::MTGameLoop() {
	MSG msg = { 0 };
	float beginTime = 0.0f;
	float endTime;
	float elapsedTime;

	mTimer.Reset();

	UINT numProcessors = static_cast<UINT>(ThreadUtil::GetNumberOfProcessors());
	CVBarrier barrier(numProcessors);

	for (UINT i = 0, end = numProcessors - 1; i < end; ++i) {
		mThreads[i] = std::thread([this](const MSG& inMsg, UINT tid, ThreadBarrier& inBarrier) -> void {
			while (inMsg.message != WM_QUIT) {
				inBarrier.Wait();

				if (!mAppPaused)
					MTProcessInput(tid, std::ref(inBarrier));

				MTUpdateGame(mTimer, tid, std::ref(inBarrier));
			}
		}, std::ref(msg), i + 1, std::ref(barrier));
	}

	while (msg.message != WM_QUIT) {
		// If there are Window messages then process them
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		// Otherwise, do animation/game stuff
		else {
			mTimer.Tick();

			endTime = mTimer.TotalTime();
			elapsedTime = endTime - beginTime;

			if (elapsedTime > mTimer.GetLimitFrameRate()) {
				barrier.Wait();
				beginTime = endTime;

				if (!mAppPaused) {
					CalculateFrameStats();
					MTProcessInput(0, std::ref(barrier));
				}	

				MTUpdateGame(mTimer, 0, std::ref(barrier));

				if (!mAppPaused)
					Draw(mTimer);
			}
		}
	}

	barrier.WakeUp();

	for (UINT i = 0, end = numProcessors - 1; i < end; ++i) {
		mThreads[i].join();
	}

	return static_cast<int>(msg.wParam);
}
#endif // MT_World

void GameWorld::AddActor(Actor* inActor) {
#ifdef MT_World
	mAddingActorMutex.lock();
	inActor->SetOwnerThreadID(mNextThreadId);
	if (bMTUpdatingActors[mNextThreadId])
		mMTPendingActors[mNextThreadId].push_back(inActor);
	else
		mMTActors[mNextThreadId].push_back(inActor);
	++mNextThreadId;
	if (mNextThreadId >= mThreads.size())
		mNextThreadId = 0;
	mAddingActorMutex.unlock();
#else
	if (bUpdatingActors)
		mPendingActors.push_back(inActor);
	else
		mActors.push_back(inActor);
#endif
}

void GameWorld::RemoveActor(Actor* inActor) {
#ifdef MT_World
	UINT tid = inActor->GetOwnerThreadID();
	auto& actors = mMTActors[tid];
	auto iter = std::find(actors.begin(), actors.end(), inActor);
	if (iter != actors.end()) {
		std::iter_swap(iter, actors.end() - 1);
		actors.pop_back();
	}
#else
	auto iter = std::find(mActors.begin(), mActors.end(), inActor);
	if (iter != mActors.end()) {
		std::iter_swap(iter, mActors.end() - 1);
		mActors.pop_back();
	}
#endif
}

Mesh* GameWorld::AddMesh(const std::string& inFileName, bool inIsSkeletal, bool inNeedToBeAligned, bool bMultiThreading) {
	auto iter = mMeshes.find(inFileName);
	if (iter != mMeshes.end())
		return iter->second.get();

	auto mesh = std::make_unique<Mesh>(inIsSkeletal, inNeedToBeAligned);
	if (!mesh->Load(inFileName, bMultiThreading)) {
		WErrln(L"Failed to add mesh");
		return nullptr;
	}

	auto meshptr = mesh.get();
	mMeshes.emplace(inFileName, std::move(mesh));

	return meshptr;	
}

void GameWorld::RemoveMesh(const std::string& fileName) {
	auto iter = mMeshes.find(fileName);
	if (iter != mMeshes.end())
		mMeshes.erase(iter);
}

GameWorld* GameWorld::GetWorld() {
	return sWorld;
}

Renderer* GameWorld::GetRenderer() const {
	return mRenderer.get();
}

InputSystem* GameWorld::GetInputSystem() const {
	return mInputSystem.get();
}

UINT GameWorld::GetPrimaryMonitorWidth() const {
	return mPrimaryMonitorWidth;
}

UINT GameWorld::GetPrimaryMonitorHeight() const {
	return mPrimaryMonitorHeight;
}

UINT GameWorld::GetClientWidth() const {
	return mClientWidth;
}

UINT GameWorld::GetClientHeight() const {
	return mClientHeight;
}

HWND GameWorld::GetMainWindowsHandle() const {
	return mhMainWnd;
}

#ifndef MT_World
void GameWorld::ProcessInput() {
	mInputSystem->PrepareForUpdate();

	mInputSystem->SetRelativeMouseMode(true);

	mInputSystem->Update();
	const InputState& state = mInputSystem->GetState();

	if (mGameState == GameState::EPlay) {
		for (auto actor : mActors) {
			if (actor->GetState() == Actor::ActorState::EActive)
				actor->ProcessInput(state);
		}
	}
}

void GameWorld::UpdateGame(const GameTimer& gt) {
	// Update all actors.
	if (mGameState != GameState::EPaused) {
		bUpdatingActors = true;
		for (auto actor : mActors)
			actor->Update(gt);
		bUpdatingActors = false;

		// Move any pending actors to mActors.
		for (auto actor : mPendingActors)
			mActors.push_back(actor);

		mPendingActors.clear();

		// Add any dead actors to a temp vector.
		GVector<Actor*> deadActors;
		for (auto actor : mActors) {
			if (actor->GetState() == Actor::ActorState::EDead)
				deadActors.push_back(actor);
		}

		// Delete dead actors(which removes them from mActors).
		for (auto actor : deadActors)
			delete actor;
	}

	mRenderer->Update(gt);
	mAudioSystem->Update(gt);
}
#else
void GameWorld::MTProcessInput(UINT tid, ThreadBarrier& inBarrier) {
	if (tid == 0) {
		mInputSystem->PrepareForUpdate();
		mInputSystem->SetRelativeMouseMode(true);
		mInputSystem->Update();
	}
	inBarrier.Wait();

	const InputState& state = mInputSystem->GetState();

	if (mGameState == GameState::EPlay) {
		auto& actors = mMTActors[tid];

		for (auto& actor : actors) {
			if (actor->GetState() == Actor::ActorState::EActive)
				actor->ProcessInput(state);
		}
	}
}

void GameWorld::MTUpdateGame(const GameTimer& gt, UINT tid, ThreadBarrier& inBarrier) {
	if (mGameState != GameState::EPaused) {
		auto& actors = mMTActors[tid];
		auto& pendingActors = mMTPendingActors[tid];

		// Update all actors.
		bMTUpdatingActors[tid] = true;
		for (auto actor : actors)
			actor->Update(gt);
		bMTUpdatingActors[tid] = false;

		// Move any pending actors to mActors.
		for (auto actor : pendingActors)
			actors.push_back(actor);

		pendingActors.clear();

		// Add any dead actors to a temp vector.
		GVector<Actor*> deadActors;
		for (auto actor : actors) {
			if (actor->GetState() == Actor::ActorState::EDead)
				deadActors.push_back(actor);
		}

		// Delete dead actors(which removes them from mActors).
		for (auto actor : deadActors)
			delete actor;

		inBarrier.Wait();

		if (tid == 0) {
			mRenderer->Update(gt);
			mAudioSystem->Update(gt);
		}
	}
}
#endif

void GameWorld::Draw(const GameTimer& gt) {
	mRenderer->Draw(gt);
}

#ifdef UsingVulkan
namespace {
	void GLFWProcessInput(GLFWwindow* inMainWnd, int inKey, int inScanCode, int inAction, int inMods) {
		if (inAction == GLFW_PRESS) {
			WLogln(L"[Key callback] Pressed key: ", std::to_wstring(inKey));
		}
		else {
			WLogln(L"[Key callback] Released key: ", std::to_wstring(inKey));
		}
	}
}
#endif

GameResult GameWorld::InitMainWindow() {
#ifndef UsingVulkan
	WNDCLASS wc;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = MainWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = mhInst;
	wc.hIcon = LoadIcon(0, IDI_APPLICATION);
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
	wc.lpszMenuName = 0;
	wc.lpszClassName = L"MainWnd";

	if (!RegisterClass(&wc))
		ReturnGameResult(S_FALSE, L"RegisterClass Failed");

	// Compute window rectangle dimensions based on requested client area dimensions.
	RECT R = { 0, 0, static_cast<LONG>(mClientWidth), static_cast<LONG>(mClientHeight) };
	AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
	int width = R.right - R.left;
	int height = R.bottom - R.top;

	mPrimaryMonitorWidth = GetSystemMetrics(SM_CXSCREEN);
	mPrimaryMonitorHeight = GetSystemMetrics(SM_CYSCREEN);

	int clientPosX = static_cast<int>((mPrimaryMonitorWidth - mClientWidth) * 0.5f);
	int clientPosY = static_cast<int>((mPrimaryMonitorHeight - mClientHeight) * 0.5f);

	mhMainWnd = CreateWindow(L"MainWnd", mMainWndCaption.c_str(),
		WS_OVERLAPPEDWINDOW, clientPosX, clientPosY, width, height, 0, 0, mhInst, 0);
	if (!mhMainWnd)
		ReturnGameResult(S_FALSE, L"CreateWindow Failed");

	ShowWindow(mhMainWnd, SW_SHOW);
	UpdateWindow(mhMainWnd);
#else
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	mMainGLFWWindow = glfwCreateWindow(mClientWidth, mClientHeight, "VkGame", nullptr, nullptr);
	mhMainWnd = glfwGetWin32Window(mMainGLFWWindow);

	auto primaryMonitor = glfwGetPrimaryMonitor();
	const auto vidmode = glfwGetVideoMode(primaryMonitor);

	mPrimaryMonitorWidth = static_cast<UINT>(vidmode->width);
	mPrimaryMonitorHeight = static_cast<UINT>(vidmode->height);

	int clientPosX = static_cast<int>((mPrimaryMonitorWidth - mClientWidth) * 0.5f);
	int clientPosY = static_cast<int>((mPrimaryMonitorHeight - mClientHeight) * 0.5f);

	glfwSetWindowPos(mMainGLFWWindow, clientPosX, clientPosY);
	
	glfwSetKeyCallback(mMainGLFWWindow, GLFWProcessInput);
#endif

	return GameResult(S_OK);
}

LRESULT GameWorld::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
		// WM_ACTIVATE is sent when the window is activated or deactivated.  
		// We pause the game when the window is deactivated and unpause it 
		// when it becomes active.
	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_INACTIVE) {
			mAppPaused = true;
			mTimer.SetLimitFrameRate(GameTimer::ELimitFrameRate30f);
			mPrevBusVolume = mAudioSystem->GetBusVolume("bus:/");
			mAudioSystem->SetBusVolume("bus:/", 0.0f);
			mInputSystem->IgnoreMouseInput();
		}
		else {
			mAppPaused = false;
			mTimer.SetLimitFrameRate(mLimitFrameRate);
			mAudioSystem->SetBusVolume("bus:/", mPrevBusVolume);
			mInputSystem->IgnoreMouseInput();
		}
		return 0;

		// WM_SIZE is sent when the user resizes the window.  
	case WM_SIZE:
		// Save the new client area dimensions.
		mClientWidth = LOWORD(lParam);
		mClientHeight = HIWORD(lParam);
		{
			HRESULT hr = S_OK;
			if (mRenderer->IsValid()) {
				if (wParam == SIZE_MINIMIZED) {
					mAppPaused = true;
					mMinimized = true;
					mMaximized = false;
				}
				else if (wParam == SIZE_MAXIMIZED) {
					mAppPaused = false;
					mMinimized = false;
					mMaximized = true;
					hr = OnResize().hr;
				}
				else if (wParam == SIZE_RESTORED) {
					// Restoring from minimized state?
					if (mMinimized) {
						mAppPaused = false;
						mMinimized = false;
						hr = OnResize().hr;
					}

					// Restoring from maximized state?
					else if (mMaximized) {
						mAppPaused = false;
						mMaximized = false;
						hr = OnResize().hr;
					}
					else if (mResizing) {
						// If user is dragging the resize bars, we do not resize 
						// the buffers here because as the user continuously 
						// drags the resize bars, a stream of WM_SIZE messages are
						// sent to the window, and it would be pointless (and slow)
						// to resize for each WM_SIZE message received from dragging
						// the resize bars.  So instead, we reset after the user is 
						// done resizing the window and releases the resize bars, which 
						// sends a WM_EXITSIZEMOVE message.
					}
					else { // API call such as SetWindowPos or mSwapChain->SetFullscreenState.
						hr = OnResize().hr;
					}
				}
			}
			return hr;
		}

		// WM_EXITSIZEMOVE is sent when the user grabs the resize bars.
	case WM_ENTERSIZEMOVE:
		mAppPaused = true;
		mResizing = true;
		mTimer.Stop();
		return 0;

		// WM_EXITSIZEMOVE is sent when the user releases the resize bars.
		// Here we reset everything based on the new window dimensions.
	case WM_EXITSIZEMOVE:
		mAppPaused = false;
		mResizing = false;
		mTimer.Start();
		return OnResize().hr;

		// WM_DESTROY is sent when the window is being destroyed.
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

		// The WM_MENUCHAR message is sent when a menu is active and the user presses 
		// a key that does not correspond to any mnemonic or accelerator key. 
	case WM_MENUCHAR:
		// Don't beep when we alt-enter.
		return MAKELRESULT(0, MNC_CLOSE);

		// Catch this message so to prevent the window from becoming too small.
	case WM_GETMINMAXINFO:
		(reinterpret_cast<MINMAXINFO*>(lParam))->ptMinTrackSize.x = 200;
		(reinterpret_cast<MINMAXINFO*>(lParam))->ptMinTrackSize.y = 200;
		return 0;

	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;

	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;

	case WM_MOUSEMOVE:
		OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;

	case WM_MOUSEWHEEL:
		if (static_cast<SHORT>(HIWORD(wParam)) > 0)
			mInputSystem->OnWheelUp();
		else
			mInputSystem->OnWheelDown();
		return 0;

	case WM_KEYUP:
		OnKeyboardInput(WM_KEYUP, wParam, lParam);
		return 0;

	case WM_KEYDOWN:
		OnKeyboardInput(WM_KEYDOWN, wParam, lParam);
		return 0;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

GameResult GameWorld::OnResize() {
	if (bFinishedInit)
		CheckGameResult(mRenderer->OnResize(mClientWidth, mClientHeight));

	return GameResult(S_OK);
}

void GameWorld::CalculateFrameStats() {
	// Code computes the average frames per second, and also 
	// the average time it takes to render one frame
	// These stats are appended to the window caption bar
	static int frameCnt = 0;
	static float timeElapsed = 0.0f;

	frameCnt++;

	// Compute averages over one second period
	if (mTimer.TotalTime() - timeElapsed >= 0.05f) {
		float fps = static_cast<float>(frameCnt); // fps = frameCnt / 1
		float mspf = 1000.0f / fps;

		mRenderer->AddOutputText(
			"TEXT_FPS",
			L"fps: " + std::to_wstring(fps),
			static_cast<float>(mClientWidth * 0.01f),
			static_cast<float>(mClientHeight * 0.01f),
			16.0f
		);

		mRenderer->AddOutputText(
			"TEXT_MSPF",
			L"mspf: " + std::to_wstring(mspf),
			static_cast<float>(mClientWidth * 0.01f),
			static_cast<float>(mClientHeight * 0.05f),
			16.0f	
		);

		// Reset for next average
		frameCnt = 0;
		timeElapsed += 1.0f;
	}
}

void GameWorld::OnMouseDown(WPARAM inBtnState, int inX, int inY) {}

void GameWorld::OnMouseUp(WPARAM inBtnState, int inX, int inY) {}

void GameWorld::OnMouseMove(WPARAM inBtnState, int inX, int inY) {}

void GameWorld::OnKeyboardInput(UINT msg, WPARAM wParam, LPARAM lParam) {
	if (msg != WM_KEYUP) {
		if (wParam == VK_OEM_PLUS) {
			// Increase master volume
			float volume = mAudioSystem->GetBusVolume("bus:/");
			volume += 0.1f;
			if (volume > 1.0f) volume = 1.0f;
			mAudioSystem->SetBusVolume("bus:/", volume);
		}
		else if (wParam == VK_OEM_MINUS) {
			// Reduce master volume
			float volume = mAudioSystem->GetBusVolume("bus:/");
			volume -= 0.1f;
			if (volume < 0.0f) volume = 0.0f;
			mAudioSystem->SetBusVolume("bus:/", volume);
		}
	}
	else {
		if (wParam == VK_ESCAPE)
			PostQuitMessage(0);
	}
}