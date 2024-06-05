#include <Windows.h>

#include <exception>
#include <format>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

import offwinlib;

static void print_usage()
{
	std::wcout << L"Usage: Oplock.exe PATH [rwdx]" << std::endl;
}

int wmain(int argc, wchar_t* argv[])
{
	try
	{
		std::vector<std::wstring_view> args(argv, argv + argc);
		if (args.size() < 2)
		{
			print_usage();
			return 1;
		}

		const auto& path = args[1];
		DWORD share_mode = 0;
		bool exclusive = false;
		if (args.size() > 2)
		{
			const auto& flags = args[2];
			if (flags.find(L"r") != std::string::npos)
			{
				share_mode |= FILE_SHARE_READ;
			}
			if (flags.find(L"w") != std::string::npos)
			{
				share_mode |= FILE_SHARE_WRITE;
			}
			if (flags.find(L"d") != std::string::npos)
			{
				share_mode |= FILE_SHARE_DELETE;
			}
			if (flags.find(L"x") != std::string::npos)
			{
				exclusive = true;
			}
		}

		std::wcout << std::format(LR"([*] Setting oplock on {} with share mode {} and exclusive mode set to {}...)", path, share_mode, exclusive) << std::endl;
		auto oplock_data = owl::oplock::set_oplock(std::wstring{path}, share_mode, exclusive);
		std::wcout << L"[+] Oplock set." << std::endl;

		std::wcout << L"[*] Waiting for oplock to trigger..." << std::endl;
		oplock_data.trigger.wait();
		std::wcout << L"[+] Oplock triggered. Press enter to release handle...";
		std::cin.get();
	}
	catch (const std::exception& e)
	{
		std::wcout << L"[-] " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
