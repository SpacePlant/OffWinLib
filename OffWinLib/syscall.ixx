module;
#include <Windows.h>

#include <wil/result.h>

#include <map>
#include <string>
#include <utility>

export module offwinlib:syscall;

import :data_conversion;
import :memory;

namespace owl::syscall
{
	/*
	* Helper type that allows us to use string literals as non-type template parameters.
	*/
	template <size_t N>
	struct string_literal
	{
		wchar_t str[N];
		constexpr string_literal(const wchar_t(&arr)[N])
		{
			std::copy_n(arr, N, str);
		}
	};

	/*
	* Helper function to perform indirect syscalls implemented in "syscall_32.asm" and "syscall_64.asm".
	*/
	extern "C" NTSTATUS indirect_syscall(size_t ssn, void* stub, size_t argc, ...);

	std::map<std::wstring, std::pair<uintptr_t, size_t>> syscalls;
	void* syscall_stub;

	/*
	* Initializes SSNs and a suitable syscall stub. The syscall stub is found in the "stub_function" Nt function.
	*/
	export void initialize_syscalls(const std::wstring& stub_function)
	{
		// Parse exported functions from "ntdll"
		auto ntdll = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"ntdll"));
		THROW_LAST_ERROR_IF(!ntdll);
		auto dos_header = reinterpret_cast<PIMAGE_DOS_HEADER>(ntdll);
		auto nt_header = reinterpret_cast<PIMAGE_NT_HEADERS>(ntdll + dos_header->e_lfanew);
		auto export_dir = reinterpret_cast<PIMAGE_EXPORT_DIRECTORY>(ntdll + nt_header->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
		auto function_names = reinterpret_cast<DWORD*>(ntdll + export_dir->AddressOfNames);
		auto function_addresses = reinterpret_cast<DWORD*>(ntdll + export_dir->AddressOfFunctions);
		auto function_ordinals = reinterpret_cast<WORD*>(ntdll + export_dir->AddressOfNameOrdinals);

		// Find all functions starting with "Zw" and sort them by address (replacing "Zw" with "Nt")
		std::map<uintptr_t, std::wstring> functions;
		for (DWORD i = 0; i < export_dir->NumberOfNames; i++)
		{
			auto function_name = std::string(reinterpret_cast<char*>(ntdll + function_names[i]));
			if (!function_name.starts_with("Zw"))
			{
				continue;
			}

			auto function_address = ntdll + function_addresses[function_ordinals[i]];
			function_name.replace(0, 2, "Nt");
			functions.insert({ function_address, data_conversion::utf8_to_utf16(function_name) });
		}

		// Map function names to address and SSN
		for (size_t ssn = 0; const auto& [function_address, function_name] : functions)
		{
			syscalls.insert({ function_name, { function_address, ssn } });
			ssn++;
		}

		// Find syscall stub
	#ifdef _WIN64
		syscall_stub = memory::find_signature(reinterpret_cast<uint8_t*>(syscalls.at(stub_function).first), 32, 0, L"0F 05 C3");
	#else
		syscall_stub = memory::find_signature(reinterpret_cast<uint8_t*>(syscalls.at(stub_function).first), 16, -5, L"FF D2 C2");
	#endif
		THROW_HR_IF_NULL(E_POINTER, syscall_stub);
	}

	/*
	* Performs the "F" (indirect) syscall with "args" as arguments.
	* 
	* Note: Make sure that "initialize_syscalls" has been called first!
	*/
	export template <const string_literal F, typename... Args>
	NTSTATUS call(const Args&... args)
	{
		auto ssn = syscalls.at(F.str).second;
		return indirect_syscall(ssn, syscall_stub, sizeof...(args), args...);
	}
}
