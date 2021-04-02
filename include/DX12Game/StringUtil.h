#pragma once

class StringUtil {
public:
	static void Log(std::initializer_list<std::string> texts);
	static void Log(const std::string& text);
	static void Logln(std::initializer_list<std::string> texts);
	static void Logln(const std::string& text);

	static void Log(std::initializer_list<std::wstring> texts);
	static void Log(const std::wstring& text);
	static void Logln(std::initializer_list<std::wstring> texts);
	static void Logln(const std::wstring& text);

private:
	static HANDLE mhLogFile;

	static std::mutex mLogFileMutex;
};