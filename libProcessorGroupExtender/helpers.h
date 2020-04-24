#pragma once

namespace GroupExtend
{
	// resolve process name to PID(s)
	unsigned int GetPIDsForProcessName(const WCHAR* pwszBaseName, std::vector<unsigned long>& vFoundPids);
	// return value is count of processor groups. Returns list of groups associated with this process
	unsigned int GetProcessProcessorGroups(const unsigned long pid, std::vector<unsigned short>& vGroups);
	// build CPU affinity mask for X processors
	unsigned long long BuildAffinityMask(const unsigned int nProcessors);

	// privilege acquistion
	bool NtGetPrivByName(const WCHAR* ptszPrivName);
}