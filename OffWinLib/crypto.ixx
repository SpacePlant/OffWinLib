module;
#include <array>
#include <span>
#include <stdint.h>
#include <string>
#include <vector>

export module offwinlib:crypto;

import :data_conversion;

namespace owl::crypto
{
	/*
	* Implements the xorshift32 PRNG algorithm.
	*/
	constexpr void xorshift(uint32_t& state)
	{
		state ^= state << 13;
		state ^= state >> 17;
		state ^= state << 5;
	}

	/*
	* Encrypts or decrypts data using a key stream based on xorshift32.
	*/
	constexpr void crypt(const uint8_t* source, uint8_t* destination, size_t length, uint32_t key)
	{
		for (size_t i = 0; i < length; i++)
		{
			xorshift(key);
			destination[i] = source[i] ^ key;
		}
	}

	/*
	* Encrypts data at compile-time.
	*/
	export template<typename T, size_t N>
		requires std::is_trivially_copyable_v<T>
	consteval std::array<uint8_t, N * sizeof(T)> encrypt_data(const T(&data)[N], uint32_t key)
	{
		constexpr size_t byte_count = N * sizeof(T);
		std::array<uint8_t, byte_count> encrypted_data;

		for (size_t i = 0; i < N; i++)
		{
			auto bytes = data_conversion::value_to_bytes(data[i]);
			for (size_t j = 0; j < sizeof(T); j++)
			{
				encrypted_data[i * sizeof(T) + j] = bytes[j];
			}
		}

		crypt(encrypted_data.data(), encrypted_data.data(), byte_count, key);
		return encrypted_data;
	}

	/*
	* Decrypts data at runtime.
	*/
	export std::vector<uint8_t> decrypt_data(std::span<const uint8_t> data, uint32_t key)
	{
		std::vector<uint8_t> decrypted_data(data.size());
		crypt(data.data(), decrypted_data.data(), data.size(), key);
		return decrypted_data;
	}

	/*
	* Decrypts data to a string at runtime. Removes the last decrypted character, since the "encrypt_data" function will store a null character in the encrypted data when a string literal is supplied.
	*/
	export template<typename T>
		requires std::is_trivially_copyable_v<T>
	std::basic_string<T> decrypt_string(std::span<const uint8_t> data, uint32_t key)
	{
		std::basic_string<T> decrypted_data(data.size() / sizeof(T), T{});
		crypt(data.data(), reinterpret_cast<uint8_t*>(decrypted_data.data()), data.size(), key);
		decrypted_data.pop_back();
		return decrypted_data;
	}
}
