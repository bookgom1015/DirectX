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
		if (result.hr != S_OK) return static_cast<int>(result.hr);

		result = game.RunLoop();
		if (result.hr != S_OK) {
			GameResult reason = game.GetRenderer()->GetDeviceRemovedReason();
			std::wstringstream wsstream;
			wsstream << L"Device removed reason code: 0x" << std::hex << reason.hr;
			WLogln(wsstream.str());
			return static_cast<int>(result.hr);
		}

		game.CleanUp();
		WLogln(L"The game has been succeesfully cleaned up");
		
		return static_cast<int>(result.hr);
	}
	catch (std::exception& e) {
		std::wstringstream wsstream;
		wsstream << e.what();
		WErrln(wsstream.str());
		WLogln(L"Catched");
		return -1;
	}
}

namespace {
#ifdef UsingVulkan
	void GLFWProcessInputCallback(GLFWwindow* inMainWnd, int inKey, int inScanCode, int inAction, int inMods) {
		auto world = reinterpret_cast<GameWorld*>(glfwGetWindowUserPointer(inMainWnd));
		world->MsgProc(inMainWnd, inKey, inScanCode, inAction, inMods);
	}

	void GLFWResizeCallback(GLFWwindow* inMainWnd, int inWidth, int inHeight) {
		auto world = reinterpret_cast<GameWorld*>(glfwGetWindowUserPointer(inMainWnd));
		world->GetRenderer()->OnResize(inWidth, inHeight);
	}

	void GLFWFocusCallback(GLFWwindow* inMainWnd, int inState) {
		auto world = reinterpret_cast<GameWorld*>(glfwGetWindowUserPointer(inMainWnd));
		world->OnWindowFocusChanged(static_cast<bool>(inState));
	}

	void MainWndProc(GLFWwindow* inMainWnd, int inKey, int inScanCode, int inAction, int inMods) {
		return GameWorld::GetWorld()->MsgProc(inMainWnd, inKey, inScanCode, inAction, inMods);
	}
#else
	LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		// Forward hwnd on because we can get messages (e.g., WM_CREATE)
		// before CreateWindow returns, and thus before mhMainWnd is valid
		return GameWorld::GetWorld()->MsgProc(hwnd, msg, wParam, lParam);
	}
#endif
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

	if (!ThreadUtil::Initialize())
		ReturnGameResult(S_FALSE, L"Failed to initialize ThreadUtil");

	mNumProcessors = ThreadUtil::GetProcessorCount(false);

	mThreads.resize(mNumProcessors);
	mActors.resize(mNumProcessors);
	mPendingActors.resize(mNumProcessors);
	bUpdatingActors.resize(mNumProcessors, false);

	mCVBarrier = std::make_unique<CVBarrier>(mNumProcessors);
	mSpinlockBarrier = std::make_unique<SpinlockBarrier>(mNumProcessors);

#ifdef UsingVulkan
	CheckGameResult(mRenderer->Initialize(mClientWidth, mClientHeight, mNumProcessors, mMainGLFWWindow, nullptr, nullptr));
#else
	CheckGameResult(mRenderer->Initialize(mClientWidth, mClientHeight, mNumProcessors, mhMainWnd, mCVBarrier.get(), mSpinlockBarrier.get()));
#endif

	if (!mAudioSystem->Initialize())
		ReturnGameResult(S_FALSE, L"Failed to initialize AudioSystem");
	mAudioSystem->SetBusVolume("bus:/", 0.0f);
	
	if (!mInputSystem->Initialize(mhMainWnd))
		ReturnGameResult(S_FALSE, L"Failed to initialize InputSystem");

	mPerfAnalyzer.Initialize(mRenderer.get(), mNumProcessors);

	mLimitFrameRate = GameTimer::LimitFrameRate::ELimitFrameRateNone;
	mTimer.SetLimitFrameRate(mLimitFrameRate);

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

