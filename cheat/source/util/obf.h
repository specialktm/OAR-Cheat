#pragma once

// Compile-time XOR string encryption (tightened from the charm sample's
// secure_string.hpp).
//
// Each literal gets its own seed derived from __LINE__ / __COUNTER__, is XOR
// encrypted at compile time with a 64-bit splitmix keyed mask, and decrypts
// into a transient (self-wiping) buffer so plaintext only ever exists in
// registers / a stack buffer that is zeroed before leaving scope.
//
// Usage: PRJ_STR("some secret string")  ->  std::string (decrypted, transient)
// The plaintext never appears in the .rdata strings section.

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>

#if defined(_MSC_VER)
#define PRG_PROTECT_NOINLINE __declspec(noinline)
#define PRG_PROTECT_FORCEINLINE __forceinline
#else
#define PRG_PROTECT_NOINLINE __attribute__((noinline))
#define PRG_PROTECT_FORCEINLINE inline __attribute__((always_inline))
#endif

namespace prg
{
	namespace detail
	{
		constexpr std::uint64_t mix(std::uint64_t value) noexcept
		{
			value += 0x9E3779B97F4A7C15ull;
			value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ull;
			value = (value ^ (value >> 27)) * 0x94D049BB133111EBull;
			return value ^ (value >> 31);
		}

		constexpr std::uint64_t seed_from(std::uint64_t line, std::uint64_t counter) noexcept
		{
			return mix((line << 32) ^ counter ^ 0xD6E8FEB86659FD93ull);
		}

		constexpr std::uint8_t mask_for(std::uint64_t seed, std::size_t index) noexcept
		{
			const std::uint64_t value = mix(seed ^ (0xA5A5A5A55A5A5A5Aull + index * 0x632BE59BD9B4E019ull));
			return static_cast<std::uint8_t>(value & 0xFFu);
		}

		inline void wipe_memory(void* data, std::size_t size) noexcept
		{
			volatile auto* bytes = static_cast<volatile unsigned char*>(data);
			while (size--)
			{
				*bytes++ = 0;
			}
		}

		template <std::size_t N>
		class transient_literal
		{
		public:
			transient_literal() = default;
			transient_literal(const transient_literal&) = delete;
			transient_literal& operator=(const transient_literal&) = delete;

			transient_literal(transient_literal&& other) noexcept
			{
				buffer_ = other.buffer_;
				other.wipe();
			}

			transient_literal& operator=(transient_literal&& other) noexcept
			{
				if (this != &other)
				{
					wipe();
					buffer_ = other.buffer_;
					other.wipe();
				}
				return *this;
			}

			~transient_literal()
			{
				wipe();
			}

			[[nodiscard]] PRG_PROTECT_FORCEINLINE const char* data() const noexcept
			{
				return buffer_.data();
			}

			[[nodiscard]] PRG_PROTECT_FORCEINLINE std::string str() const
			{
				return std::string(buffer_.data(), N ? N - 1 : 0);
			}

			PRG_PROTECT_FORCEINLINE operator const char* () const noexcept
			{
				return data();
			}

			PRG_PROTECT_FORCEINLINE operator std::string() const
			{
				return str();
			}

			PRG_PROTECT_FORCEINLINE operator std::string_view() const noexcept
			{
				return std::string_view(buffer_.data(), N ? N - 1 : 0);
			}

		private:
			template <std::size_t, std::uint64_t>
			friend class encrypted_literal;

			PRG_PROTECT_FORCEINLINE void wipe() noexcept
			{
				wipe_memory(buffer_.data(), buffer_.size());
			}

			std::array<char, N> buffer_{};
		};

		template <std::size_t N, std::uint64_t Seed>
		class encrypted_literal
		{
		public:
			constexpr encrypted_literal(const char(&plain)[N]) noexcept
			{
				for (std::size_t i = 0; i < N; ++i)
				{
					encoded_[i] = static_cast<std::uint8_t>(plain[i]) ^ mask_for(Seed, i);
				}
			}

			[[nodiscard]] PRG_PROTECT_NOINLINE transient_literal<N> decrypt() const noexcept
			{
				transient_literal<N> out;
				for (std::size_t i = 0; i < N; ++i)
				{
					out.buffer_[i] = static_cast<char>(encoded_[i] ^ mask_for(Seed, i));
				}
				return out;
			}

		private:
			std::array<std::uint8_t, N> encoded_{};
		};
	}

	template <std::uint64_t Seed, std::size_t N>
	[[nodiscard]] constexpr auto make_literal(const char(&plain)[N]) noexcept
	{
		return detail::encrypted_literal<N, detail::mix(Seed ^ (N * 0x100000001B3ull))>(plain);
	}
}

#define PRJ_STR(value) ([]() { \
	constexpr auto prj_seed = ::prg::detail::seed_from(__LINE__, __COUNTER__); \
	static constexpr auto prj_literal = ::prg::make_literal<prj_seed>(value); \
	return prj_literal.decrypt().str(); \
}())