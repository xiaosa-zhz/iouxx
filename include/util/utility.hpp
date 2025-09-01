#ifndef IOUXX_UTILITY_H
#define IOUXX_UTILITY_H 1

#include "linux/time_types.h"
#include "bits/types/struct_iovec.h"
#include "sys/socket.h"

#include <utility>
#include <chrono>
#include <system_error>
#include <span>

namespace iouxx::utility {

    template<typename F>
    struct defer {
        ~defer() { this->f(); }
        F f;
    };

    template<typename>
    inline constexpr bool always_false = false;

    struct system_addrsock_info {
        ::sockaddr* addr;
        std::size_t addrlen;
    };

    inline ::__kernel_timespec to_kernel_timespec(std::chrono::nanoseconds stdtime) noexcept {
        ::__kernel_timespec ts;
        const auto sec
            = std::chrono::duration_cast<std::chrono::seconds>(stdtime);
        const auto nsec = stdtime - sec;
        ts.tv_sec = sec.count();
        ts.tv_nsec = nsec.count();
        return ts;
    }

    inline std::chrono::nanoseconds from_kernel_timespec(const ::__kernel_timespec& ts) noexcept {
        return std::chrono::seconds(ts.tv_sec) + std::chrono::nanoseconds(ts.tv_nsec);
    }

    // Pre: ev >= 0
    inline std::error_code make_system_error_code(int ev) noexcept {
        if (ev != 0) {
            return std::error_code(ev, std::system_category());
        }
        return std::error_code();
    }

    inline std::error_code make_invalid_argument_error() noexcept {
        return std::make_error_code(std::errc::invalid_argument);
    }

    template<typename T>
    concept byte_unit = std::same_as<T, std::byte> || std::same_as<T, unsigned char>;

    template<typename R>
    concept byte_span_like = std::constructible_from<std::span<std::byte>, R>;

    template<typename R>
    concept char_span_like = std::constructible_from<std::span<unsigned char>, R>;

    template<typename R>
    concept readonly_byte_span_like = std::constructible_from<std::span<const std::byte>, R>;

    template<typename R>
    concept readonly_char_span_like = std::constructible_from<std::span<const unsigned char>, R>;

    template<typename R>
    concept buffer_like = byte_span_like<R> || char_span_like<R>;

    template<typename R>
    concept readonly_buffer_like = readonly_byte_span_like<R> || readonly_char_span_like<R>;

    template<buffer_like R>
    inline auto to_buffer(R&& r) noexcept {
        if constexpr (byte_span_like<R>) {
            return std::span<std::byte>(std::forward<R>(r));
        } else if constexpr (char_span_like<R>) {
            return std::span<unsigned char>(std::forward<R>(r));
        } else {
            static_assert(always_false<R>, "Unreachable");
        }
    }

    template<buffer_like R>
    inline auto to_readonly_buffer(R&& r) noexcept {
        if constexpr (readonly_byte_span_like<R>) {
            return std::span<const std::byte>(std::forward<R>(r));
        } else if constexpr (readonly_char_span_like<R>) {
            return std::span<const unsigned char>(std::forward<R>(r));
        } else {
            static_assert(always_false<R>, "Unreachable");
        }
    }

    template<typename R>
    concept buffer_range = std::ranges::input_range<std::remove_cvref_t<R>>
        && buffer_like<std::ranges::range_value_t<std::remove_cvref_t<R>>>;

    template<byte_unit ByteType, std::size_t N>
    inline ::iovec to_iovec(std::span<ByteType, N> buffer) noexcept {
        return ::iovec{
            .iov_base = buffer.data(),
            .iov_len = buffer.size()
        };
    }

    template<byte_unit ByteType>
    inline std::span<ByteType> from_iovec(const ::iovec& iov) noexcept {
        return std::span<ByteType>(
            static_cast<ByteType*>(iov.iov_base), iov.iov_len);
    }

} // namespace iouxx::utility

#endif // IOUXX_UTILITY_H
