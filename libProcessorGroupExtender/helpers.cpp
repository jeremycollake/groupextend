#include "pch.h"
#include "helpers.h"

// resolve process name to PID(s)
unsigned int GetPIDsForProcessName(const WCHAR* pwszBaseName, std::vector<unsigned long>& vFoundPids)
{
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnapshot == INVALID_HANDLE_VALUE)
	{		
		return 0;
	}

	if (hSnapshot)
	{
		PROCESSENTRY32 pe32;
		pe32.dwSize = sizeof(PROCESSENTRY32);
		if (!Process32First(hSnapshot, &pe32))
		{			
			CloseHandle(hSnapshot);
			return(FALSE);
		}
		do
		{
			if (0 == _wcsnicmp(pwszBaseName, pe32.szExeFile, _countof(pe32.szExeFile)))
			{
				vFoundPids.push_back(pe32.th32ProcessID);
			}
		} while (Process32Next(hSnapshot, &pe32));

		CloseHandle(hSnapshot);
	}

	return static_cast<unsigned int>(vFoundPids.size());
}

// return value is count of processor groups. Returns list of groups associated with this process
unsigned int GetProcessProcessorGroups(const unsigned long pid, std::vector<unsigned short>& vGroups)
{
	vGroups.clear();

	HANDLE hProcess = NULL;
	hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
	if (!hProcess)
	{
		return 0;
	}
	unsigned short nGroupCount = 0;
	unsigned short* pGroupArray = NULL;
	// get the required buffer size
	if (FALSE != GetProcessGroupAffinity(hProcess, &nGroupCount, NULL)
		||
		ERROR_INSUFFICIENT_BUFFER != GetLastError())
	{
		return 0;
	}
	pGroupArray = new unsigned short[nGroupCount];
	if (FALSE == GetProcessGroupAffinity(hProcess, &nGroupCount, pGroupArray))
	{
		return 0;
	}
	// got the groups, populate vector and return
	for (unsigned short nI = 0; nI < nGroupCount; nI++)
	{
		vGroups.push_back(pGroupArray[nI]);
	}
	delete pGroupArray;

	return static_cast<unsigned int>(vGroups.size());
}

// build CPU affinity mask for X processors
unsigned long long BuildAffinityMask(const unsigned int nProcessors)
{
	unsigned long long bitmaskAffinity = 0;
	for (unsigned int n = 0; n < nProcessors; n++)
	{
		bitmaskAffinity |= (1ULL << n);
	}
	return bitmaskAffinity;
}


bool NtGetPrivByName(const WCHAR* pwszPrivName)
{
	HANDLE hToken;
	TOKEN_PRIVILEGES tkp;
	if (!OpenProcessToken(GetCurrentProcess(),
		TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
	{
		return false;
	}
	LookupPrivilegeValue(NULL, pwszPrivName, &tkp.Privileges[0].Luid);
	tkp.PrivilegeCount = 1;
	tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	return AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, (PTOKEN_PRIVILEGES)NULL, 0) ? true : false;
}