module;
#include <Windows.h>

#include <wil/result.h>

#include <optional>
#include <span>
#include <sstream>
#include <stdint.h>
#include <string>
#include <tuple>
#include <vector>

export module offwinlib:memory;

namespace owl::memory
{
	/*
	* Looks for "signature" from "address" to "address" + "range". If the signature is found, the address of it is returned with "offset" added. Otherwise, a null pointer is returned.
	* 
	* The signature should be in the format "12 56 9A DE ...". "??" can be used as a wildcard byte.
	*/
	export uint8_t* find_signature(uint8_t* address, uintptr_t range, intptr_t offset, const std::wstring& signature)
	{
		// Convert the string signature to a vector of optional bytes
		std::vector<std::optional<uint8_t>> signature_bytes;
		std::wstringstream signature_stream{signature};
		std::wstring signature_byte;
		while (signature_stream >> signature_byte)
		{
			if (signature_byte == L"??")
			{
				signature_bytes.push_back(std::nullopt);
			}
			else
			{
				signature_bytes.push_back(std::stoi(signature_byte, nullptr, 16));
			}
		}

		// Search for the signature in the specified memory area
		auto address_end = address + range - signature_bytes.size();
		for (; address <= address_end; address++)
		{
			bool success = true;
			for (auto [it, i] = std::tuple{signature_bytes.cbegin(), uintptr_t{0}}; it != signature_bytes.cend(); it++, i++)
			{
				if (*it && *(address + i) != *it)
				{
					success = false;
					break;
				}
			}
			if (success)
			{
				return address + offset;
			}
		}

		// Signature not found
		return nullptr;
	}

	/*
	* Overwrites the data at "address" with "bytes". The memory protection is changed to "intermediate_protection" for writing the data and reverted afterwards.
	*/
	export void patch(void* address, std::span<const uint8_t> bytes, DWORD intermediate_protection = PAGE_EXECUTE_READWRITE)
	{
		// Make the memory writable
		DWORD old_protect;
		THROW_IF_WIN32_BOOL_FALSE(VirtualProtect(address, bytes.size(), intermediate_protection, &old_protect));

		// Patch the memory
		std::memcpy(address, bytes.data(), bytes.size());

		// Revert the memory protection
		THROW_IF_WIN32_BOOL_FALSE(VirtualProtect(address, bytes.size(), old_protect, &old_protect));
	}
}
