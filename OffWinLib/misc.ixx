module;
#pragma comment(lib, "rpcrt4.lib")

#include <Windows.h>
#include <tlhelp32.h>

#include <wil/resource.h>
#include <wil/result.h>
#include <wil/safecast.h>

#include <filesystem>
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

	/*
	* Moves a file to a temporary folder and returns the new path.
	*/
	export std::wstring move_file_to_temp_dir(HANDLE file_handle)
	{
		// Generate a new temporary path
		auto new_path = std::wstring{std::filesystem::temp_directory_path()} + generate_uuid();

		// Construct rename info
		auto path_length = new_path.length() * sizeof(wchar_t);
		std::vector<uint8_t> rename_info_buffer(sizeof(FILE_RENAME_INFO) + path_length);  // The FileName member of FILE_RENAME_INFO is defined with a length of 1, so we don't have to add 1 for the null terminator
		auto rename_info = reinterpret_cast<FILE_RENAME_INFO*>(rename_info_buffer.data());
		rename_info->ReplaceIfExists = false;
		rename_info->RootDirectory = nullptr;
		rename_info->FileNameLength = wil::safe_cast<DWORD>(path_length);
		new_path.copy(rename_info->FileName, path_length);  // The buffer is already zero-initialized, so we don't have to worry about writing the null terminator

		// Move the file
		THROW_IF_WIN32_BOOL_FALSE(SetFileInformationByHandle(file_handle, FileRenameInfo, rename_info, wil::safe_cast<DWORD>(rename_info_buffer.size())));
		return new_path;
	}
}
