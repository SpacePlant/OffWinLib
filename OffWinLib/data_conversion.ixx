module;
#include <Windows.h>

#include <wil/result.h>
#include <wil/safecast.h>

#include <array>
#include <bit>
#include <string>
#include <string_view>

export module offwinlib:data_conversion;

namespace owl::data_conversion
{
	/*
	* Converts a UTF-16 encoded wide string to a UTF-8 encoded multi-byte string.
	*/
	export std::string utf16_to_utf8(std::wstring_view s)
	{
		// Get required string size
		auto required_size = WideCharToMultiByte(CP_UTF8, 0, s.data(), wil::safe_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
		THROW_LAST_ERROR_IF(!required_size);

		// Convert string
		std::string converted(required_size, 0);
		THROW_LAST_ERROR_IF(!WideCharToMultiByte(CP_UTF8, 0, s.data(), wil::safe_cast<int>(s.size()), converted.data(), wil::safe_cast<int>(converted.length()), nullptr, nullptr));
		return converted;
	}

	/*
	* Converts a UTF-8 encoded multi-byte string to a UTF-16 encoded wide string.
	*/
	export std::wstring utf8_to_utf16(std::string_view s)
	{
		// Get required string size
		auto required_size = MultiByteToWideChar(CP_UTF8, 0, s.data(), wil::safe_cast<int>(s.size()), nullptr, 0);
		THROW_LAST_ERROR_IF(!required_size);

		// Convert string
		std::wstring converted(required_size, 0);
		THROW_LAST_ERROR_IF(!MultiByteToWideChar(CP_UTF8, 0, s.data(), wil::safe_cast<int>(s.size()), converted.data(), wil::safe_cast<int>(converted.length())));
		return converted;
	}

	/*
	* Converts a value to an array of bytes.
	*/
	export auto value_to_bytes(auto value)
	{
		return std::bit_cast<std::array<uint8_t, sizeof(value)>>(value);
	}
}
