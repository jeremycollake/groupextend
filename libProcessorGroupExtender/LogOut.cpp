/*
* (c)2019 Jeremy Collake <jeremy@bitsum.com>, Bitsum LLC
*/
#include "pch.h"
#include "LogOut.h"

LogOut::LogOut(const LOG_TARGET target) : logTarget(target)
{
}

void LogOut::SetTarget(const LOG_TARGET target)
{
	logTarget = target;
}

void LogOut::Write(LPCTSTR fmt, ...)
{
	va_list marker;
	TCHAR szBuf[4096] = { 0 };
	va_start(marker, fmt);
	wvsprintf(szBuf, fmt, marker);
	va_end(marker);

	CString csTemp;
	csTemp.Format(L"%s", szBuf);
	switch (logTarget)
	{
	case LTARGET_STDOUT:
		wprintf(L"%s", csTemp.GetBuffer());
		break;
	case LTARGET_FILE:
		// fall-through until implemented
		//break;
	case LTARGET_DEBUG:
		csTemp.Remove('\n');
		csTemp.Remove('\r');
		MyDebugOutput(csTemp);
		break;
	case LTARGET_NONE:
	default:
		break;
	}
}

void LogOut::FormattedErrorOut(const WCHAR* msg)
{
	DWORD eNum;
	TCHAR sysMsg[256];
	TCHAR* p;

	eNum = GetLastError();
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, eNum,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		sysMsg, 256, NULL);

	// Trim the end of the line and terminate it with a null
	p = sysMsg;
	while ((*p > 31) || (*p == 9))
		++p;
	do { *p-- = 0; } while ((p >= sysMsg) &&
		((*p == '.') || (*p < 33)));

	Write(L"\n  WARNING: %s failed with error %d (%s)", msg, eNum, sysMsg);
}