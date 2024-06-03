module;
#include <Windows.h>

#ifndef _DEBUG
#define RESULT_DIAGNOSTICS_LEVEL 0
#endif
#include <wil/filesystem.h>
#include <wil/resource.h>
#include <wil/result.h>
#include <wil/safecast.h>

#include <filesystem>
#include <stdint.h>
#include <string>
#include <vector>

// Suppress warning about "unsized array" in struct
#pragma warning(disable: 4200)

export module offwinlib:junction;

struct REPARSE_DATA_BUFFER_MOUNT_POINT
{
	ULONG ReparseTag;
	USHORT ReparseDataLength;
	USHORT Reserved;
	struct
	{
		USHORT SubstituteNameOffset;
		USHORT SubstituteNameLength;
		USHORT PrintNameOffset;
		USHORT PrintNameLength;
		wchar_t PathBuffer[];
	} MountPointReparseBuffer;
};

namespace owl::junction
{
	/*
	* Creates a junction from the DOS path "junction" to the NT path "target". Will modify an existing junction. Returns true if a folder was created.
	*/
	export bool create_junction(const std::wstring& junction, const std::wstring& target)
	{
		// Create junction folder
		bool folder_created = std::filesystem::create_directory(junction);
		auto folder = wil::open_file(junction.c_str(), GENERIC_WRITE, 0, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT);

		// Construct reparse data
		auto substitute_name_buffer_length = (target.length() + 1) * sizeof(wchar_t);
		auto print_name_buffer_length = sizeof(wchar_t);  // The print name string is empty, so it's just the null terminator
		auto path_buffer_length = substitute_name_buffer_length + print_name_buffer_length;  // Both the substitute name string and the print name string have to be null-terminated, even if the documentation says otherwise...
		std::vector<uint8_t> reparse_data_buffer(sizeof(REPARSE_DATA_BUFFER_MOUNT_POINT) + path_buffer_length);
		auto reparse_data = reinterpret_cast<REPARSE_DATA_BUFFER_MOUNT_POINT*>(reparse_data_buffer.data());
		reparse_data->ReparseTag = IO_REPARSE_TAG_MOUNT_POINT;
		reparse_data->ReparseDataLength = wil::safe_cast<USHORT>(sizeof(reparse_data->MountPointReparseBuffer) + path_buffer_length);
		reparse_data->MountPointReparseBuffer.SubstituteNameOffset = 0;
		reparse_data->MountPointReparseBuffer.SubstituteNameLength = wil::safe_cast<USHORT>(substitute_name_buffer_length - sizeof(wchar_t));  // Doesn't include the null terminator
		reparse_data->MountPointReparseBuffer.PrintNameOffset = wil::safe_cast<USHORT>(substitute_name_buffer_length);
		reparse_data->MountPointReparseBuffer.PrintNameLength = 0;  // Doesn't include the null terminator
		target.copy(reparse_data->MountPointReparseBuffer.PathBuffer, target.length());  // The buffer is already zero-initialized, so we don't have to worry about writing the null terminators

		// Set reparse point
		THROW_IF_WIN32_BOOL_FALSE(DeviceIoControl(folder.get(), FSCTL_SET_REPARSE_POINT, reparse_data, wil::safe_cast<DWORD>(reparse_data_buffer.size()), nullptr, 0, nullptr, nullptr));
		return folder_created;
	}

	/*
	* Deletes the reparse point from the junction at the specified DOS path, turning it into a regular folder.
	*/
	export void delete_reparse_point_from_junction(const std::wstring& junction)
	{
		// Open junction
		auto junction_handle = wil::open_file(junction.c_str(), GENERIC_WRITE, 0, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT);

		// Delete reparse point
		REPARSE_GUID_DATA_BUFFER reparse_data{};
		reparse_data.ReparseTag = IO_REPARSE_TAG_MOUNT_POINT;
		THROW_IF_WIN32_BOOL_FALSE(DeviceIoControl(junction_handle.get(), FSCTL_DELETE_REPARSE_POINT, &reparse_data, REPARSE_GUID_DATA_BUFFER_HEADER_SIZE, nullptr, 0, nullptr, nullptr));
	}
}
