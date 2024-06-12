#include <exception>
#include <filesystem>
#include <format>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

using namespace std::string_literals;

import offwinlib;

static void print_usage()
{
	std::wcout << L"Usage: Symlink.exe LINK TARGET" << std::endl;
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

		const auto& link_path = args[1];
		const auto& target_path = args[2];

		auto slash = link_path.find_last_of(LR"(/\)");
		auto folder_path = std::wstring{link_path.substr(0, slash)};
		auto file_name = link_path.substr(slash + 1);

		auto directory_object = LR"(\BaseNamedObjects)"s;
		std::wcout << std::format(L"[*] Creating directory object in {}...", directory_object) << std::endl;
		directory_object += LR"(\)" + owl::misc::generate_uuid();
		auto directory_handle = owl::object_manager::create_directory_object(directory_object);
		std::wcout << std::format(L"[+] Directory object created: {}", directory_object) << std::endl;

		std::wcout << std::format(L"[*] Creating junction from {} to {}...", folder_path, directory_object) << std::endl;
		auto folder_created = owl::junction::create_junction(folder_path, directory_object);
		std::wcout << std::format(L"[+] Junction created. Folder created: {}", folder_created) << std::endl;

		auto symlink_path = (directory_object + LR"(\)").append(file_name);
		auto target_path_nt = LR"(\??\)"s.append(target_path);
		std::wcout << std::format(L"[*] Creating symlink from {} to {}...", symlink_path, target_path_nt) << std::endl;
		auto symlink_handle = owl::object_manager::create_object_manager_symlink(symlink_path, target_path_nt);
		std::wcout << L"[+] Symlink created. Press enter to clean up...";
		std::cin.get();

		std::wcout << L"[*] Removing junction..." << std::endl;
		if (folder_created)
		{
			std::filesystem::remove(folder_path);
			std::wcout << L"[+] Junction deleted." << std::endl;
		}
		else
		{
			owl::junction::delete_reparse_point_from_junction(folder_path);
			std::wcout << L"[+] Reparse point deleted." << std::endl;
		}
	}
	catch (const std::exception& e)
	{
		std::wcout << L"[-] " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