GameResult GameWorld::LoadData() {
	mMusicEvent = mAudioSystem->PlayEvent("event:/Over the Waves");

#ifdef UsingVulkan
	//TpsActor* tpsActor = new TpsActor();
	//tpsActor->SetPosition(0.0f, 0.0f, -5.0f);

	XMVECTOR rotateYPi = XMQuaternionRotationAxis(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), XM_PI);

	//Actor* leoniActor = new Actor();
	//leoniActor->SetPosition(0.0f, 0.0f, -2.0f);
	//leoniActor->SetQuaternion(rotateYPi);
	//SkeletalMeshComponent* leoniMeshComp = new SkeletalMeshComponent(leoniActor);
	//CheckGameResult(leoniMeshComp->LoadMesh("leoni", "leoni.fbx"));
	//leoniMeshComp->SetClipName("Idle");
	//leoniMeshComp->SetSkeleletonVisible(false);

	Actor* monkeyActor = new Actor();
	monkeyActor->SetPosition(0.0f, 4.0f, 0.0f);
	monkeyActor->SetQuaternion(rotateYPi);
	std::function<void(const GameTimer&, Actor*)> funcTurnY = [](const GameTimer& gt, Actor* actor) -> void {
		XMVECTOR rot = XMQuaternionRotationAxis(XMVector4Normalize(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)), gt.TotalTime() * 2.0f);
		actor->SetQuaternion(rot);
	};
	monkeyActor->AddFunction(std::make_shared<std::function<void(const GameTimer&, Actor*)>>(funcTurnY));
	MeshComponent* monkeyMeshComp = new MeshComponent(monkeyActor);
	CheckGameResult(monkeyMeshComp->LoadMesh("monkey", "monkey.fbx"));
#endif

