#include "DX12Game/StringUtil.h"

HANDLE StringUtil::StringUtilHelper::ghLogFile = CreateFile(
	L"./log.txt",
	GENERIC_WRITE, 
	FILE_SHARE_WRITE, 
	NULL, 
	CREATE_ALWAYS,
	FILE_ATTRIBUTE_NORMAL,
	NULL
);

std::mutex StringUtil::StringUtilHelper::gLogFileMutex;