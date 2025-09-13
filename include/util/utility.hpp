#pragma once
#ifndef IOUXX_UTILITY_H
#define IOUXX_UTILITY_H 1

#ifndef IOUXX_USE_CXX_MODULE

#include "linux/time_types.h"
#include "bits/types/struct_iovec.h"
#include "sys/socket.h"

#include <concepts>
#include <type_traits>
#include <utility>
#include <expected>
#include <chrono>
#include <system_error>
#include <expected>
#include <span>

#include "macro_config.hpp" // IWYU pragma: export
#include "cxxmodule_helper.hpp" // IWYU pragma: export

#endif // IOUXX_USE_CXX_MODULE

IOUXX_EXPORT
namespace iouxx::utility {

    template<typename Defer>
    struct defer {
        ~defer() { this->f(); }
        Defer f;
    };

    template<typename>
    inline constexpr bool always_false = false;

    struct system_addrsock_info {
        ::sockaddr* addr;
        std::size_t addrlen;
    };

    constexpr auto to_kernel_timespec(std::chrono::nanoseconds stdtime)
        noexcept -> ::__kernel_timespec {
        ::__kernel_timespec ts;
        const auto sec
            = std::chrono::duration_cast<std::chrono::seconds>(stdtime);
        const auto nsec = stdtime - sec;
        ts.tv_sec = sec.count();
        ts.tv_nsec = nsec.count();
        return ts;
    }

    constexpr auto from_kernel_timespec(const ::__kernel_timespec& ts)
        noexcept -> std::chrono::nanoseconds {
        return std::chrono::seconds(ts.tv_sec) + std::chrono::nanoseconds(ts.tv_nsec);
    }

    // Pre: ev >= 0
    constexpr std::error_code make_system_error_code(int ev) noexcept {
        if (ev != 0) {
            return std::error_code(ev, std::system_category());
        }
        return std::error_code();
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

    template<template<typename...> class tmp, typename T>
    inline constexpr bool is_specialization_of_v = false;

    template<template<typename...> class tmp, typename... Args>
    inline constexpr bool is_specialization_of_v<tmp, tmp<Args...>> = true;

    template<typename T>
    concept expected_like = is_specialization_of_v<std::expected, std::remove_cvref_t<T>>;

    template<typename Void>
    concept void_like = std::is_void_v<Void>;

    template<typename Callable, typename... Args>
    concept nothrow_invocable = std::invocable<Callable, Args...>
        && std::is_nothrow_invocable_v<Callable, Args...>;

    template<typename Callback>
    concept errorcode_callback = std::invocable<Callback&, std::error_code>;

    template<typename Callback, typename Result>
    concept callback = void_like<Callback>
        || (std::invocable<Callback&, std::unexpected<std::error_code>>
        && std::invocable<Callback&, std::expected<Result, std::error_code>>
        && (void_like<Result> || std::invocable<Callback&, Result>));

    template<typename Callback, typename Result>
    concept eligible_callback = (callback<Callback, Result>)
        || (void_like<Result> && errorcode_callback<Callback>);

    template<typename Callback, typename Result>
    concept eligible_maybe_void_callback = void_like<Callback>
        || eligible_callback<Callback, Result>;

    template<typename Callback>
    concept nothrow_errorcode_callback = nothrow_invocable<Callback&, std::error_code>;

    template<typename Callback>
    concept nothrow_unexpected_callback =
        nothrow_invocable<Callback&, std::unexpected<std::error_code>>;

    template<typename Callback, typename Result>
    concept nothrow_expected_callback =
        nothrow_invocable<Callback&, std::expected<Result, std::error_code>>
        && (void_like<Result> || nothrow_invocable<Callback&, Result>);

    template<typename Callback, typename Result>
    concept eligible_nothrow_callback = eligible_callback<Callback, Result>
        && ((callback<Callback, Result>
            && nothrow_expected_callback<Callback, Result>
            && nothrow_unexpected_callback<Callback>)
        || (void_like<Result>
            && errorcode_callback<Callback>
            && nothrow_errorcode_callback<Callback>));

    constexpr auto void_success()
        noexcept -> std::expected<void, std::error_code> {
        return {};
    }

    constexpr auto fail(int ev)
        noexcept -> std::unexpected<std::error_code> {
        return std::unexpected(make_system_error_code(ev));
    }

    constexpr auto fail(std::errc ec)
        noexcept -> std::unexpected<std::error_code> {
        return std::unexpected(std::make_error_code(ec));
    }

    constexpr auto fail_invalid_argument()
        noexcept -> std::unexpected<std::error_code> {
        return fail(std::errc::invalid_argument);
    }

    template<typename Callback>
    concept nothrow_constructible_callback =
        std::is_nothrow_constructible_v<std::decay_t<Callback>, Callback&&>;

    template<typename T>
    concept not_tag = !is_specialization_of_v<std::in_place_type_t, std::remove_cvref_t<T>>;

} // namespace iouxx::utility

#endif // IOUXX_UTILITY_H