#ifndef UsingVulkan	
	TpsActor* tpsActor = new TpsActor();
	tpsActor->SetPosition(0.0f, 0.0f, -5.0f);

	XMVECTOR rotateYPi = XMQuaternionRotationAxis(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), XM_PI);

	Actor* monkeyActor = new Actor();
	monkeyActor->SetPosition(0.0f, 4.0f, 0.0f);
	monkeyActor->SetQuaternion(rotateYPi);
	std::function<void(const GameTimer&, Actor*)> funcTurnY = [](const GameTimer& gt, Actor* actor) -> void {
		XMVECTOR rot = XMQuaternionRotationAxis(XMVector4Normalize(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)), gt.TotalTime() * 2.0f);
		actor->SetQuaternion(rot);
	};
	monkeyActor->AddFunction(std::make_shared<std::function<void(const GameTimer&, Actor*)>>(funcTurnY));
	MeshComponent* monkeyMeshComp = new MeshComponent(monkeyActor);
	CheckGameResult(monkeyMeshComp->LoadMesh("monkey", "monkey.fbx"));
	
	monkeyActor = new Actor();
	monkeyActor->SetQuaternion(rotateYPi);
	monkeyMeshComp = new MeshComponent(monkeyActor);
	std::function<void(const GameTimer&, Actor*)> funcTurnCCW = [](const GameTimer& gt, Actor* actor) -> void {
		XMVECTOR rot = XMQuaternionRotationAxis(XMVector4Normalize(XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f)), gt.TotalTime() * 2.0f);
		actor->SetPosition(3.0f + cosf(2.0f * gt.TotalTime()), 2.5f + sinf(2.0f * gt.TotalTime()), 0.0f);
		actor->SetQuaternion(rot);
	};
	monkeyActor->AddFunction(std::make_shared<std::function<void(const GameTimer&, Actor*)>>(funcTurnCCW));
	CheckGameResult(monkeyMeshComp->LoadMesh("monkey", "monkey.fbx"));
	
	monkeyActor = new Actor();
	monkeyActor->SetQuaternion(rotateYPi);
	std::function<void(const GameTimer&, Actor*)> funcTurnCW = [](const GameTimer& gt, Actor* actor) -> void {
		actor->SetPosition(-3.0f + -cosf(2.0f * gt.TotalTime()), 2.5f + sinf(2.0f * gt.TotalTime()), 0.0f);
	};
	monkeyActor->AddFunction(std::make_shared<std::function<void(const GameTimer&, Actor*)>>(funcTurnCW));
	monkeyMeshComp = new MeshComponent(monkeyActor);
	CheckGameResult(monkeyMeshComp->LoadMesh("monkey", "monkey.fbx"));
	
	Actor* leoniActor = new Actor();
	leoniActor->SetPosition(0.0f, 0.0f, -2.0f);
	leoniActor->SetQuaternion(rotateYPi);
	SkeletalMeshComponent* leoniMeshComp = new SkeletalMeshComponent(leoniActor);
	CheckGameResult(leoniMeshComp->LoadMesh("leoni", "leoni.fbx"));
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
			CheckGameResult(treeMeshComp->LoadMesh("tree", "tree_a.fbx"));
		}
	}

	Actor* grassoneActor;
	MeshComponent* grassoneMeshComp;
	int numGrass = 5;
	for (int i = -numGrass; i <= numGrass; ++i) {
		for (int j = -numGrass; j <= numGrass; ++j) {
			if (i == 0 && j == 0)
				continue;

			grassoneActor = new Actor();

			XMVECTOR pos = XMVectorSet(
				static_cast<float>(5 * i), 0.0f,
				static_cast<float>(5 * j), 1.0f
			);
			XMVECTOR offset = XMVectorSet(
				3.0f * MathHelper::RandF() - 1.5f,
				0.0f,
				3.0f * MathHelper::RandF() - 1.5f,
				0.0f
			);

			grassoneActor->SetPosition(pos + offset);
			grassoneActor->SetQuaternion(XMQuaternionRotationAxis(
				XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),
				2.0f * MathHelper::RandF() * MathHelper::Pi - MathHelper::Pi)
			);

			grassoneMeshComp = new MeshComponent(grassoneActor);
			CheckGameResult(grassoneMeshComp->LoadMesh("grassone", "grass_variant_1.fbx"));
		}
	}

	Actor* grasstwoActor;
	MeshComponent* grasstwoMeshComp;
	for (int i = -numGrass; i <= numGrass; ++i) {
		for (int j = -numGrass; j <= numGrass; ++j) {
			if (i == 0 && j == 0)
				continue;

			grasstwoActor = new Actor();

			XMVECTOR pos = XMVectorSet(
				static_cast<float>(4.5 * i), 0.0f,
				static_cast<float>(4.5 * j), 1.0f
			);
			XMVECTOR offset = XMVectorSet(
				3.0f * MathHelper::RandF() - 1.5f,
				0.0f,
				3.0f * MathHelper::RandF() - 1.5f,
				0.0f
			);

			grasstwoActor->SetPosition(pos + offset);
			grasstwoActor->SetQuaternion(XMQuaternionRotationAxis(
				XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),
				2.0f * MathHelper::RandF() * MathHelper::Pi - MathHelper::Pi)
			);

			grasstwoMeshComp = new MeshComponent(grasstwoActor);
			CheckGameResult(grasstwoMeshComp->LoadMesh("grasstwo", "grass_variant_2.fbx"));
		}
	}

	Actor* grassthreeActor;
	MeshComponent* grassthreeMeshComp;
	for (int i = -numGrass; i <= numGrass; ++i) {
		for (int j = -numGrass; j <= numGrass; ++j) {
			if (i == 0 && j == 0)
				continue;

			grassthreeActor = new Actor();

			XMVECTOR pos = XMVectorSet(
				static_cast<float>(6.0 * i), 0.0f,
				static_cast<float>(6.0 * j), 1.0f
			);
			XMVECTOR offset = XMVectorSet(
				3.0f * MathHelper::RandF() - 1.5f,
				0.0f,
				3.0f * MathHelper::RandF() - 1.5f,
				0.0f
			);

			grassthreeActor->SetPosition(pos + offset);
			grassthreeActor->SetQuaternion(XMQuaternionRotationAxis(
				XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),
				2.0f * MathHelper::RandF() * MathHelper::Pi - MathHelper::Pi)
			);

			grassthreeMeshComp = new MeshComponent(grassthreeActor);
			CheckGameResult(grassthreeMeshComp->LoadMesh("grassthree", "grass_variant_3.fbx"));
		}
	}

	Actor* grassfourActor;
	MeshComponent* grassfourMeshComp;
	for (int i = -numGrass; i <= numGrass; ++i) {
		for (int j = -numGrass; j <= numGrass; ++j) {
			if (i == 0 && j == 0)
				continue;

			grassfourActor = new Actor();

			XMVECTOR pos = XMVectorSet(
				static_cast<float>(4.0 * i), 0.0f,
				static_cast<float>(4.0 * j), 1.0f
			);
			XMVECTOR offset = XMVectorSet(
				2.0f * MathHelper::RandF() - 0.5f,
				0.0f,
				2.0f * MathHelper::RandF() - 0.5f,
				0.0f
			);

			grassfourActor->SetPosition(pos + offset);
			grassfourActor->SetQuaternion(XMQuaternionRotationAxis(
				XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),
				2.0f * MathHelper::RandF() * MathHelper::Pi - MathHelper::Pi)
			);

			grassfourMeshComp = new MeshComponent(grassfourActor);
			CheckGameResult(grassfourMeshComp->LoadMesh("grassfour", "grass_variant_4.fbx"));
		}
	}
