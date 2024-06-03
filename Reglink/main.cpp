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
	std::wcout << L"\tReglink.exe create LINK TARGET" << std::endl;
	std::wcout << L"\tReglink.exe modify LINK TARGET" << std::endl;
	std::wcout << L"\tReglink.exe delete LINK" << std::endl;
}

int wmain(int argc, wchar_t* argv[])
{
	try
	{
		std::vector<std::wstring_view> args(argv, argv + argc);
		if (args.size() < 3)
		{
			print_usage();
			return 1;
		}

		const auto& command = args[1];
		auto link_path = std::wstring{args[2]};
		if (command == L"create")
		{
			if (args.size() < 4)
			{
				print_usage();
				return 1;
			}

			auto target_path = std::wstring{args[3]};

			std::wcout << std::format(LR"([*] Creating registry link from {} to {}...)", link_path, target_path) << std::endl;
			owl::registry::create_registry_symlink_nt(link_path, target_path);
			std::wcout << L"[+] Link created." << std::endl;
		}
		else if (command == L"modify")
		{
			if (args.size() < 4)
			{
				print_usage();
				return 1;
			}

			auto target_path = std::wstring{args[3]};

			std::wcout << std::format(LR"([*] Modifying registry link at {} to point to {}...)", link_path, target_path) << std::endl;
			owl::registry::modify_registry_symlink_nt(link_path, target_path);
			std::wcout << L"[+] Link modified." << std::endl;
		}
		else if (command == L"delete")
		{
			std::wcout << std::format(LR"([*] Deleting registry link {}...)", link_path) << std::endl;
			owl::registry::delete_registry_symlink_nt(link_path);
			std::wcout << L"[+] Link deleted." << std::endl;
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
