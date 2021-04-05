#include "DX12Game/GameCore.h"
#include "DX12Game/GameWorld.h"
#include "DX12Game/Renderer.h"
#include "DX12Game/AudioSystem.h"
#include "DX12Game/InputSystem.h"
#include "DX12Game/GameCamera.h"
#include "DX12Game/FpsActor.h"
#include "DX12Game/SkeletalMeshComponent.h"
#include "DX12Game/Mesh.h"
#include "DX12Game/TpsActor.h"

using namespace DirectX;
using namespace DirectX::PackedVector;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd) {
	// Enable run-time memory check for debug builds
#if defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF || _CRTDBG_LEAK_CHECK_DF);
#endif

	try {
		GameWorld game(hInstance);
	
		if (!game.Initialize())
			return 0;

		int status = game.RunLoop();

		return status;
	}
	catch (DxException& e) {
		std::wstringstream wsstream;
		wsstream << L"Error Code: 0x" << std::hex << e.ErrorCode << std::endl << e.ToString();
		std::wstring wstr = wsstream.str();
		MessageBox(nullptr, wstr.c_str(), L"HR Failed", MB_OK);
		return 0;
	}
	catch (std::exception& e) {
		std::wstringstream wsstream;
		wsstream << e.what();
		MessageBox(nullptr, wsstream.str().c_str(), L"HR Failed", MB_OK);
		return 0;
	}
}

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	// Forward hwnd on because we can get messages (e.g., WM_CREATE)
	// before CreateWindow returns, and thus before mhMainWnd is valid
	return GameWorld::GetWorld()->MsgProc(hwnd, msg, wParam, lParam);
}

GameWorld* GameWorld::mWorld = nullptr;

GameWorld::GameWorld(HINSTANCE hInstance) 
	: mhInst(hInstance) {
	if (mWorld == nullptr)
		mWorld = this;

	mRenderer = std::make_unique<Renderer>();
	mAudioSystem = std::make_unique<AudioSystem>();
	mInputSystem = std::make_unique<InputSystem>();
}

GameWorld::~GameWorld() {}

bool GameWorld::Initialize() {
	if (!InitMainWindow())
		return false;
	
	if (!mRenderer->Initialize(mhMainWnd, mClientWidth, mClientHeight))
		return false;

	if (!mAudioSystem->Initialize())
		return false;
	mAudioSystem->SetBusVolume("bus:/", 0.0f);

	if (!mInputSystem->Initialize(mhMainWnd))
		return false;

	mTimer.SetLimitFrameRate(GameTimer::LimitFrameRate::ELimitFrameRateNone);

#if defined(MT_World)
	size_t numProcessors = (size_t)ThreadUtil::GetNumberOfProcessors();
	mThreads.resize(numProcessors);
	mMTActors.resize(numProcessors);
	mMTPendingActors.resize(numProcessors);
	bMTUpdatingActors.resize(numProcessors);
#endif

	return true;
}

bool GameWorld::LoadData() {
	mMusicEvent = mAudioSystem->PlayEvent("event:/Over the Waves");

	//FpsActor* fpsActor = new FpsActor();
	//fpsActor->SetPosition(0.0f, 0.0f, -5.0f);
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

	Actor* treeActor;
	MeshComponent* treeMeshComp;
	for (int i = -1; i <= 1; ++i) {
		for (int j = -1; j <= 1; ++j) {
			if (i == 0 && j == 0)
				continue;

			treeActor = new Actor();

			XMVECTOR pos = XMVectorSet(static_cast<float>(12 * i), 0.0f, static_cast<float>(12 * j), 1.0f);
			XMVECTOR offset = XMVectorSet(16.0f * MathHelper::RandF() - 8.0f, 0.0f, 16.0f * MathHelper::RandF() - 8.0f, 0.0f);

			treeActor->SetPosition(pos + offset);
			treeActor->SetQuaternion(XMQuaternionRotationAxis(
				XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), 2.0f * MathHelper::RandF() * MathHelper::Pi - MathHelper::Pi));

			treeMeshComp = new MeshComponent(treeActor);
			if (!treeMeshComp->MTLoadMesh("tree", "tree_1.fbx"))
				return false;
		}
	}

#if defined(MT_World)
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
#if defined(MT_World)
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
	if (!LoadData())
		return -1;

	OnResize();