#endif // UsingVulkan

	for (auto& actors : mActors) {
		for (auto& actor : actors)
			CheckGameResult(actor->OnLoadingData());
	}

	Logln("End LoadData");
	return GameResultOk;
}

void GameWorld::UnloadData() {
	for (auto& actors : mActors) {
		for (auto actor : actors)
			actor->OnUnloadingData();
	
		while (!actors.empty())
			actors.pop_back();
	}
}

GameResult GameWorld::RunLoop() {
	CheckGameResult(LoadData());
	CheckGameResult(OnResize());

	GameResult result = GameLoop();

	UnloadData();

	return result;
}

GameResult GameWorld::GameLoop() {
	MSG msg = { 0 };
	float beginTime = 0.0f;
	float endTime;
	float elapsedTime;

	mTimer.Reset();
	
#ifdef UsingVulkan
	while (!glfwWindowShouldClose(mMainGLFWWindow) && mGameState != GameState::ETerminated) {
		glfwPollEvents();

		mTimer.Tick();

		endTime = mTimer.TotalTime();
		elapsedTime = endTime - beginTime;

		if (elapsedTime > mTimer.GetLimitFrameRate()) {
			beginTime = endTime;

			if (!mAppPaused) {
				ProcessInput(mTimer);
			}

			UpdateGame(mTimer);

			if (!mAppPaused)
				Draw(mTimer);
		}
	}
#else // UsingVulkan
	CVBarrier barrier(mNumProcessors);

	for (UINT i = 0, end = mNumProcessors - 1; i < end; ++i) {
		mThreads[i] = std::thread([this](const MSG& inMsg, UINT inTid, ThreadBarrier& inBarrier) -> void {
			try {
				while (mGameState != GameState::ETerminated) {
					mPerfAnalyzer.WholeLoopBeginTime(inTid);
					inBarrier.Wait();

					if (!mAppPaused)
						ProcessInput(mTimer, inTid);

					mPerfAnalyzer.UpdateBeginTime(inTid);
					BreakIfFailed(UpdateGame(mTimer, inTid));
					mPerfAnalyzer.UpdateEndTime(inTid);

					mPerfAnalyzer.DrawBeginTime(inTid);
					if (!mAppPaused)
						BreakIfFailed(Draw(mTimer, inTid));
					mPerfAnalyzer.DrawEndTime(inTid);

					mPerfAnalyzer.WholeLoopEndTime(inTid);
				}
			}
			catch (std::exception& e) {
				Logln("Thread ", std::to_string(inTid), ": ", e.what());
			}

			mGameState = GameState::ETerminated;
			inBarrier.Terminate();
			mCVBarrier->Terminate();
			mSpinlockBarrier->Terminate();
			}, std::ref(msg), i + 1, std::ref(barrier));
	}

	try {
		while (msg.message != WM_QUIT && mGameState != GameState::ETerminated) {
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
					mPerfAnalyzer.WholeLoopBeginTime(0);
					barrier.Wait();

					if (mGameState == GameState::ETerminated)
						break;

					beginTime = endTime;

					if (!mAppPaused) {
						ProcessInput(mTimer, 0);
					}

					mPerfAnalyzer.UpdateBeginTime(0);
					BreakIfFailed(UpdateGame(mTimer, 0));
					mPerfAnalyzer.UpdateEndTime(0);

					mPerfAnalyzer.DrawBeginTime(0);
					if (!mAppPaused)
						BreakIfFailed(Draw(mTimer, 0));
					mPerfAnalyzer.DrawEndTime(0);

					mPerfAnalyzer.WholeLoopEndTime(0);
				}
			}
		}
	}
	catch (std::exception& e) {
		Logln("Thread 0: ", e.what());
	}
	
	mGameState = GameState::ETerminated;
	barrier.Terminate();
	mCVBarrier->Terminate();
	mSpinlockBarrier->Terminate();

	for (UINT i = 0, end = mNumProcessors - 1; i < end; ++i)
		mThreads[i].join();
#endif // UsingVulkan

	return GameResult(static_cast<HRESULT>(msg.wParam));
}

