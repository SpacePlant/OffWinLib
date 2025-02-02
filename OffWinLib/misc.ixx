module;
#pragma comment(lib, "ntdll")
#pragma comment(lib, "rpcrt4.lib")

#include <Windows.h>
#include <winternl.h>
#include <comdef.h>
#include <taskschd.h>
#include <tlhelp32.h>

#include <wil/com.h>
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
import :syscall;

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
	void rbs_append_timestamp(std::vector<uint8_t>& v)
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
	void rbs_append_string(std::vector<uint8_t>& v, std::wstring_view s)
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

	/*
	* Starts the specified task from the specified task folder.
	*/
	export void start_scheduled_task(const std::wstring& task_folder, const std::wstring& task_name)
	{
		// Make sure COM is initialized
		auto com_cleanup = wil::CoInitializeEx();

		// Connect to the task scheduler
		auto task_scheduler = wil::CoCreateInstance<TaskScheduler, ITaskService>();
		THROW_IF_FAILED(task_scheduler->Connect(_variant_t{}, _variant_t{}, _variant_t{}, _variant_t{}));

		// Get the task folder
		wil::com_ptr<ITaskFolder> folder;
		THROW_IF_FAILED(task_scheduler->GetFolder(_bstr_t{task_folder.c_str()}, &folder));

		// Run the task
		wil::com_ptr<IRegisteredTask> task;
		THROW_IF_FAILED(folder->GetTask(_bstr_t{task_name.c_str()}, &task));
		THROW_IF_FAILED(task->Run(_variant_t{}, nullptr));
	}

	/*
	* Overwrites the .text section of a loaded DLL with a clean version from "KnownDlls" using indirect syscalls.
	* 
	* Note: Make sure that "syscall::initialize_syscalls" has been called first!
	*/
	export void unhook_dll_sys(const std::wstring& dll_name)
	{
		// Prepare syscalls
		enum SECTION_INHERIT
		{
			ViewShare = 1,
			ViewUnmap = 2
		};
		auto NtOpenSection = &syscall::call<L"NtOpenSection", HANDLE*, ACCESS_MASK, OBJECT_ATTRIBUTES*>;
		auto NtMapViewOfSection = &syscall::call<L"NtMapViewOfSection", HANDLE, HANDLE, void**, uintptr_t, size_t, int64_t*, size_t*, SECTION_INHERIT, uint32_t, uint32_t>;
		auto NtUnmapViewOfSection = &syscall::call<L"NtUnmapViewOfSection", HANDLE, void*>;
		auto NtProtectVirtualMemory = &syscall::call<L"NtProtectVirtualMemory", HANDLE, void**, size_t*, uint32_t, uint32_t*>;

		// Open clean DLL handle from "KnownDlls"
	#ifdef _WIN64
		auto knowndlls_path = LR"(\KnownDlls\)" + dll_name + L".dll";
	#else
		auto knowndlls_path = LR"(\KnownDlls32\)" + dll_name + L".dll";
	#endif
		UNICODE_STRING knowndlls_path_unicode;
		RtlInitUnicodeString(&knowndlls_path_unicode, knowndlls_path.c_str());
		OBJECT_ATTRIBUTES knowndlls_path_object;
		InitializeObjectAttributes(&knowndlls_path_object, &knowndlls_path_unicode, 0, nullptr, nullptr);
		wil::unique_handle section;
		THROW_IF_NTSTATUS_FAILED(NtOpenSection(&section, SECTION_MAP_READ, &knowndlls_path_object));

		// Map clean DLL to memory
		auto current_process = GetCurrentProcess();
		void* dll_clean = 0;
		size_t view_size = 0;
		THROW_IF_NTSTATUS_FAILED(NtMapViewOfSection(section.get(), current_process, &dll_clean, 0, 0, nullptr, &view_size, ViewUnmap, 0, PAGE_READONLY));

		// Find .text sections
		auto dll = GetModuleHandleW(dll_name.c_str());
		THROW_LAST_ERROR_IF_NULL(dll);
		auto dos_header = reinterpret_cast<PIMAGE_DOS_HEADER>(dll);
		auto nt_header = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<uintptr_t>(dll) + dos_header->e_lfanew);
		auto text_current = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(dll) + nt_header->OptionalHeader.BaseOfCode);
		auto text_clean = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(dll_clean) + nt_header->OptionalHeader.BaseOfCode);
		auto text_size = nt_header->OptionalHeader.SizeOfCode;

		// Make current .text section writable
		auto base_address = text_current;
		size_t region_size = text_size;
		uint32_t old_protect;
		THROW_IF_NTSTATUS_FAILED(NtProtectVirtualMemory(current_process, &base_address, &region_size, PAGE_EXECUTE_READWRITE, &old_protect));

		// Overwrite current .text section with clean .text section
		std::memcpy(text_current, text_clean, text_size);

		// Restore memory protections
		base_address = text_current;
		region_size = text_size;
		THROW_IF_NTSTATUS_FAILED(NtProtectVirtualMemory(current_process, &base_address, &region_size, old_protect, &old_protect));

		// Unmap clean DLL
		THROW_IF_NTSTATUS_FAILED(NtUnmapViewOfSection(current_process, dll_clean));
	}
}
