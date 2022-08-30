#include "LearningDXR/Application.h"

#include <windowsx.h>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd) {
	try {
		Application app;

		auto result = app.Initialize();
		if (FAILED(result.hr)) {
			ShowRemovedReason(app.GetRenderer());
			WLogln(L"Error Occured");
			return static_cast<int>(result.hr);
		}

		result = app.RunLoop();
		if (FAILED(result.hr)) {
			ShowRemovedReason(app.GetRenderer());
			WLogln(L"Error Occured");
			return static_cast<int>(result.hr);
		}

		app.CleanUp();

		WLogln(L"The game has been succeesfully cleaned up");

		return static_cast<int>(result.hr);
	}
	catch (const std::exception& e) {
		std::wstringstream wsstream;
		wsstream << e.what();
		WErrln(wsstream.str());
		WLogln(L"Catched");
		return -1;
	}
}

namespace {
	LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		// Forward hwnd on because we can get messages (e.g., WM_CREATE)
		// before CreateWindow returns, and thus before mhMainWnd is valid
		return Application::GetApp()->MsgProc(hwnd, msg, wParam, lParam);
	}
}

Application* Application::sApp = nullptr;

Application::Application() {
	sApp = this;

	bIsCleanedUp = false;

	mRenderer = std::make_unique<Renderer>();

	mClientWidth = 800;
	mClientHeight = 600;
}

Application::~Application() {
	if (!bIsCleanedUp)
		CleanUp();
}

GameResult Application::Initialize() {
	CheckGameResult(InitMainWindow());
	CheckGameResult(mRenderer->Initialize(mhMainWnd, mClientWidth, mClientHeight));

	return GameResultOk;
}

GameResult Application::RunLoop() {
	MSG msg = { 0 };

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

			if (!mAppPaused) {
				BreakIfFailed(Update());
				BreakIfFailed(Draw());
			}
			else {
				Sleep(100);
			}
		}
	}

	return GameResult(static_cast<HRESULT>(msg.wParam));
}

void Application::CleanUp() {
	if (mRenderer != nullptr)
		mRenderer->CleanUp();

	bIsCleanedUp = true;
}

GameResult Application::InitMainWindow() {
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
	if (!RegisterClass(&wc))  ReturnGameResult(E_FAIL, L"Failed to register window class");

	// Compute window rectangle dimensions based on requested client area dimensions.
	RECT R = { 0, 0, static_cast<LONG>(mClientWidth), static_cast<LONG>(mClientHeight) };
	AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
	int width = R.right - R.left;
	int height = R.bottom - R.top;

	mPrimaryMonitorWidth = GetSystemMetrics(SM_CXSCREEN);
	mPrimaryMonitorHeight = GetSystemMetrics(SM_CYSCREEN);

	int clientPosX = static_cast<int>((mPrimaryMonitorWidth - mClientWidth) * 0.5f);
	int clientPosY = static_cast<int>((mPrimaryMonitorHeight - mClientHeight) * 0.5f);

	mhMainWnd = CreateWindow(
		L"MainWnd", L"DXR Application",
		WS_OVERLAPPEDWINDOW, 
		clientPosX, clientPosY, 
		width, height, 
		0, 0, mhInst, 0);
	if (!mhMainWnd) ReturnGameResult(E_FAIL, L"Failed to create window");

	ShowWindow(mhMainWnd, SW_SHOW);
	UpdateWindow(mhMainWnd);

	return GameResultOk;
}

void Application::OnResize() {
	mRenderer->OnResize(mClientWidth, mClientHeight);
}

GameResult Application::Update() {
	CheckGameResult(mRenderer->Update(mTimer));

	return GameResultOk;
}

GameResult Application::Draw() {
	CheckGameResult(mRenderer->Draw());

	return GameResultOk;
}

void Application::OnMouseDown(WPARAM inBtnState, int inX, int inY) {

}

void Application::OnMouseUp(WPARAM inBtnState, int inX, int inY) {

}

void Application::OnMouseMove(WPARAM inBtnState, int inX, int inY) {

}

void Application::OnKeyboardInput(UINT msg, WPARAM wParam, LPARAM lParam) {
	if (msg == WM_KEYDOWN) {
		if (wParam == VK_SPACE) {
			bool state = mRenderer->GetRenderType();
			mRenderer->SetRenderType(!state);
		}
	}
	else {

	}
}

Application* Application::GetApp() {
	return sApp;
}

Renderer* Application::GetRenderer() {
	return mRenderer.get();
}

LRESULT Application::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
		// WM_ACTIVATE is sent when the window is activated or deactivated.  
		// We pause the game when the window is deactivated and unpause it 
		// when it becomes active.
	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_INACTIVE) {
			mTimer.Stop();
			mAppPaused = true;
		}
		else {
			mTimer.Start();
			mAppPaused = false;
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
		mTimer.Stop();
		mAppPaused = true;
		mResizing = true;
		return 0;

		// WM_EXITSIZEMOVE is sent when the user releases the resize bars.
		// Here we reset everything based on the new window dimensions.
	case WM_EXITSIZEMOVE:
		mTimer.Start();
		mAppPaused = false;
		mResizing = false;
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