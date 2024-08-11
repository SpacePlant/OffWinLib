module;
#pragma comment(lib, "OneCore.lib")

#include <Windows.h>

#include <wil/resource.h>
#include <wil/result.h>
#include <wil/safecast.h>

#include <span>
#include <stdint.h>
#include <string>

export module offwinlib:injection;

import :data_conversion;

namespace owl::injection
{
	export enum class AllocationMethod
	{
		Alloc,
		Map,
		Stomp
	};

	export struct InjectionOptions
	{
		AllocationMethod allocation_method = AllocationMethod::Alloc;
		bool cleanup = false;
		std::wstring stomp_module;
		std::wstring stomp_function;
	};

	/*
	* Injects the DLL located at the given path into the process with the specified PID.
	* 
	* Injection options ("injection_options" struct):
	*	allocation_method: Specifies how memory allocation is performed.
	*		Alloc: Uses VirtualAllocEx/WriteProcessMemory.
	*		Map: Uses MapViewOfFile(2).
	*	cleanup: Wait for DLL loading to complete and free the allocated memory.
	*/
	export void dll_inject(DWORD process_id, const std::wstring& dll_path, const InjectionOptions& injection_options = {})
	{
		// Open target process handle
		wil::unique_handle process{OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, process_id)};
		THROW_LAST_ERROR_IF_NULL(process);

		void* memory = nullptr;
		auto data_size = (dll_path.length() + 1) * sizeof(wchar_t);
		if (injection_options.allocation_method == AllocationMethod::Alloc)
		{
			// Allocate memory in target process
			memory = VirtualAllocEx(process.get(), nullptr, data_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
			THROW_LAST_ERROR_IF_NULL(memory);

			// Write DLL path to target process
			THROW_IF_WIN32_BOOL_FALSE(WriteProcessMemory(process.get(), memory, dll_path.c_str(), data_size, nullptr));
		}
		else if (injection_options.allocation_method == AllocationMethod::Map)
		{
			// Create memory-backed file mapping
			wil::unique_handle mapping{CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_EXECUTE_READWRITE, 0, wil::safe_cast<DWORD>(data_size), nullptr)};
			THROW_LAST_ERROR_IF_NULL(mapping);

			// Map view into current process
			memory = MapViewOfFile(mapping.get(), FILE_MAP_WRITE, 0, 0, 0);
			THROW_LAST_ERROR_IF_NULL(memory);

			// Write DLL path to the view
			std::memcpy(memory, dll_path.c_str(), data_size);

			// Unmap view
			THROW_IF_WIN32_BOOL_FALSE(UnmapViewOfFile(memory));

			// Map view into target process
			memory = MapViewOfFile2(mapping.get(), process.get(), 0, nullptr, 0, 0, PAGE_READONLY);
			THROW_LAST_ERROR_IF_NULL(memory);
		}
		else
		{
			THROW_HR(E_INVALIDARG);
		}

		// Locate LoadLibraryW
		auto kernel32 = GetModuleHandleW(L"kernel32");
		THROW_LAST_ERROR_IF_NULL(kernel32);
		auto loadlibrary = GetProcAddress(kernel32, "LoadLibraryW");
		THROW_LAST_ERROR_IF_NULL(loadlibrary);

		// Start remote thread to execute LoadLibraryW in target process
		wil::unique_handle thread{CreateRemoteThread(process.get(), nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(loadlibrary), memory, 0, nullptr)};
		THROW_LAST_ERROR_IF_NULL(thread);

