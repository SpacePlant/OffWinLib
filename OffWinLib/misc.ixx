module;
#pragma comment(lib, "rpcrt4.lib")

#include <Windows.h>
#include <tlhelp32.h>

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

	/*
	* Returns a random UUID.
	*/
	export std::wstring generate_uuid()
	{
		// Create UUID
		UUID uuid;
		THROW_IF_WIN32_ERROR(UuidCreate(&uuid));

		// Convert UUID to string
		wil::unique_rpc_wstr uuid_string;
		THROW_IF_WIN32_ERROR(UuidToStringW(&uuid, &uuid_string));
		return std::wstring{reinterpret_cast<wchar_t*>(uuid_string.get())};
	}
}
