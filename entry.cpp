#include <windows.h>
#include "LogOut.h"
#include "groupextend.h"
#include "entry.h"
#include "helpers.h"

HANDLE g_hExitEvent = NULL;

void ShowUsage()
{
	wprintf(L"\nUsage: groupextend [pid|name]\n");
}

BOOL WINAPI CtrlHandler(DWORD fdwCtrlType)
{
	switch (fdwCtrlType)
	{
	case CTRL_SHUTDOWN_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_CLOSE_EVENT:
	case CTRL_C_EVENT:
	case CTRL_BREAK_EVENT:
		wprintf(L"\n > Ctrl event");
		if (g_hExitEvent) SetEvent(g_hExitEvent);
		return TRUE;
	default:
		return FALSE;
	}
}

int wmain(int argc, const wchar_t* argv[])
{
	LogOut Log(LogOut::LTARGET_STDOUT);
	Log.Write(GroupExtend::INTRO_STRING);
	Log.Write(GroupExtend::BUILD_STRING_FMT, GroupExtend::BUILD_NUM_STR, __DATE__);
	Log.Write(L"\n");

	if (argc < 2)
	{
		ShowUsage();
		return 1;
	}

	g_hExitEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

	// try to resolve command line argument(s) to PIDs from exeName
	// if that fails, assume is numeric PID
	// this allows for processes with exeNames of integers (if that ever happens)	
	std::vector<unsigned long> vecTargetPIDs;
	for (int i = 1; i < argc; i++)
	{
		if (GetPIDsForProcessName(argv[i], vecTargetPIDs))
		{
			Log.Write(L"\n%s has instances of PID(s)", argv[i]);
			for (auto& pid : vecTargetPIDs)
			{
				Log.Write(L" %u", pid);
			}
		}
		else
		{
			unsigned long pid = wcstoul(argv[i], nullptr, 10);
			if (pid)
			{
				vecTargetPIDs.push_back(pid);
			}
		}
	}

	if (vecTargetPIDs.size() == 0)
	{
		Log.Write(L"\nERROR: No processes found that match specification.\n");
		return 5;
	}

	if (vecTargetPIDs.size() > 1)
	{
		Log.Write(L"\nWARNING: Multiple process instances were found, but groupextend can currently only manage one (per instance). Managing %u", vecTargetPIDs[0]);
	}

	SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
	//SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

	SetConsoleCtrlHandler(CtrlHandler, TRUE);

	int nR = ExtendGroupForProcess(vecTargetPIDs[0], g_hExitEvent, Log);

	CloseHandle(g_hExitEvent);
	return nR;
}