#if defined(MT_World)
	int status = MTGameLoop();
#else
	int status = GameLoop();
#endif

	UnloadData();

	return status;
}

#if !defined(MT_World)
int GameWorld::GameLoop() {
	MSG msg = { 0 };
	float beginTime = 0.0f;
	float endTime;
	float elapsedTime;

	mTimer.Reset();

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

	return (int)msg.wParam;
}
#else
int GameWorld::MTGameLoop() {
	MSG msg = { 0 };
	float beginTime = 0.0f;
	float endTime;
	float elapsedTime;

	mTimer.Reset();

	UINT numProcessors = (UINT)ThreadUtil::GetNumberOfProcessors();
	CVBarrier barrier(numProcessors);

	for (UINT i = 0, end = numProcessors - 1; i < end; ++i) {
		mThreads[i] = std::thread([this](const MSG& inMsg, UINT tid, const float& inElapsedTime, ThreadBarrier& inBarrier) -> void {
			while (inMsg.message != WM_QUIT) {
				inBarrier.Wait();

				if (inElapsedTime > mTimer.GetLimitFrameRate()) {
					if (!mAppPaused)
						MTProcessInput(tid, std::ref(inBarrier));
					MTUpdateGame(mTimer, tid, std::ref(inBarrier));
				}

				inBarrier.Wait();
			}
		}, std::ref(msg), i + 1, std::ref(elapsedTime), std::ref(barrier));
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

			barrier.Wait();

			if (elapsedTime > mTimer.GetLimitFrameRate()) {
				beginTime = endTime;
				if (!mAppPaused) {
					CalculateFrameStats();
					MTProcessInput(0, std::ref(barrier));
				}
				MTUpdateGame(mTimer, 0, std::ref(barrier));
				if (!mAppPaused)
					Draw(mTimer);
			}

			barrier.Wait();
		}
	}

	barrier.WakeUp();

	for (UINT i = 0, end = numProcessors - 1; i < end; ++i)
		mThreads[i].join();

	return (int)msg.wParam;
}
#endif

void GameWorld::AddActor(Actor* inActor) {
#if defined(MT_World)
	mAddingActorMutex.lock();
	inActor->SetOwnerThreadID(mNextThreadId);
	if (bMTUpdatingActors[mNextThreadId])
		mMTPendingActors[mNextThreadId].push_back(inActor);
	else
		mMTActors[mNextThreadId].push_back(inActor);
	++mNextThreadId;
	mAddingActorMutex.unlock();
#else
	if (bUpdatingActors)
		mPendingActors.push_back(inActor);
	else
		mActors.push_back(inActor);
#endif
}

