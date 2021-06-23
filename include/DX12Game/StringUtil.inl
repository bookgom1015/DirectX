#ifndef __STRINGUTIL_INL__
#define __STRINGUTIL_INL__

void StringUtil::LogFunc(const std::string& text) {
	std::wstring wstr;
	wstr.assign(text.begin(), text.end());

	DWORD writtenBytes = 0;

	StringUtilHelper::gLogFileMutex.lock();

	WriteFile(
		StringUtilHelper::ghLogFile,
		wstr.c_str(),
		static_cast<DWORD>(wstr.length() * sizeof(wchar_t)),
		&writtenBytes,
		NULL
	);

	StringUtilHelper::gLogFileMutex.unlock();
}

void StringUtil::LogFunc(const std::wstring& text) {
	DWORD writtenBytes = 0;

	StringUtilHelper::gLogFileMutex.lock();

	WriteFile(
		StringUtilHelper::ghLogFile,
		text.c_str(),
		static_cast<DWORD>(text.length() * sizeof(wchar_t)),
		&writtenBytes,
		NULL
	);

	StringUtilHelper::gLogFileMutex.unlock();
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

#endif // __STRINGUTIL_INL__