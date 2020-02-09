#pragma once

#include <windows.h>
#include "version.h"

void ShowUsage();
BOOL WINAPI CtrlHandler(DWORD fdwCtrlType);
int wmain(int argc, const wchar_t* argv[]);

namespace GroupExtend
{
	const WCHAR* const BUILD_NUM_STR = CURRENT_VERSION;	
	const unsigned short INVALID_GROUP_ID = 256;
	const WCHAR* const INTRO_STRING = L"\ngroupextend, (c)2020 Jeremy Collake <jeremy@bitsum.com>, https://bitsum.com";
	const WCHAR* const BUILD_STRING_FMT = L"\nbuild %s date %hs";
	const LogOut::LOG_TARGET DefaultLogTarget = LogOut::LTARGET_STDOUT;
}