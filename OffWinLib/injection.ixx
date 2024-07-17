module;
#include <Windows.h>

#include <wil/resource.h>
#include <wil/result.h>

#include <span>
#include <string>

export module offwinlib:injection;

namespace owl::injection
{
	/*
	* Injects the DLL located at "dll_path" into the process with PID "process_id". If "wait_for_completion" is set, the function waits for LoadLibraryW to complete and frees the allocated memory.
	*/
	export void dll_inject(DWORD process_id, const std::wstring& dll_path, bool wait_for_completion)
	{
		// Open target process handle
		wil::unique_handle process{OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, process_id)};
		THROW_LAST_ERROR_IF_NULL(process);

		// Allocate memory in target process
		auto data_size = (dll_path.length() + 1) * sizeof(wchar_t);
		auto memory = VirtualAllocEx(process.get(), nullptr, data_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		THROW_LAST_ERROR_IF_NULL(memory);

		// Write DLL path to target process
		THROW_IF_WIN32_BOOL_FALSE(WriteProcessMemory(process.get(), memory, dll_path.c_str(), data_size, nullptr));

		// Locate LoadLibraryW
		auto kernel32 = GetModuleHandleA("kernel32");
		THROW_LAST_ERROR_IF_NULL(kernel32);
		auto loadlibrary = GetProcAddress(kernel32, "LoadLibraryW");
		THROW_LAST_ERROR_IF_NULL(loadlibrary);

		// Start remote thread to execute LoadLibraryW in target process
		wil::unique_handle thread{CreateRemoteThread(process.get(), nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(loadlibrary), memory, 0, nullptr)};
		THROW_LAST_ERROR_IF_NULL(thread);

		if (wait_for_completion)
		{
			// Wait for the remote thread to complete execution
			THROW_LAST_ERROR_IF(WaitForSingleObject(thread.get(), INFINITE) == WAIT_FAILED);

			// Free the allocated memory in target process
			THROW_IF_WIN32_BOOL_FALSE(VirtualFreeEx(process.get(), memory, 0, MEM_RELEASE));
		}
	}

	/*
	* Injects the shellcode into the process with PID "process_id". If "wait_for_completion" is set, the function waits for shellcode execution to complete and frees the allocated memory.
	*/
	export void shellcode_inject(DWORD process_id, std::span<const uint8_t> shellcode, bool wait_for_completion)
	{
		// Open target process handle
		wil::unique_handle process{OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, process_id)};
		THROW_LAST_ERROR_IF_NULL(process);

		// Allocate memory in target process
		auto memory = VirtualAllocEx(process.get(), nullptr, shellcode.size(), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		THROW_LAST_ERROR_IF_NULL(memory);

		// Write shellcode to target process
		THROW_IF_WIN32_BOOL_FALSE(WriteProcessMemory(process.get(), memory, shellcode.data(), shellcode.size(), nullptr));

		// Make shellcode executable
		DWORD old_protect;
		THROW_IF_WIN32_BOOL_FALSE(VirtualProtectEx(process.get(), memory, shellcode.size(), PAGE_EXECUTE_READ, &old_protect));

		// Start remote thread to execute shellcode in target process
		wil::unique_handle thread{CreateRemoteThread(process.get(), nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(memory), nullptr, 0, nullptr)};
		THROW_LAST_ERROR_IF_NULL(thread);

		if (wait_for_completion)
		{
			// Wait for the remote thread to complete execution
			THROW_LAST_ERROR_IF(WaitForSingleObject(thread.get(), INFINITE) == WAIT_FAILED);

			// Free the allocated memory in target process
			THROW_IF_WIN32_BOOL_FALSE(VirtualFreeEx(process.get(), memory, 0, MEM_RELEASE));
		}
	}
}