void GameWorld::RemoveActor(Actor* inActor) {
#if defined(MT_World)
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

Mesh* GameWorld::AddMesh(const std::string& inFileName, bool inIsSkeletal, bool inNeedToBeAligned) {
	auto iter = mMeshes.find(inFileName);
	if (iter != mMeshes.end())
		return iter->second.get();

	auto mesh = std::make_unique<Mesh>(inIsSkeletal, inNeedToBeAligned);
	if (!mesh->Load(inFileName))
		return nullptr;

	auto meshptr = mesh.get();
	mMeshes.emplace(inFileName, std::move(mesh));

	return meshptr;	
}

Mesh* GameWorld::MTAddMesh(const std::string& inFileName, bool inIsSkeletal, bool inNeedToBeAligned) {
	auto iter = mMeshes.find(inFileName);
	if (iter != mMeshes.end())
		return iter->second.get();

	auto mesh = std::make_unique<Mesh>(inIsSkeletal, inNeedToBeAligned);
	if (!mesh->MTLoad(inFileName))
		return nullptr;

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
	return mWorld;
}

Renderer* GameWorld::GetRenderer() const {
	return mRenderer.get();
}

InputSystem* GameWorld::GetInputSystem() const {
	return mInputSystem.get();
}

int GameWorld::GetPrimaryMonitorWidth() const {
	return mPrimaryMonitorWidth;
}

int GameWorld::GetPrimaryMonitorHeight() const {
	return mPrimaryMonitorHeight;
}

HWND GameWorld::GetMainWindowsHandle() const {
	return mhMainWnd;
}

#if !defined(MT_World)
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
		std::vector<Actor*> deadActors;
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
		std::vector<Actor*> deadActors;
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

bool GameWorld::InitMainWindow() {
	WNDCLASS wc;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = MainWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = mhInst;
	wc.hIcon = LoadIcon(0, IDI_APPLICATION);
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	wc.lpszMenuName = 0;
	wc.lpszClassName = L"MainWnd";

	if (!RegisterClass(&wc)) {
		MessageBox(0, L"RegisterClass Failed", 0, 0);
		return false;
	}

	// Compute window rectangle dimensions based on requested client area dimensions.
	RECT R = { 0, 0, mClientWidth, mClientHeight };
	AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
	int width = R.right - R.left;
	int height = R.bottom - R.top;

	mPrimaryMonitorWidth = GetSystemMetrics(SM_CXSCREEN);
	mPrimaryMonitorHeight = GetSystemMetrics(SM_CYSCREEN);

	int clientPosX = static_cast<int>((mPrimaryMonitorWidth - mClientWidth) * 0.5f);
	int clientPosY = static_cast<int>((mPrimaryMonitorHeight - mClientHeight) * 0.5f);

	mhMainWnd = CreateWindow(L"MainWnd", mMainWndCaption.c_str(),
		WS_OVERLAPPEDWINDOW, clientPosX, clientPosY, width, height, 0, 0, mhInst, 0);
	if (!mhMainWnd) {
		MessageBox(0, L"CreateWindow Failed", 0, 0);
		return false;
	}

	ShowWindow(mhMainWnd, SW_SHOW);
	UpdateWindow(mhMainWnd);

	return true;
}

LRESULT GameWorld::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
		// WM_ACTIVATE is sent when the window is activated or deactivated.  
		// We pause the game when the window is deactivated and unpause it 
		// when it becomes active.
	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_INACTIVE) {
			mAppPaused = true;
			//mTimer.Stop();
			mPrevBusVolume = mAudioSystem->GetBusVolume("bus:/");
			mAudioSystem->SetBusVolume("bus:/", 0.0f);
			mInputSystem->IgnoreMouseInput();
		}
		else {
			mAppPaused = false;
			//mTimer.Start();
			mAudioSystem->SetBusVolume("bus:/", mPrevBusVolume);
			mInputSystem->IgnoreMouseInput();
		}
		return 0;

		// WM_SIZE is sent when the user resizes the window.  
	case WM_SIZE:
		// Save the new client area dimensions.
		mClientWidth = LOWORD(lParam);
		mClientHeight = HIWORD(lParam);
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
				OnResize();
			}
			else if (wParam == SIZE_RESTORED) {

				// Restoring from minimized state?
				if (mMinimized) {
					mAppPaused = false;
					mMinimized = false;
					OnResize();
				}

				// Restoring from maximized state?
				else if (mMaximized) {
					mAppPaused = false;
					mMaximized = false;
					OnResize();
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
					OnResize();
				}
			}
		}
		return 0;

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
		OnResize();
		return 0;

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
		((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
		((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
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

	case WM_KEYUP:
		OnKeyboardInput(WM_KEYUP, wParam, lParam);
		return 0;

	case WM_KEYDOWN:
		OnKeyboardInput(WM_KEYDOWN, wParam, lParam);
		return 0;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

void GameWorld::SetVCount(UINT inCount) {
	mVCount = inCount;
}

void GameWorld::OnResize() {
	mRenderer->OnResize(mClientWidth, mClientHeight);
}

void GameWorld::CalculateFrameStats() {
	// Code computes the average frames per second, and also 
	// the average time it takes to render one frame
	// These stats are appended to the window caption bar
	static int frameCnt = 0;
	static float timeElapsed = 0.0f;

	frameCnt++;

	// Compute averages over one second period
	if (mTimer.TotalTime() - timeElapsed >= 1.0f) {
		float fps = static_cast<float>(frameCnt); // fps = frameCnt / 1
		float mspf = 1000.0f / fps;

		std::wstring fpsStr = std::to_wstring(fps);
		std::wstring mspfStr = std::to_wstring(mspf);

		std::wstring windowText = mMainWndCaption +
			L"    fps: " + fpsStr +
			L"   mspf: " + mspfStr +
			L"    voc: " + std::to_wstring(mVCount);
		SetWindowText(mhMainWnd, windowText.c_str());

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