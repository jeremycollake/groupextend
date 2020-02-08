/*
* (c)2020 Jeremy Collake <jeremy@bitsum.com>, Bitsum LLC
*/
#include <windows.h>
#include <TlHelp32.h>
#include <map>
#include <atlstr.h>
#include "helpers.h"
#include "LogOut.h"

HANDLE g_hExitEvent = NULL;
LogOut Log(LogOut::LTARGET_STDOUT);

namespace GroupExtend
{
	const unsigned int REFRESH_MS = 1000;
	const unsigned short INVALID_GROUP_ID = 256;	
	const WCHAR* BUILD_NUM_STR = L"002";
	const WCHAR* INTRO_STRING = L"\ngroupextend, (c)2020 Jeremy Collake <jeremy@bitsum.com>, https://bitsum.com";
	const WCHAR* BUILD_STRING_FMT = L"\nbuild %s date %hs";
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
		if(g_hExitEvent) SetEvent(g_hExitEvent);
		return TRUE;
	default:
		return FALSE;
	}
}

void ShowUsage()
{
	wprintf(L"\nUsage: groupextend [pid|name]\n");
}

// the meat
int ExtendGroupForProcess(unsigned long pid)
{
	Log.Write(L"\n Monitoring process %u", pid);
	Log.Write(L"\n");

	unsigned short nActiveGroupCount = static_cast<unsigned short>(GetActiveProcessorGroupCount());
	if (nActiveGroupCount < 2)
	{
		Log.Write(L"\n ERROR: Active processor groups is only %u. Nothing to do, aborting.", nActiveGroupCount);
		return 2;
	}

	// should be equal size groups, but check each group to support theoretical variance in size
	std::vector<unsigned int> vecProcessorsPerGroup;
	std::vector<unsigned long long> vecAllCPUMaskPerGroup;
	for (unsigned short curGroup = 0; curGroup < nActiveGroupCount; curGroup++)
	{
		// store the processor count for this group
		vecProcessorsPerGroup.push_back(GetActiveProcessorCount(curGroup));
		// and the all CPU affinity mask for this group
		vecAllCPUMaskPerGroup.push_back(BuildAffinityMask(vecProcessorsPerGroup[curGroup]));
	}

	// now get group info for target process
	std::vector<unsigned short> vecGroupsThisProcess;
	if (!GetProcessProcessorGroups(pid, vecGroupsThisProcess))
	{
		Log.Write(L"\n ERROR: GetProcessProcessorGroups returned 0. Aborting.");
		return 3;
	}
	if (vecGroupsThisProcess.size() > 1)
	{
		Log.Write(L"\n WARNING: Process is already multi-group! This algorithm is not (yet) designed to handle this situation.");
		// continue on, debatably
	}
	Log.Write(L"\n Process currently has threads on group(s)");
	for (auto& i : vecGroupsThisProcess)
	{
		Log.Write(L" %u", i);
	}
	unsigned short nDefaultGroupId = vecGroupsThisProcess[0];

	// track all thread assignments to processor groups (all threads in the managed app)
	std::map<unsigned long, unsigned short> mapThreadIDsToProcessorGroupNum;

	// keep count of threads per group (group ID is index)
	std::vector<unsigned long> vecThreadCountPerGroup;

	// initialize vector
	for (unsigned short n = 0; n < nActiveGroupCount; n++)
	{
		vecThreadCountPerGroup.push_back(0);
	}

	do
	{
		HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
		if (hSnapshot == INVALID_HANDLE_VALUE)
		{
			Log.FormattedErrorOut(L" CreateToolhelp32Snapshot");
			return 3;
		}

		if (hSnapshot)
		{
			THREADENTRY32 te32;
			te32.dwSize = sizeof(THREADENTRY32);
			if (!Thread32First(hSnapshot, &te32))
			{
				Log.FormattedErrorOut(L" Thread32First");
				CloseHandle(hSnapshot);
				return(FALSE);
			}

			// for marking each thread ID as found so we can identify deleted threads and remove from mapThreadIDsToProcessorGroupNum
			std::map<unsigned long, bool> mapThreadIDsFoundThisEnum;
			do
			{
				if (pid == te32.th32OwnerProcessID)
				{
					mapThreadIDsFoundThisEnum[te32.th32ThreadID] = true;
				}
			} while (Thread32Next(hSnapshot, &te32));

			// remove deleted threads
			std::vector<unsigned long> vecPendingThreadIDDeletions;
			for (auto& i : mapThreadIDsToProcessorGroupNum)
			{
				if (mapThreadIDsFoundThisEnum.find(i.first) == mapThreadIDsFoundThisEnum.end())
				{
					vecPendingThreadIDDeletions.push_back(i.first);
				}
			}
			for (auto& i : vecPendingThreadIDDeletions)
			{
				unsigned short nGroupId = mapThreadIDsToProcessorGroupNum.find(i)->second;
				vecThreadCountPerGroup[nGroupId]--;
				mapThreadIDsToProcessorGroupNum.erase(i);
				Log.Write(L"\n Thread %u terminated on group %u", i, nGroupId);
			}
			// add new threads
			std::vector<unsigned long> vecPendingThreadIDAdditions;
			for (auto& i : mapThreadIDsFoundThisEnum)
			{
				if (mapThreadIDsToProcessorGroupNum.find(i.first) == mapThreadIDsToProcessorGroupNum.end())
				{
					vecPendingThreadIDAdditions.push_back(i.first);
				}
			}
			for (auto& i : vecPendingThreadIDAdditions)
			{
				unsigned short nGroupId = GroupExtend::INVALID_GROUP_ID;
				// determine target group (if room on default group, use it, then use others)
				// if not default group, check CPU assignment mask of group for first free CPU, and use it
				if (vecThreadCountPerGroup[nDefaultGroupId] < vecProcessorsPerGroup[nDefaultGroupId])
				{
					nGroupId = nDefaultGroupId;
					Log.Write(L"\n Leaving thread in default group.");
				}
				else
				{
					for (int n = 0; n < nActiveGroupCount; n++)
					{
						// only check groups other than default
						if (n != nDefaultGroupId)
						{
							if (vecThreadCountPerGroup[n] < vecProcessorsPerGroup[n])
							{
								nGroupId = n;
								break;
							}
						}
					}
				}
				if (nGroupId == GroupExtend::INVALID_GROUP_ID)
				{
					nGroupId = nDefaultGroupId;
					Log.Write(L"\n No space in supplemental group(s), leaving in default group");
				}
				// if not default group, then select specific CPU
				if (nGroupId != nDefaultGroupId)
				{
					HANDLE hThread = OpenThread(THREAD_SET_INFORMATION | THREAD_QUERY_INFORMATION, FALSE, i);
					if (hThread)
					{
						GROUP_AFFINITY grpAffinity = {};
						grpAffinity.Group = nGroupId;
						grpAffinity.Mask = vecAllCPUMaskPerGroup[nGroupId];
						DWORD_PTR dwPriorAffinity = SetThreadGroupAffinity(hThread, &grpAffinity, nullptr);
						CloseHandle(hThread);
						if (!dwPriorAffinity)
						{
							// error, so leave in default group						
							nGroupId = nDefaultGroupId;
							Log.Write(L"\n WARNING: Error setting thread affinity for %u (terminated too quick?). Leaving in default group.", i);
						}
					}
					else
					{
						// no access, so leave in default group						
						nGroupId = nDefaultGroupId;
						Log.Write(L"\n WARNING: No access to thread %u. Leaving in default group.", i);
					}
				}
				Log.Write(L"\n Thread %u found, group %u", i, nGroupId);
				vecThreadCountPerGroup[nGroupId]++;
				mapThreadIDsToProcessorGroupNum[i] = nGroupId;

				_ASSERT(vecAssignedAffinityPerGroup[nDefaultGroupId] == 0);
			}

			Log.Write(L"\n Managing %u threads", mapThreadIDsToProcessorGroupNum.size());
			for (unsigned short n = 0; n < nActiveGroupCount; n++)
			{
				Log.Write(L"\n Group %u has %u threads", n, vecThreadCountPerGroup[n]);
			}

			if (!mapThreadIDsToProcessorGroupNum.size())
			{
				Log.Write(L"\n No threads to manage, exiting ...");
				break;
			}
		}
		CloseHandle(hSnapshot);
	} while (WaitForSingleObject(g_hExitEvent, GroupExtend::REFRESH_MS) == WAIT_TIMEOUT);

	return 0;
}

int wmain(int argc, const wchar_t* argv[])
{
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

	int nR = ExtendGroupForProcess(vecTargetPIDs[0]);

	CloseHandle(g_hExitEvent);
	return nR;
}
