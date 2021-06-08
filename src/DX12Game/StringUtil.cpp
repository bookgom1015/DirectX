#include "DX12Game/GameCore.h"

HANDLE StringUtil::mhLogFile = CreateFile(L"./log.txt",
	GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

std::mutex StringUtil::mLogFileMutex;

void StringUtil::Log(std::initializer_list<std::string> texts) {
	std::wstringstream wsstream;

	for (auto text : texts)
		wsstream << text.c_str();

	mLogFileMutex.lock();

	DWORD writtenBytes = 0;
	WriteFile(
		mhLogFile,
		wsstream.str().c_str(), 
		static_cast<DWORD>(wsstream.str().length() * sizeof(wchar_t)),
		&writtenBytes,
		NULL
	);

	mLogFileMutex.unlock();
}

void StringUtil::Log(const std::string& text) {
	std::wstringstream wsstream;

	wsstream << text.c_str();

	mLogFileMutex.lock();

	DWORD writtenBytes = 0;
	WriteFile(
		mhLogFile,
		wsstream.str().c_str(),
		static_cast<DWORD>(wsstream.str().length() * sizeof(wchar_t)),
		&writtenBytes,
		NULL
	);

	mLogFileMutex.unlock();
}

void StringUtil::Logln(std::initializer_list<std::string> texts) {
	std::wstringstream wsstream;

	for (auto text : texts)
		wsstream << text.c_str();
	wsstream << '\n';

	mLogFileMutex.lock();

	DWORD writtenBytes = 0;
	WriteFile(
		mhLogFile, 
		wsstream.str().c_str(), 
		static_cast<DWORD>(wsstream.str().length() * sizeof(wchar_t)),
		&writtenBytes, 
		NULL
	);

	mLogFileMutex.unlock();
}

void StringUtil::Logln(const std::string& text) {
	std::wstringstream wsstream;

	wsstream << text.c_str() << '\n';

	mLogFileMutex.lock();

	DWORD writtenBytes = 0;
	WriteFile(
		mhLogFile,
		wsstream.str().c_str(),
		static_cast<DWORD>(wsstream.str().length() * sizeof(wchar_t)),
		&writtenBytes,
		NULL
	);

	mLogFileMutex.unlock();
}

void StringUtil::Log(std::initializer_list<std::wstring> texts) {
	std::wstringstream wsstream;

	for (auto text : texts)
		wsstream << text;

	mLogFileMutex.lock();

	DWORD writtenBytes = 0;
	WriteFile(
		mhLogFile, 
		wsstream.str().c_str(),
		static_cast<DWORD>(wsstream.str().length() * sizeof(wchar_t)),
		&writtenBytes, 
		NULL
	);

	mLogFileMutex.unlock();
}

void StringUtil::Log(const std::wstring& text) {
	std::wstringstream wsstream;

	wsstream << text;

	mLogFileMutex.lock();

	DWORD writtenBytes = 0;
	WriteFile(
		mhLogFile,
		wsstream.str().c_str(), 
		static_cast<DWORD>(wsstream.str().length() * sizeof(wchar_t)),
		&writtenBytes, 
		NULL
	);

	mLogFileMutex.unlock();
}

void StringUtil::Logln(std::initializer_list<std::wstring> texts) {
	std::wstringstream wsstream;

	for (auto text : texts)
		wsstream << text;
	wsstream << L'\n';

	mLogFileMutex.lock();

	DWORD writtenBytes = 0;
	WriteFile(
		mhLogFile, 
		wsstream.str().c_str(), 
		static_cast<DWORD>(wsstream.str().length() * sizeof(wchar_t)),
		&writtenBytes, 
		NULL
	);

	mLogFileMutex.unlock();
}

void StringUtil::Logln(const std::wstring& text) {
	std::wstringstream wsstream;

	wsstream << text << L'\n';

	mLogFileMutex.lock();

	DWORD writtenBytes = 0;
	WriteFile(
		mhLogFile,
		wsstream.str().c_str(),
		static_cast<DWORD>(wsstream.str().length() * sizeof(wchar_t)),
		&writtenBytes, 
		NULL
	);

	mLogFileMutex.unlock();
}

void StringUtil::SetTextToWnd(HWND hWnd, LPCWSTR newText) {
	SetWindowText(hWnd, newText);
}

void StringUtil::AppendTextToWnd(HWND hWnd, LPCWSTR newText) {
	int finalLength = GetWindowTextLength(hWnd) + lstrlen(newText) + 1;
	wchar_t* buf = reinterpret_cast<wchar_t*>(std::malloc(finalLength * sizeof(wchar_t)));

	GetWindowText(hWnd, buf, finalLength);

	wcscat_s(buf, finalLength, newText);

	SetWindowText(hWnd, buf);

	std::free(buf);
}