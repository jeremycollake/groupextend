#include <windows.h>
#include "libProcessorGroupExtender/LogOut.h"
#include "libProcessorGroupExtender/helpers.h"
#include "libProcessorGroupExtender/groupextend.h"
#include "entry.h"

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
	LogOut Log(GroupExtend::DefaultLogTarget);
	Log.Write(GroupExtend::INTRO_STRING);
	Log.Write(GroupExtend::BUILD_STRING_FMT, GroupExtend::BUILD_NUM_STR, __DATE__);
	Log.Write(L"\n");

	if (argc < 2)
	{
		ShowUsage();
		return 1;
	}

	// try to resolve command line argument(s) to PIDs from exeName
	// if that fails, assume is numeric PID
	// this allows for processes with exeNames of integers (if that ever happens)	
	std::vector<unsigned long> vecTargetPIDs;
	for (int i = 1; i < argc; i++)
	{
		if (GroupExtend::GetPIDsForProcessName(argv[i], vecTargetPIDs))
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
		return 2;
	}

	if (vecTargetPIDs.size() > 1)
	{
		Log.Write(L"\nWARNING: Multiple process instances were found, but groupextend can currently only manage one (per instance). Managing %u", vecTargetPIDs[0]);
	}

	// required priv tokens, by name
	const WCHAR* pwszSecTokens[] =
	{
		SE_ASSIGNPRIMARYTOKEN_NAME,
		SE_DEBUG_NAME,
		SE_INC_BASE_PRIORITY_NAME
	};

	for (const WCHAR* pwszToken : pwszSecTokens)
	{
		if (!GroupExtend::NtGetPrivByName(pwszToken))
		{
			Log.Write(L"\n WARNING: Couldn't get privilege %s", pwszToken);
		}
	}

	SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
	//SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

	// create before SetConsoelCtrlHandler
	g_hExitEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

	SetConsoleCtrlHandler(CtrlHandler, TRUE);

	// for signalling caller that thread stopped (e.g. process no longer exists or error)
	HANDLE hThreadStoppedEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

	if (!g_hExitEvent || !hThreadStoppedEvent)
	{
		Log.Write(L"\n ERROR creating events. Aborting");
		if (hThreadStoppedEvent) CloseHandle(hThreadStoppedEvent);
		if (g_hExitEvent) CloseHandle(g_hExitEvent);
		return 3;
	}

	//
	// start management of target process threads
	// magic is in libProcessorGroupExtender
	// 	
	ProcessorGroupExtender_SingleProcess cExtender;
	if (cExtender.StartAsync(vecTargetPIDs[0], 0, GroupExtend::DefaultLogTarget, hThreadStoppedEvent))
	{
		HANDLE hWaits[2] = { g_hExitEvent, hThreadStoppedEvent };
		WaitForMultipleObjects(_countof(hWaits), hWaits, FALSE, INFINITE);
		cExtender.Stop();
	}

	CloseHandle(hThreadStoppedEvent);
	CloseHandle(g_hExitEvent);
	return 0;
}
