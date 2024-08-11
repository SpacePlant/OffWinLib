#include <exception>
#include <format>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

import offwinlib;

static void print_usage()
{
	std::wcout << L"Usage" << std::endl;
	std::wcout << L"\tDLLInjector.exe pid DLL_PATH PID" << std::endl;
	std::wcout << L"\tDLLInjector.exe name DLL_PATH PROCESS_NAME" << std::endl;
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
		auto dll_path = std::wstring(args[2]);
		if (command == L"pid")
		{
			auto pid = std::stoi(std::wstring{args[3]});
			std::wcout << std::format(LR"([*] Injecting {} into process with pid {}...)", dll_path, pid) << std::endl;
			owl::injection::dll_inject(pid, dll_path);
			std::wcout << L"[+] DLL injected." << std::endl;
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

			std::wcout << std::format(LR"([*] Injecting {} into process with pid {}...)", dll_path, pids[0]) << std::endl;
			owl::injection::dll_inject(pids[0], dll_path);
			std::wcout << L"[+] DLL injected." << std::endl;
		}
		else
		{
			print_usage();
			return 1;
		}
	}
	catch (const std::exception& e)
	{
		std::wcout << L"[-] " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
