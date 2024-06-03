module;
#include <Windows.h>
#include <tlhelp32.h>

#ifndef _DEBUG
#define RESULT_DIAGNOSTICS_LEVEL 0
#endif
#include <wil/resource.h>
#include <wil/result.h>

#include <string>
#include <vector>

export module offwinlib:misc;

namespace owl::misc
{
	/*
	* Returns a vector containing the PIDs for all running processes with the specified name.
	*/
	export std::vector<DWORD> get_pids_from_process_name(const std::wstring& process_name)
	{
		// Create a snapshot of running processes
		wil::unique_hfile snapshot{CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)};
		THROW_LAST_ERROR_IF(!snapshot);

		// Get first process entry
		std::vector<DWORD> results;
		PROCESSENTRY32W process_entry =
		{
			.dwSize = sizeof(PROCESSENTRY32W)
		};
		if (!Process32FirstW(snapshot.get(), &process_entry))
		{
			return results;
		}
		
		// Check the process entry and continue with the remaining process entries
		do
		{
			if (!_wcsicmp(process_entry.szExeFile, process_name.c_str()))
			{
				results.push_back(process_entry.th32ProcessID);
			}
		} while (Process32NextW(snapshot.get(), &process_entry));
		return results;
	}
}
