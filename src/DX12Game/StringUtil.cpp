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
	WriteFile(mhLogFile, wsstream.str().c_str(), (DWORD)wsstream.str().length() * sizeof(wchar_t), &writtenBytes, NULL);

	mLogFileMutex.unlock();
}

void StringUtil::Log(const std::string& text) {
	std::wstringstream wsstream;

	wsstream << text.c_str();

	mLogFileMutex.lock();

	DWORD writtenBytes = 0;
	WriteFile(mhLogFile, wsstream.str().c_str(), (DWORD)wsstream.str().length() * sizeof(wchar_t), &writtenBytes, NULL);

	mLogFileMutex.unlock();
}

void StringUtil::Logln(std::initializer_list<std::string> texts) {
	std::wstringstream wsstream;

	for (auto text : texts)
		wsstream << text.c_str();
	wsstream << '\n';

	mLogFileMutex.lock();

	DWORD writtenBytes = 0;
	WriteFile(mhLogFile, wsstream.str().c_str(), (DWORD)wsstream.str().length() * sizeof(wchar_t), &writtenBytes, NULL);

	mLogFileMutex.unlock();
}

void StringUtil::Logln(const std::string& text) {
	std::wstringstream wsstream;

	wsstream << text.c_str() << '\n';

	mLogFileMutex.lock();

	DWORD writtenBytes = 0;
	WriteFile(mhLogFile, wsstream.str().c_str(), (DWORD)wsstream.str().length() * sizeof(wchar_t), &writtenBytes, NULL);

	mLogFileMutex.unlock();
}

void StringUtil::Log(std::initializer_list<std::wstring> texts) {
	std::wstringstream wsstream;

	for (auto text : texts)
		wsstream << text;

	mLogFileMutex.lock();

	DWORD writtenBytes = 0;
	WriteFile(mhLogFile, wsstream.str().c_str(), (DWORD)wsstream.str().length() * sizeof(wchar_t), &writtenBytes, NULL);

	mLogFileMutex.unlock();
}

void StringUtil::Log(const std::wstring& text) {
	std::wstringstream wsstream;

	wsstream << text;

	mLogFileMutex.lock();

	DWORD writtenBytes = 0;
	WriteFile(mhLogFile, wsstream.str().c_str(), (DWORD)wsstream.str().length() * sizeof(wchar_t), &writtenBytes, NULL);

	mLogFileMutex.unlock();
}

void StringUtil::Logln(std::initializer_list<std::wstring> texts) {
	std::wstringstream wsstream;

	for (auto text : texts)
		wsstream << text;
	wsstream << L'\n';

	mLogFileMutex.lock();

	DWORD writtenBytes = 0;
	WriteFile(mhLogFile, wsstream.str().c_str(), (DWORD)wsstream.str().length() * sizeof(wchar_t), &writtenBytes, NULL);

	mLogFileMutex.unlock();
}

void StringUtil::Logln(const std::wstring& text) {
	std::wstringstream wsstream;

	wsstream << text << L'\n';

	mLogFileMutex.lock();

	DWORD writtenBytes = 0;
	WriteFile(mhLogFile, wsstream.str().c_str(), (DWORD)wsstream.str().length() * sizeof(wchar_t), &writtenBytes, NULL);

	mLogFileMutex.unlock();
}