		if (injection_options.cleanup)
		{
			// Wait for the remote thread to complete execution
			THROW_LAST_ERROR_IF(WaitForSingleObject(thread.get(), INFINITE) == WAIT_FAILED);

			if (injection_options.allocation_method == AllocationMethod::Alloc)
			{
				// Free the allocated memory in target process
				THROW_IF_WIN32_BOOL_FALSE(VirtualFreeEx(process.get(), memory, 0, MEM_RELEASE));
			}
			else if (injection_options.allocation_method == AllocationMethod::Map)
			{
				// Unmap view from target process
				THROW_IF_WIN32_BOOL_FALSE(UnmapViewOfFile2(process.get(), memory, 0));
			}
			else
			{
				THROW_HR(E_INVALIDARG);
			}
		}
	}

	/*
	* Injects the shellcode into the process with the specified PID.
	* 
	* Injection options ("injection_options" struct):
	*	allocation_method: Specifies how memory allocation is performed.
	*		Alloc: Uses VirtualAllocEx/WriteProcessMemory.
	*		Map: Uses MapViewOfFile(2).
	*		Stomp: Overwrites an existing function. Uses "stomp_module" and "stomp_function" for the module/function to be overwritten (the module is not loaded automatically). Ignores "cleanup".
	*	cleanup: Wait for shellcode execution to complete and free the allocated memory.
	*/
	export void shellcode_inject(DWORD process_id, std::span<const uint8_t> shellcode, const InjectionOptions& injection_options = {})
	{
		// Open target process handle
		wil::unique_handle process{OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, process_id)};
		THROW_LAST_ERROR_IF_NULL(process);

		void* memory = nullptr;
		if (injection_options.allocation_method == AllocationMethod::Alloc)
		{
			// Allocate memory in target process
			memory = VirtualAllocEx(process.get(), nullptr, shellcode.size(), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
			THROW_LAST_ERROR_IF_NULL(memory);

			// Write shellcode to target process
			THROW_IF_WIN32_BOOL_FALSE(WriteProcessMemory(process.get(), memory, shellcode.data(), shellcode.size(), nullptr));

			// Make shellcode executable
			DWORD old_protect;
			THROW_IF_WIN32_BOOL_FALSE(VirtualProtectEx(process.get(), memory, shellcode.size(), PAGE_EXECUTE_READ, &old_protect));
		}
		else if (injection_options.allocation_method == AllocationMethod::Map)
		{
			// Create memory-backed file mapping
			wil::unique_handle mapping{CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_EXECUTE_READWRITE, 0, wil::safe_cast<DWORD>(shellcode.size()), nullptr)};
			THROW_LAST_ERROR_IF_NULL(mapping);

			// Map view into current process
			memory = MapViewOfFile(mapping.get(), FILE_MAP_WRITE, 0, 0, 0);
			THROW_LAST_ERROR_IF_NULL(memory);

			// Copy the shellcode to the view
			std::memcpy(memory, shellcode.data(), shellcode.size());

			// Unmap view
			THROW_IF_WIN32_BOOL_FALSE(UnmapViewOfFile(memory));

			// Map view into target process
			memory = MapViewOfFile2(mapping.get(), process.get(), 0, nullptr, 0, 0, PAGE_EXECUTE_READ);
			THROW_LAST_ERROR_IF_NULL(memory);
		}
		else if (injection_options.allocation_method == AllocationMethod::Stomp)
		{
			// Find function address
			auto module = LoadLibraryExW(injection_options.stomp_module.c_str(), nullptr, DONT_RESOLVE_DLL_REFERENCES);
			THROW_LAST_ERROR_IF_NULL(module);
			memory = GetProcAddress(module, data_conversion::utf16_to_utf8(injection_options.stomp_function).c_str());
			THROW_LAST_ERROR_IF_NULL(memory);
			THROW_IF_WIN32_BOOL_FALSE(FreeLibrary(module));

			// Make the memory in the target process writable
			DWORD old_protect;
			THROW_IF_WIN32_BOOL_FALSE(VirtualProtectEx(process.get(), memory, shellcode.size(), PAGE_READWRITE, &old_protect));

			// Write shellcode to target process
			THROW_IF_WIN32_BOOL_FALSE(WriteProcessMemory(process.get(), memory, shellcode.data(), shellcode.size(), nullptr));

			// Make shellcode executable
			THROW_IF_WIN32_BOOL_FALSE(VirtualProtectEx(process.get(), memory, shellcode.size(), PAGE_EXECUTE_READ, &old_protect));
		}
		else
		{
			THROW_HR(E_INVALIDARG);
		}

		// Start remote thread to execute shellcode in target process
		wil::unique_handle thread{CreateRemoteThread(process.get(), nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(memory), nullptr, 0, nullptr)};
		THROW_LAST_ERROR_IF_NULL(thread);

		if (injection_options.cleanup)
		{
			// Wait for the remote thread to complete execution
			THROW_LAST_ERROR_IF(WaitForSingleObject(thread.get(), INFINITE) == WAIT_FAILED);

			if (injection_options.allocation_method == AllocationMethod::Alloc)
			{
				// Free the allocated memory in target process
				THROW_IF_WIN32_BOOL_FALSE(VirtualFreeEx(process.get(), memory, 0, MEM_RELEASE));
			}
			else if (injection_options.allocation_method == AllocationMethod::Map)
			{
				// Unmap view from target process
				THROW_IF_WIN32_BOOL_FALSE(UnmapViewOfFile2(process.get(), memory, 0));
			}
			else
			{
				THROW_HR(E_INVALIDARG);
			}
		}
	}

	/*
	* Executes the specified shellcode in the current thread.
	* 
	* Execution options ("injection_options" struct):
	*	allocation_method: Specifies how memory allocation is performed.
	*		Alloc: Uses VirtualAlloc.
	*		Map: Uses MapViewOfFile.
	*		Stomp: Overwrites an existing function. Uses "stomp_module" and "stomp_function" for the module/function to be overwritten (the module is not loaded automatically). Ignores "cleanup".
	*	cleanup: Free the allocated memory when shellcode execution has completed.
	*/
	export void shellcode_execute(std::span<const uint8_t> shellcode, const InjectionOptions& injection_options = {})
	{
		void* memory = nullptr;
		if (injection_options.allocation_method == AllocationMethod::Alloc)
		{
			// Allocate memory for the shellcode
			memory = VirtualAlloc(nullptr, shellcode.size(), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
			THROW_LAST_ERROR_IF_NULL(memory);

			// Copy the shellcode to the allocated memory
			std::memcpy(memory, shellcode.data(), shellcode.size());

			// Make the memory executable
			DWORD old_protect;
			THROW_IF_WIN32_BOOL_FALSE(VirtualProtect(memory, shellcode.size(), PAGE_EXECUTE_READ, &old_protect));
		}
		else if (injection_options.allocation_method == AllocationMethod::Map)
		{
			// Create memory-backed file mapping
			wil::unique_handle mapping{CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_EXECUTE_READWRITE, 0, wil::safe_cast<DWORD>(shellcode.size()), nullptr)};
			THROW_LAST_ERROR_IF_NULL(mapping);

			// Map writeable view into process
			memory = MapViewOfFile(mapping.get(), FILE_MAP_WRITE, 0, 0, 0);
			THROW_LAST_ERROR_IF_NULL(memory);

			// Copy the shellcode to the view
			std::memcpy(memory, shellcode.data(), shellcode.size());

			// Unmap view
			THROW_IF_WIN32_BOOL_FALSE(UnmapViewOfFile(memory));

			// Remap view as executable
			memory = MapViewOfFile(mapping.get(), FILE_MAP_READ | FILE_MAP_EXECUTE, 0, 0, 0);
			THROW_LAST_ERROR_IF_NULL(memory);
		}
		else if (injection_options.allocation_method == AllocationMethod::Stomp)
		{
			// Find function address
			auto module = GetModuleHandleW(injection_options.stomp_module.c_str());
			THROW_LAST_ERROR_IF_NULL(module);
			memory = GetProcAddress(module, data_conversion::utf16_to_utf8(injection_options.stomp_function).c_str());
			THROW_LAST_ERROR_IF_NULL(memory);

			// Make the memory writable
			DWORD old_protect;
			THROW_IF_WIN32_BOOL_FALSE(VirtualProtect(memory, shellcode.size(), PAGE_READWRITE, &old_protect));

			// Overwrite the function with the shellcode
			std::memcpy(memory, shellcode.data(), shellcode.size());

			// Make the memory executable
			THROW_IF_WIN32_BOOL_FALSE(VirtualProtect(memory, shellcode.size(), PAGE_EXECUTE_READ, &old_protect)); 
		}
		else
		{
			THROW_HR(E_INVALIDARG);
		}

		// Execute payload
		reinterpret_cast<void(*)()>(memory)();

		if (injection_options.cleanup)
		{
			if (injection_options.allocation_method == AllocationMethod::Alloc)
			{
				// Free allocated memory
				THROW_IF_WIN32_BOOL_FALSE(VirtualFree(memory, 0, MEM_RELEASE));
			}
			else if (injection_options.allocation_method == AllocationMethod::Map)
			{
				// Unmap view again
				THROW_IF_WIN32_BOOL_FALSE(UnmapViewOfFile(memory));
			}
			else
			{
				THROW_HR(E_INVALIDARG);
			}
		}
	}
}