void GameWorld::AddActor(Actor* inActor) {
	inActor->SetOwnerThreadId(mNextThreadId);
	
	if (bUpdatingActors[mNextThreadId])
		mPendingActors[mNextThreadId].push_back(inActor);
	else
		mActors[mNextThreadId].push_back(inActor);
	
	++mNextThreadId;
	if (mNextThreadId >= mThreads.size()) mNextThreadId = 0;
}

void GameWorld::RemoveActor(Actor* inActor) {
	UINT tid = inActor->GetOwnerThreadId();
	auto& actors = mActors[tid];
	auto iter = std::find(actors.begin(), actors.end(), inActor);
	if (iter != actors.end()) {
		std::iter_swap(iter, actors.end() - 1);
		actors.pop_back();
	}
}

GameResult GameWorld::AddMesh(const std::string& inFileName, Mesh*& outMeshPtr, bool inIsSkeletal, bool inNeedToBeAligned) {
	Logln("Begin AddMesh");
	auto iter = mMeshes.find(inFileName);
	if (iter != mMeshes.end()) {
		outMeshPtr = iter->second.get();
		return GameResultOk;
	}

	auto mesh = std::make_unique<Mesh>(inIsSkeletal, inNeedToBeAligned);
	Logln("Begin mesh load");
	CheckGameResult(mesh->Load(inFileName));
	Logln("End mesh load");

	outMeshPtr = mesh.get();
	mMeshes.emplace(inFileName, std::move(mesh));

	Logln("End AddMesh");
	return GameResultOk;
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

#ifdef UsingVulkan
void GameWorld::MsgProc(GLFWwindow* inMainWnd, int inKey, int inScanCode, int inAction, int inMods) {
	if (inKey == GLFW_KEY_ESCAPE && inAction == GLFW_PRESS) {
		mGameState = GameState::ETerminated;
	}
	else if (inKey == GLFW_KEY_MINUS && (inAction == GLFW_PRESS || inAction == GLFW_REPEAT)) {
		// Reduce master volume
		float volume = mAudioSystem->GetBusVolume("bus:/");
		volume -= 0.1f;
		if (volume < 0.0f) volume = 0.0f;
		mAudioSystem->SetBusVolume("bus:/", volume);
	}
	else if (inKey == GLFW_KEY_EQUAL && (inAction == GLFW_PRESS || inAction == GLFW_REPEAT)) {
		// Increase master volume
		float volume = mAudioSystem->GetBusVolume("bus:/");
		volume += 0.1f;
		if (volume > 1.0f) volume = 1.0f;
		mAudioSystem->SetBusVolume("bus:/", volume);
	}
}

void GameWorld::OnWindowFocusChanged(bool inState) {
	mAppPaused = !inState;
	if (inState) {
		mGameState = GameState::EPlay;
		mTimer.SetLimitFrameRate(mLimitFrameRate);
		mAudioSystem->SetBusVolume("bus:/", mPrevBusVolume);
		mInputSystem->IgnoreMouseInput();
	}
	else {
		mGameState = GameState::EPaused;
		mTimer.SetLimitFrameRate(GameTimer::ELimitFrameRate30f);
		mPrevBusVolume = mAudioSystem->GetBusVolume("bus:/");
		mAudioSystem->SetBusVolume("bus:/", 0.0f);
		mInputSystem->IgnoreMouseInput();
	}
}
#else
LRESULT GameWorld::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
		// WM_ACTIVATE is sent when the window is activated or deactivated.  
		// We pause the game when the window is deactivated and unpause it 
		// when it becomes active.
	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_INACTIVE) {
			mAppPaused = true;
			mGameState = GameState::EPaused;
			mTimer.SetLimitFrameRate(GameTimer::ELimitFrameRate30f);
			mPrevBusVolume = mAudioSystem->GetBusVolume("bus:/");
			mAudioSystem->SetBusVolume("bus:/", 0.0f);
			mInputSystem->IgnoreMouseInput();
		}
		else {
			mAppPaused = false;
			mGameState = GameState::EPlay;
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
					mGameState = GameState::EPaused;
				}
				else if (wParam == SIZE_MAXIMIZED) {
					mAppPaused = false;
					mMinimized = false;
					mMaximized = true;
					mGameState = GameState::EPlay;
					hr = OnResize().hr;
				}
				else if (wParam == SIZE_RESTORED) {
					// Restoring from minimized state?
					if (mMinimized) {
						mAppPaused = false;
						mMinimized = false;
						mGameState = GameState::EPlay;
						hr = OnResize().hr;
					}

					// Restoring from maximized state?
					else if (mMaximized) {
						mAppPaused = false;
						mMaximized = false;
						mGameState = GameState::EPlay;
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
		mGameState = GameState::EPaused;
		mTimer.Stop();
		return 0;

		// WM_EXITSIZEMOVE is sent when the user releases the resize bars.
		// Here we reset everything based on the new window dimensions.
	case WM_EXITSIZEMOVE:
		mAppPaused = false;
		mResizing = false;
		mGameState = GameState::EPlay;
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

void GameWorld::OnMouseDown(WPARAM inBtnState, int inX, int inY) {}

void GameWorld::OnMouseUp(WPARAM inBtnState, int inX, int inY) {}

void GameWorld::OnMouseMove(WPARAM inBtnState, int inX, int inY) {}

void GameWorld::OnKeyboardInput(UINT msg, WPARAM wParam, LPARAM lParam) {
	if (msg == WM_KEYDOWN) {
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
		else if (wParam == VK_F1) {
			bool state = mRenderer->GetSsaoEnabled();

			mRenderer->SetSsaoEnabled(!state);
		}
		else if (wParam == VK_F2) {
			bool state = mRenderer->GetSsrEnabled();

			mRenderer->SetSsrEnabled(!state);
		}
		else if (wParam == VK_F3) {
			bool state = mRenderer->GetBloomEnabled();

			mRenderer->SetBloomEnabled(!state);
		}
		else if (wParam == VK_HOME) {
			bool state = mRenderer->GetDrawDebugSkeletonsEnabled();

			mRenderer->SetDrawDebugSkeletonsEnabled(!state);
		}
		else if (wParam == VK_END) {
			bool state = mRenderer->GetDrawDebugWindowsEnabled();

			mRenderer->SetDrawDebugWindowsEnabled(!state);
		}
	}
	else {
		if (wParam == VK_ESCAPE)
			PostQuitMessage(0);
	}
}
#endif // UsingVulkan

void GameWorld::ProcessInput(const GameTimer& gt, UINT inTid) {
	if (inTid == 0) {
		mInputSystem->PrepareForUpdate();
		mInputSystem->SetRelativeMouseMode(true);
		mInputSystem->Update();
	}

#ifndef UsingVulkan
	mCVBarrier->Wait();
#endif

	const InputState& state = mInputSystem->GetState();

	if (mGameState == GameState::EPlay) {
		auto& actors = mActors[inTid];
		
		for (auto& actor : actors) {
			if (actor->GetState() == Actor::ActorState::EActive)
				actor->ProcessInput(state);
		}
	}
	else {
		// To do for UI.
	}
}

GameResult GameWorld::UpdateGame(const GameTimer& gt, UINT inTid) {
	auto& actors = mActors[inTid];
	auto& pendingActors = mPendingActors[inTid];
	
	// Update all actors.
	bUpdatingActors[inTid] = true;
	for (auto actor : actors)
		actor->Update(gt);
	bUpdatingActors[inTid] = false;
	
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

	for (auto deadActor : deadActors) {
		auto iter = std::find(actors.begin(), actors.end(), deadActor);
		if (iter != actors.end()) {
			std::iter_swap(iter, actors.end() - 1);
			actors.pop_back();
		}
	}
	
	// Delete dead actors(which removes them from mActors).
	for (auto deadActor : deadActors)
		delete deadActor;

	CheckGameResult(mRenderer->Update(gt, inTid));

	if (inTid == 0)
		mAudioSystem->Update(gt);

	return GameResult(S_OK);
}

GameResult GameWorld::Draw(const GameTimer& gt, UINT inTid) {
	CheckGameResult(mRenderer->Draw(gt, inTid));
	
	return GameResult(S_OK);
}

GameResult GameWorld::InitMainWindow() {
#ifdef UsingVulkan
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

	glfwSetWindowUserPointer(mMainGLFWWindow, this);

	glfwSetKeyCallback(mMainGLFWWindow, GLFWProcessInputCallback);
	glfwSetFramebufferSizeCallback(mMainGLFWWindow, GLFWResizeCallback);
	glfwSetWindowFocusCallback(mMainGLFWWindow, GLFWFocusCallback);
#else
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
#endif

	return GameResult(S_OK);
}

GameResult GameWorld::OnResize() {
	if (bFinishedInit)
		CheckGameResult(mRenderer->OnResize(mClientWidth, mClientHeight));

	return GameResult(S_OK);
}