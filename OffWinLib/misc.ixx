module;
#pragma comment(lib, "rpcrt4.lib")

#include <Windows.h>
#include <tlhelp32.h>

#include <wil/resource.h>
#include <wil/result.h>
#include <wil/safecast.h>

#include <chrono>
#include <concepts>
#include <filesystem>
#include <span>
#include <stdint.h>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

export module offwinlib:misc;

import :data_conversion;

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
	* Moves a file or folder to a temporary folder and returns the new path.
	*/
	export std::wstring move_to_temp_dir(HANDLE file_handle)
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

	/*
	* Returns an embedded binary resource.
	*/
	export std::span<const uint8_t> get_resource(uint16_t id)
	{
		// Find resource meta information
		auto resource_info = FindResourceW(nullptr, MAKEINTRESOURCEW(id), RT_RCDATA);
		THROW_LAST_ERROR_IF_NULL(resource_info);

		// Open resource
		auto resource = LoadResource(nullptr, resource_info);
		THROW_LAST_ERROR_IF_NULL(resource);

		// Get resource location and size
		auto data = LockResource(resource);
		THROW_HR_IF_NULL(E_FAIL, data);  // LockResource documentation doesn't say whether the function sets GetLastError
		auto size = SizeofResource(nullptr, resource_info);
		THROW_LAST_ERROR_IF(!size);
		return std::span{static_cast<uint8_t*>(data), size};
	}

	template<typename fn_type>
	concept loop_fn = requires(fn_type fn)
	{
		{ fn() } -> std::convertible_to<bool>;
	};
	/*
	* Executes "fn" repeatedly until it returns true. If the timeout is reached, an exception is thrown.
	*/
	export void loop_with_timeout(std::chrono::milliseconds max_wait, loop_fn auto&& fn)
	{
		auto start = std::chrono::steady_clock::now();
		while ((std::chrono::steady_clock::now() - start) < max_wait)
		{
			if (std::forward<decltype(fn)>(fn)())
			{
				return;
			}
		}
		THROW_WIN32(WAIT_TIMEOUT);
	}

	/*
	* Appends a DOS timestamp to a vector (helper function for RBS file generation).
	*/
	static void rbs_append_timestamp(std::vector<uint8_t>& v)
	{
		// Get local time
		SYSTEMTIME local_time;
		GetLocalTime(&local_time);

		// Convert to file time
		FILETIME file_time;
		THROW_IF_WIN32_BOOL_FALSE(SystemTimeToFileTime(&local_time, &file_time));

		// Convert to DOS time
		WORD dos_date;
		WORD dos_time;
		THROW_IF_WIN32_BOOL_FALSE(FileTimeToDosDateTime(&file_time, &dos_date, &dos_time));

		// Append data to vector
		auto dos_time_bytes = data_conversion::value_to_bytes(dos_time);
		auto dos_date_bytes = data_conversion::value_to_bytes(dos_date);
		v.insert(v.cend(), dos_time_bytes.cbegin(), dos_time_bytes.cend());
		v.insert(v.cend(), dos_date_bytes.cbegin(), dos_date_bytes.cend());
	}

	/*
	* Appends a string prefixed with the length to a vector (helper function for RBS file generation).
	*/
	static void rbs_append_string(std::vector<uint8_t>& v, std::wstring_view s)
	{
		// Convert string to UTF-8
		auto s_converted = data_conversion::utf16_to_utf8(s);

		// Convert string length to bytes
		auto length = wil::safe_cast<uint16_t>(s_converted.length());
		auto length_bytes = data_conversion::value_to_bytes(length);

		// Append data to vector
		v.insert(v.cend(), length_bytes.cbegin(), length_bytes.cend());
		v.insert(v.cend(), s_converted.cbegin(), s_converted.cend());
	}

	/*
	* Builds a Rollback Script (RBS) file for the Windows Installer. The format is undocumented and was partially reverse engineered. Strings should probably only contain ASCII characters. The "package" argument should match the name of the MSI package (not the file name). The "command" argument will be executed as SYSTEM during rollback.
	*/
	export std::vector<uint8_t> build_rbs(std::wstring_view package, std::wstring_view command)
	{
		// Split command and arguments
		std::wstring_view arguments;
		auto space = command.find(L" ");
		if (space != std::wstring_view::npos)
		{
			arguments = command.substr(space + 1);
			command.remove_suffix(arguments.length() + 1);
		}

		// Create buffer and append static data
		std::vector<uint8_t> rbs;
		rbs.insert(rbs.cend(),
			{
				0x02, 0x09, 0x00, 0x40, 0x49, 0x58, 0x4F, 0x53, 0x00, 0x40, 0xF4, 0x01, 0x00, 0x00, 0x00, 0x40
			});
		
		// Append DOS timestamp (not used)
		rbs_append_timestamp(rbs);

		// Append static data
		rbs.insert(rbs.cend(),
			{
				0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x09, 0x00, 0x00, 0x40, 0x02, 0x00,
				0x00, 0x00, 0x00, 0x40, 0x15, 0x00, 0x00, 0x00, 0x00, 0x40, 0x04, 0x00, 0x00, 0x00, 0x00, 0x40,
				0x01, 0x00, 0x00, 0x00, 0x04, 0x10
			});

		// Append random GUID prefixed with the length (not used)
		rbs_append_string(rbs, L"{" + generate_uuid() + L"}");

		// Append package name prefixed with the length
		rbs_append_string(rbs, package);

		// Append random MSI name prefixed with the length (not used)
		rbs_append_string(rbs, generate_uuid() + L".msi");

		// Append static data
		rbs.insert(rbs.cend(),
			{
				0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x01, 0x00, 0x40, 0x01, 0x00,
				0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x80
			});

		// Append random GUID prefixed with the length (not used)
		rbs_append_string(rbs, L"{" + generate_uuid() + L"}");

		// Append static data
		rbs.insert(rbs.cend(),
			{
				0x00, 0x80, 0x00, 0x80, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00,
				0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x02, 0x00, 0x00, 0x00, 0x05, 0x03, 0x00, 0x40,
				0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40
			});

		// Append code page (not used)
		uint16_t code_page = CP_UTF8;
		auto code_page_bytes = data_conversion::value_to_bytes(code_page);
		rbs.insert(rbs.cend(), code_page_bytes.cbegin(), code_page_bytes.cend());

		// Append static data
		rbs.insert(rbs.cend(),
			{
				0x00, 0x00, 0x05, 0x02, 0x00, 0x40, 0x01, 0x00, 0x00, 0x00
			});

		// Append package name prefixed with the length (not used)
		rbs_append_string(rbs, package);

		// Append static data
		rbs.insert(rbs.cend(),
			{
				0x08, 0x01
			});

		// Generate and append random action name prefixed with the length (not used)
		auto action_name = generate_uuid();
		rbs_append_string(rbs, action_name);

		// Append partially unknown data (custom rollback action options?)
		rbs.insert(rbs.cend(),
			{
				0x4C, (arguments.empty() ? uint8_t{0x03} : uint8_t{0x04})
			});

		// Append action name prefixed with the length (not used)
		rbs_append_string(rbs, action_name);

		// Append partially unknown data (custom rollback action options?)
		rbs.insert(rbs.cend(),
			{
				0x00, 0x40, 0xF2, 0x0D, 0x00, 0x00
			});

		// Append command prefixed with the length
		rbs_append_string(rbs, command);

		// Append arguments prefixed with the length
		if (!arguments.empty())
		{
			rbs_append_string(rbs, arguments);
		}

		// Append static data
		rbs.insert(rbs.cend(),
			{
				0x03, 0x03, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
				0x00, 0x00, 0x00, 0x00
			});
		return rbs;
	}
}
