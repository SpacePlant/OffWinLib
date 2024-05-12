module;
#pragma comment(lib, "ntdll")

#include <Windows.h>
#include <winternl.h>

#ifndef _DEBUG
#define RESULT_DIAGNOSTICS_LEVEL 0
#endif
#include <wil/resource.h>
#include <wil/result.h>
#include <wil/safecast.h>

#include <string>
#include <string_view>

export module offwinlib:registry;

extern "C"
{
	NTSTATUS NTAPI NtCreateKey(HANDLE* KeyHandle, ACCESS_MASK DesiredAccess, OBJECT_ATTRIBUTES* ObjectAttributes, ULONG TitleIndex, UNICODE_STRING* Class, ULONG CreateOptions, ULONG* Disposition);
	NTSTATUS NTAPI NtOpenKey(HANDLE* KeyHandle, ACCESS_MASK DesiredAccess, OBJECT_ATTRIBUTES* ObjectAttributes);
	NTSTATUS NTAPI NtSetValueKey(HANDLE KeyHandle, UNICODE_STRING* ValueName, ULONG TitleIndex, ULONG Type, void* Data, ULONG DataSize);
	NTSTATUS NTAPI NtDeleteKey(HANDLE KeyHandle);
}

namespace registry
{
	/*
	* Creates a registry symbolic link from "root_key"\"sub_key" to "target". "root_key" can be a predefined key, e.g. "HKEY_LOCAL_MACHINE". "target" should be an absolute registry path.
	*/
	export void create_registry_symlink(HKEY root_key, const std::wstring& sub_key, std::wstring_view target)
	{
		// Create registry symlink
		wil::unique_hkey link;
		THROW_IF_WIN32_ERROR(RegCreateKeyExW(root_key, sub_key.c_str(), 0, NULL, REG_OPTION_CREATE_LINK, KEY_WRITE, NULL, &link, NULL));

		// Set symlink target
		THROW_IF_WIN32_ERROR(RegSetValueExW(link.get(), L"SymbolicLinkValue", 0, REG_LINK, reinterpret_cast<const BYTE*>(target.data()), wil::safe_cast<DWORD>(target.length() * sizeof(wchar_t))));
	}

	/*
	* Creates a registry symbolic link from "link" to "target". Both arguments should be absolute registry paths.
	*/
	export void create_registry_symlink_nt(const std::wstring& link, std::wstring target)
	{
		// Create registry symlink
		wil::unique_handle key;
		UNICODE_STRING key_name;
		RtlInitUnicodeString(&key_name, link.c_str());
		OBJECT_ATTRIBUTES key_name_obj;
		InitializeObjectAttributes(&key_name_obj, &key_name, OBJ_CASE_INSENSITIVE, nullptr, nullptr);
		THROW_IF_NTSTATUS_FAILED(NtCreateKey(&key, KEY_SET_VALUE, &key_name_obj, 0, nullptr, REG_OPTION_CREATE_LINK, nullptr));

		// Set symlink target
		UNICODE_STRING link_value;
		RtlInitUnicodeString(&link_value, L"SymbolicLinkValue");
		THROW_IF_NTSTATUS_FAILED(NtSetValueKey(key.get(), &link_value, 0, REG_LINK, reinterpret_cast<void*>(target.data()), wil::safe_cast<ULONG>(target.length() * sizeof(wchar_t))));
	}

	/*
	* Modifies the existing registry symbolic link "root_key"\"sub_key" to point to "target". "root_key" can be a predefined key, e.g. "HKEY_LOCAL_MACHINE". "target" should be an absolute registry path.
	*/
	export void modify_registry_symlink(HKEY root_key, const std::wstring& sub_key, std::wstring_view target)
	{
		// Open existing registry symlink
		wil::unique_hkey link;
		THROW_IF_WIN32_ERROR(RegOpenKeyExW(root_key, sub_key.c_str(), REG_OPTION_OPEN_LINK, KEY_WRITE, &link));

		// Change symlink target
		THROW_IF_WIN32_ERROR(RegSetValueExW(link.get(), L"SymbolicLinkValue", 0, REG_LINK, reinterpret_cast<const BYTE*>(target.data()), wil::safe_cast<DWORD>(target.length() * sizeof(wchar_t))));
	}

	/*
	* Modifies the existing registry symbolic link "link" to point to "target". Both arguments should be absolute registry paths.
	*/
	export void modify_registry_symlink_nt(const std::wstring& link, std::wstring target)
	{
		// Open existing registry symlink
		wil::unique_handle key;
		UNICODE_STRING key_name;
		RtlInitUnicodeString(&key_name, link.c_str());
		OBJECT_ATTRIBUTES key_name_obj;
		InitializeObjectAttributes(&key_name_obj, &key_name, OBJ_CASE_INSENSITIVE | OBJ_OPENLINK, nullptr, nullptr);
		THROW_IF_NTSTATUS_FAILED(NtOpenKey(&key, KEY_SET_VALUE, &key_name_obj));

		// Change symlink target
		UNICODE_STRING link_value;
		RtlInitUnicodeString(&link_value, L"SymbolicLinkValue");
		THROW_IF_NTSTATUS_FAILED(NtSetValueKey(key.get(), &link_value, 0, REG_LINK, reinterpret_cast<void*>(target.data()), wil::safe_cast<ULONG>(target.length() * sizeof(wchar_t))));
	}

	/*
	* Deletes the specified registry symbolic link, which should be an absolute registry path.
	*/
	export void delete_registry_symlink_nt(const std::wstring& link)
	{
		// Open registry symlink
		wil::unique_handle key;
		UNICODE_STRING key_name;
		RtlInitUnicodeString(&key_name, link.c_str());
		OBJECT_ATTRIBUTES key_name_obj;
		InitializeObjectAttributes(&key_name_obj, &key_name, OBJ_CASE_INSENSITIVE | OBJ_OPENLINK, nullptr, nullptr);
		THROW_IF_NTSTATUS_FAILED(NtOpenKey(&key, DELETE, &key_name_obj));

		// Delete symlink
		THROW_IF_NTSTATUS_FAILED(NtDeleteKey(key.get()));
	}
}
