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
	/*
	* Represents an opportunistic lock.
	*/
	export class Oplock
	{
		std::unique_ptr<REQUEST_OPLOCK_INPUT_BUFFER> input_buffer;
		std::unique_ptr<REQUEST_OPLOCK_OUTPUT_BUFFER> output_buffer;
		std::unique_ptr<OVERLAPPED> overlapped;
		wil::unique_hfile handle;
		wil::unique_event trigger;

		Oplock(wil::unique_hfile handle, wil::unique_event trigger, std::unique_ptr<REQUEST_OPLOCK_INPUT_BUFFER> input_buffer, std::unique_ptr<REQUEST_OPLOCK_OUTPUT_BUFFER> output_buffer, std::unique_ptr<OVERLAPPED> overlapped) :
			handle{std::move(handle)},
			trigger{std::move(trigger)},
			input_buffer{std::move(input_buffer)},
			output_buffer{std::move(output_buffer)},
			overlapped{std::move(overlapped)}
		{}

	public:
		/*
		* Returns the file handle.
		*/
		HANDLE get_file_handle()
		{
			return handle.get();
		}

		/*
		* Waits for the oplock to trigger.
		*/
		bool wait(DWORD ms = INFINITE)
		{
			return trigger.wait(ms);
		}

		/*
		* Closes the file handle and releases the oplock to let file operations continue.
		*/
		void release()
		{
			handle.reset();
		}

		~Oplock()
		{
			if (!trigger.is_signaled())
			{
				CancelIo(handle.get());
				trigger.wait();
			}
		}
		Oplock(Oplock&&) = default;
		Oplock& operator=(Oplock&&) = default;

		/*
		* Sets an oplock on the file or folder at the specified DOS path.
		*/
		static Oplock set_oplock(const std::wstring& path, DWORD share_mode, bool exclusive, DWORD access_mask = GENERIC_READ, bool delete_on_close = false)
		{
			// Open the path
			DWORD flags =
				FILE_FLAG_OPEN_REPARSE_POINT |
				FILE_FLAG_OVERLAPPED |
				(std::filesystem::is_directory(path) ? FILE_FLAG_BACKUP_SEMANTICS : 0) |
				(delete_on_close ? FILE_FLAG_DELETE_ON_CLOSE : 0);
			auto path_handle = wil::open_file(path.c_str(), access_mask, share_mode, flags);

			// Create an event for when the oplock is triggered
			auto overlapped = std::make_unique<OVERLAPPED>();
			wil::unique_event oplock_trigger{wil::EventOptions::ManualReset};
			overlapped->hEvent = oplock_trigger.get();

			// Request oplock
			auto input_buffer = std::make_unique<REQUEST_OPLOCK_INPUT_BUFFER>(
				REQUEST_OPLOCK_CURRENT_VERSION,
				static_cast<WORD>(sizeof(REQUEST_OPLOCK_INPUT_BUFFER)),
				OPLOCK_LEVEL_CACHE_READ | OPLOCK_LEVEL_CACHE_HANDLE | (exclusive ? DWORD{OPLOCK_LEVEL_CACHE_WRITE} : 0),
				REQUEST_OPLOCK_INPUT_FLAG_REQUEST
			);
			auto output_buffer = std::make_unique<REQUEST_OPLOCK_OUTPUT_BUFFER>(
				REQUEST_OPLOCK_CURRENT_VERSION,
				static_cast<WORD>(sizeof(REQUEST_OPLOCK_OUTPUT_BUFFER))
			);
			if (DeviceIoControl(
				path_handle.get(),
				FSCTL_REQUEST_OPLOCK,
				input_buffer.get(),
				sizeof(REQUEST_OPLOCK_INPUT_BUFFER),
				output_buffer.get(),
				sizeof(REQUEST_OPLOCK_OUTPUT_BUFFER),
				nullptr,
				overlapped.get()
			))  // The DeviceIoControl call should fail with ERROR_IO_PENDING
			{
				THROW_HR(E_FAIL);
			}
			THROW_LAST_ERROR_IF(GetLastError() != ERROR_IO_PENDING);

			return Oplock{std::move(path_handle), std::move(oplock_trigger), std::move(input_buffer), std::move(output_buffer), std::move(overlapped)};
		}
	};
}
