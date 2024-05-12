module;
#pragma comment(lib, "ntdll")

#include <Windows.h>
#include <winternl.h>

#ifndef _DEBUG
#define RESULT_DIAGNOSTICS_LEVEL 0
#endif
#include <wil/resource.h>
#include <wil/result.h>

#include <string>

export module offwinlib:object_manager;

ACCESS_MASK constexpr SYMBOLIC_LINK_ALL_ACCESS = STANDARD_RIGHTS_REQUIRED | 1;
extern "C" NTSTATUS NTAPI NtCreateSymbolicLinkObject(HANDLE* Handle, ACCESS_MASK DesiredAccess, OBJECT_ATTRIBUTES* ObjectAttributes, UNICODE_STRING* DestinationName);

namespace object_manager
{
	/*
	* Creates an object manager symbolic link from the NT path "link" to the NT path "target". The symlink is deleted when the returned handle is closed.
	*/
	export wil::unique_handle create_object_manager_symlink(const std::wstring& link, const std::wstring& target)
	{
		UNICODE_STRING link_string;
		RtlInitUnicodeString(&link_string, link.c_str());
		OBJECT_ATTRIBUTES link_obj;
		InitializeObjectAttributes(&link_obj, &link_string, OBJ_CASE_INSENSITIVE, nullptr, nullptr);
		UNICODE_STRING target_string;
		RtlInitUnicodeString(&target_string, target.c_str());
		wil::unique_handle handle;
		THROW_IF_NTSTATUS_FAILED(NtCreateSymbolicLinkObject(&handle, SYMBOLIC_LINK_ALL_ACCESS, &link_obj, &target_string));
		return handle;
	}
}
