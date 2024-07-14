module;
#include <Windows.h>

#include <wil/filesystem.h>
#include <wil/resource.h>
#include <wil/result.h>

#include <filesystem>
#include <memory>
#include <string>

export module offwinlib:oplock;

namespace owl::oplock
{
	export class oplock_data
	{
		// Ordered for correct destruction order
		std::unique_ptr<OVERLAPPED> overlapped;
	public:
		wil::unique_event trigger;
		wil::unique_hfile handle;

		oplock_data(wil::unique_hfile handle, std::unique_ptr<OVERLAPPED> overlapped, wil::unique_event trigger) : handle{std::move(handle)}, overlapped{std::move(overlapped)}, trigger{std::move(trigger)} {};
	};

	/*
	* Sets an oplock on the file or folder at the specified DOS path. Returns a struct with a handle to the path and an event that signals when the oplock is triggered. Close the handle to let file operations continue.
	*/
	export oplock_data set_oplock(const std::wstring& path, DWORD share_mode, bool exclusive, DWORD access_mask = GENERIC_READ, bool delete_on_close = false)
	{
		// Open the path
		DWORD flags = FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_OVERLAPPED | (std::filesystem::is_directory(path) ? FILE_FLAG_BACKUP_SEMANTICS : 0) | (delete_on_close ? FILE_FLAG_DELETE_ON_CLOSE : 0);
		auto path_handle = wil::open_file(path.c_str(), access_mask, share_mode, flags);

		// Create an event for when the oplock is triggered
		auto overlapped = std::make_unique<OVERLAPPED>();
		wil::unique_event oplock_trigger{wil::EventOptions::ManualReset};
		overlapped->hEvent = oplock_trigger.get();

		// Request oplock
		REQUEST_OPLOCK_INPUT_BUFFER input_buffer
		{
			.StructureVersion = REQUEST_OPLOCK_CURRENT_VERSION,
			.StructureLength = sizeof(REQUEST_OPLOCK_INPUT_BUFFER),
			.RequestedOplockLevel = OPLOCK_LEVEL_CACHE_READ | OPLOCK_LEVEL_CACHE_HANDLE | (exclusive ? DWORD{OPLOCK_LEVEL_CACHE_WRITE} : 0),
			.Flags = REQUEST_OPLOCK_INPUT_FLAG_REQUEST
		};
		REQUEST_OPLOCK_OUTPUT_BUFFER output_buffer
		{
			.StructureVersion = REQUEST_OPLOCK_CURRENT_VERSION,
			.StructureLength = sizeof(REQUEST_OPLOCK_OUTPUT_BUFFER)
		};
		if (DeviceIoControl(path_handle.get(), FSCTL_REQUEST_OPLOCK, &input_buffer, sizeof(input_buffer), &output_buffer, sizeof(output_buffer), nullptr, overlapped.get()))  // The DeviceIoControl call should fail with ERROR_IO_PENDING
		{
			THROW_HR(E_FAIL);
		}
		THROW_LAST_ERROR_IF(GetLastError() != ERROR_IO_PENDING);

		return oplock_data{std::move(path_handle), std::move(overlapped), std::move(oplock_trigger)};
	}
}
