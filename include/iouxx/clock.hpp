// iouxx::boottime_clock
// A trivial C++ clock wrapping Linux CLOCK_BOOTTIME (monotonic and includes suspend time).
// This provides a steady clock which continues to advance during system suspend,
// unlike std::chrono::steady_clock on Linux (which maps to CLOCK_MONOTONIC).
//
// Requirements (C++ "TrivialClock") satisfied:
//   - typedefs: rep, period, duration, time_point
//   - static constexpr bool is_steady
//   - static time_point now() noexcept
//   - all types are trivially copyable / literal / constexpr where required
//
// Epoch definition: the epoch is the same as CLOCK_BOOTTIME epoch (system boot).
// The absolute values are only meaningful for differences; treat time_points
// as opaque except for subtraction/comparison.

#pragma once
#ifndef IOUXX_BOOTTIME_CLOCK_H
#define IOUXX_BOOTTIME_CLOCK_H 1

#ifndef IOUXX_USE_CXX_MODULE

#include <chrono>
#include <ctime>
#include <bits/time.h> // CLOCK_BOOTTIME
#include "macro_config.hpp" // IWYU pragma: keep
#include "cxxmodule_helper.hpp" // IWYU pragma: keep

#endif // IOUXX_USE_CXX_MODULE

// Some libcs may require _GNU_SOURCE for CLOCK_BOOTTIME, but build system can define it globally.
#ifndef CLOCK_BOOTTIME
#error "CLOCK_BOOTTIME not available on this system. boottime_clock requires Linux with CLOCK_BOOTTIME."
#endif

IOUXX_EXPORT
namespace iouxx {

    struct boottime_clock {
        using duration   = std::chrono::nanoseconds;
        using rep        = duration::rep;
        using period     = duration::period;
        using time_point = std::chrono::time_point<boottime_clock, duration>;
        static constexpr bool is_steady = true; // monotonic + never goes backwards

        static time_point now() noexcept {
            ::timespec ts;
            ::clock_gettime(CLOCK_BOOTTIME, &ts);
            // Prevent overflow: tv_nsec < 1e9 always; tv_sec fits in 64 bits for practical uptimes.
            auto ns = static_cast<rep>(ts.tv_sec) * 1'000'000'000 + static_cast<rep>(ts.tv_nsec);
            return time_point(duration{ns});
        }
    };

    constexpr ::timespec to_timespec(boottime_clock::duration d) noexcept {
        using rep = boottime_clock::rep;
        rep total_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(d).count();
        ::timespec ts{};
        ts.tv_sec  = static_cast<time_t>(total_ns / 1'000'000'000);
        ts.tv_nsec = static_cast<long>(total_ns % 1'000'000'000);
        return ts;
    }

    inline ::timespec to_timespec(boottime_clock::time_point tp) noexcept {
        return to_timespec(tp.time_since_epoch());
    }

    inline boottime_clock::duration from_timespec_duration(const ::timespec& ts) noexcept {
        return std::chrono::seconds(ts.tv_sec) + std::chrono::nanoseconds(ts.tv_nsec);
    }

    inline boottime_clock::time_point from_timespec_time_point(const ::timespec& ts) noexcept {
        return boottime_clock::time_point(from_timespec_duration(ts));
    }

    // libcxx has not implemented is_clock_v yet
#if defined(__cpp_lib_chrono) && __cpp_lib_chrono >= 201907L
    static_assert(std::chrono::is_clock_v<boottime_clock>, "boottime_clock must satisfy clock requirements");
#endif

} // namespace iouxx

#endif // IOUXX_BOOTTIME_CLOCK_H
