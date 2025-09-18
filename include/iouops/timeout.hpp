#pragma once
#ifndef IOUXX_OPERATION_TIMEOUT_H
#define IOUXX_OPERATION_TIMEOUT_H 1

#ifndef IOUXX_USE_CXX_MODULE

#include <chrono>
#include <functional>
#include <concepts>
#include <utility>
#include <type_traits>

#include "iouringxx.hpp"
#include "clock.hpp"
#include "util/utility.hpp"
#include "macro_config.hpp"

#endif // IOUXX_USE_CXX_MODULE

namespace iouxx::details {

    // libcxx has not implemented is_clock_v yet
    template<typename Clock>
    concept clock =
#if defined(__cpp_lib_chrono) && __cpp_lib_chrono >= 201907L
        std::chrono::is_clock_v<Clock>;
#else // ! __cpp_lib_chrono
        requires {
            typename Clock::rep;
            typename Clock::period;
            typename Clock::duration;
            typename Clock::time_point;
            { Clock::is_steady } -> std::convertible_to<bool>;
            { Clock::now() } -> std::convertible_to<typename Clock::time_point>;
        };
#endif // __cpp_lib_chrono

    // io_uring only supports CLOCK_MONOTONIC, CLOCK_REALTIME,and CLOCK_BOOTTIME.
    // Corresponding to std::chrono::steady_clock, std::chrono::system_clock,
    // and iouxx::boottime_clock (provided by this library).
    template<clock Clock>
    inline consteval void is_supported_clock() noexcept {
        static_assert(std::same_as<Clock, std::chrono::steady_clock>
                      || std::same_as<Clock, std::chrono::system_clock>
                      || std::same_as<Clock, iouxx::boottime_clock>,
                      "Only steady_clock, system_clock and boottime_clock are supported.");
    }

    template<clock Clock>
    inline void set_clock_flag(unsigned& flags) noexcept {
        if constexpr (std::same_as<Clock, std::chrono::steady_clock>) {
            flags &= ~IORING_TIMEOUT_REALTIME;
            flags &= ~IORING_TIMEOUT_BOOTTIME;
        } else if constexpr (std::same_as<Clock, std::chrono::system_clock>) {
            flags |= IORING_TIMEOUT_REALTIME;
            flags &= ~IORING_TIMEOUT_BOOTTIME;
        } else if constexpr (std::same_as<Clock, iouxx::boottime_clock>) {
            flags |= IORING_TIMEOUT_BOOTTIME;
            flags &= ~IORING_TIMEOUT_REALTIME;
        } else {
            // Should never reach here
            is_supported_clock<Clock>();
        }
    }

} // namespace iouxx::details

IOUXX_EXPORT
namespace iouxx::inline iouops {

    // Timeout operation with user-defined callback.
    template<utility::eligible_callback<void> Callback>
    class timeout_operation : public operation_base
    {
    public:
        template<utility::not_tag F>
        explicit timeout_operation(iouxx::ring& ring, F&& f)
            noexcept(utility::nothrow_constructible_callback<F>) :
            operation_base(iouxx::op_tag<timeout_operation>, ring),
            callback(std::forward<F>(f))
        {}

        template<typename F, typename... Args>
        explicit timeout_operation(iouxx::ring& ring, std::in_place_type_t<F>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<F, Args...>) :
            operation_base(iouxx::op_tag<timeout_operation>, ring),
            callback(std::forward<Args>(args)...)
        {}

        using callback_type = Callback;
        using result_type = void;

        static constexpr std::uint8_t opcode = IORING_OP_TIMEOUT;

        template<details::clock Clock = std::chrono::steady_clock>
        auto wait_for(std::chrono::nanoseconds duration, Clock clock = Clock{}) &
            noexcept -> timeout_operation& {
            details::is_supported_clock<Clock>();
            ts = utility::to_kernel_timespec(duration);
            details::set_clock_flag<Clock>(flags);
            flags &= ~IORING_TIMEOUT_ABS;
            return *this;
        }

        template<details::clock Clock, typename Duration>
        auto wait_until(std::chrono::time_point<Clock, Duration> time_point) &
            noexcept -> timeout_operation& {
            details::is_supported_clock<Clock>();
            ts = utility::to_kernel_timespec(time_point.time_since_epoch());
            details::set_clock_flag<Clock>(flags);
            flags |= IORING_TIMEOUT_ABS;
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_timeout(sqe, &ts, 0, flags);
        }

        void do_callback(int ev, std::uint32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
            utility::eligible_nothrow_callback<callback_type, result_type>) {
            if (ev == -ETIME) {
                ev = 0; // not an error for pure timeout
            }
            if constexpr (utility::callback<callback_type, void>) {
                if (ev == 0) {
                    std::invoke(callback, utility::void_success());
                } else {
                    std::invoke(callback, utility::fail(-ev));
                }
            } else if constexpr (utility::errorcode_callback<callback_type>) {
                std::invoke(callback, utility::make_system_error_code(-ev));
            } else {
                static_assert(utility::always_false<Callback>, "Unreachable");
            }
        }

        ::__kernel_timespec ts{};
        unsigned flags = 0;
        [[no_unique_address]] callback_type callback;
    };

    template<utility::not_tag F>
    timeout_operation(iouxx::ring&, F) -> timeout_operation<std::decay_t<F>>;

    template<typename F, typename... Args>
    timeout_operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...) -> timeout_operation<F>;

    // Timeout that triggers multiple times.
    // On success, callback receive boolean indicating whether there are more shots.
    // Warning:
    // The operation object must outlive the whole execution of the multishot
    template<utility::eligible_callback<bool> Callback>
    class multishot_timeout_operation : public operation_base
    {
        static_assert(!utility::is_specialization_of_v<syncwait_callback, Callback>,
            "multishot operation does not support syncronous wait.");
        static_assert(!utility::is_specialization_of_v<awaiter_callback, Callback>,
            "multishot operation does not support coroutine await.");
    public:
        template<utility::not_tag F>
        explicit multishot_timeout_operation(iouxx::ring& ring, F&& f)
            noexcept(utility::nothrow_constructible_callback<F>) :
            operation_base(iouxx::op_tag<multishot_timeout_operation>, ring),
            callback(std::forward<F>(f))
        {}

        template<typename F, typename... Args>
        explicit multishot_timeout_operation(iouxx::ring& ring, std::in_place_type_t<F>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<F, Args...>) :
            operation_base(iouxx::op_tag<multishot_timeout_operation>, ring),
            callback(std::forward<Args>(args)...)
        {}

        using callback_type = Callback;
        using result_type = bool;
        
        static constexpr std::uint8_t opcode = IORING_OP_TIMEOUT;

        template<details::clock Clock = std::chrono::steady_clock>
        auto wait_for(std::chrono::nanoseconds duration, Clock clock = Clock{})
            & noexcept -> multishot_timeout_operation& {
            details::is_supported_clock<Clock>();
            ts = utility::to_kernel_timespec(duration);
            details::set_clock_flag<Clock>(flags);
            return *this;
        }

        // n = 0 means infinite
        multishot_timeout_operation& repeat(std::size_t n) & noexcept {
            count = n;
            return *this;
        }

        multishot_timeout_operation& repeat_forever() & noexcept {
            count = 0;
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_timeout(sqe, &ts, count, flags);
        }

        void do_callback(int ev, std::uint32_t cqe_flags) IOUXX_CALLBACK_NOEXCEPT_IF(
            utility::eligible_nothrow_callback<callback_type, result_type>) {
            if (ev == -ETIME) {
                ev = 0; // not an error for pure timeout
            }
            const bool more = cqe_flags & IORING_CQE_F_MORE;
            if (ev == 0) {
                std::invoke(callback, more);
            } else {
                std::invoke(callback, utility::fail(-ev));
            }
        }

        ::__kernel_timespec ts{};
        std::size_t count = 1;
        unsigned flags = IORING_TIMEOUT_MULTISHOT;
        [[no_unique_address]] callback_type callback;
    };

    template<utility::not_tag F>
    multishot_timeout_operation(iouxx::ring&, F)
        -> multishot_timeout_operation<std::decay_t<F>>;

    template<typename F, typename... Args>
    multishot_timeout_operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...)
        -> multishot_timeout_operation<F>;

    // Cancel a previously submitted timeout by its identifier.
    template<utility::eligible_maybe_void_callback<void> Callback>
    class timeout_cancel_operation : public operation_base
    {
    public:
        template<utility::not_tag F>
        timeout_cancel_operation(iouxx::ring& ring, F&& f) noexcept :
            operation_base(iouxx::op_tag<timeout_cancel_operation>, ring),
            callback(std::forward<F>(f))
        {}

        template<typename F, typename... Args>
        timeout_cancel_operation(iouxx::ring& ring, std::in_place_type_t<F>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<F, Args...>) :
            operation_base(iouxx::op_tag<timeout_cancel_operation>, ring),
            callback(std::forward<Args>(args)...)
        {}

        using callback_type = Callback;
        using result_type = void;

        static constexpr std::uint8_t opcode = IORING_OP_TIMEOUT_REMOVE;

        timeout_cancel_operation& target(operation_identifier identifier) & noexcept {
            id = identifier;
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_timeout_remove(sqe, id.user_data64(), 0);
        }

        void do_callback(int ev, std::uint32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
            utility::eligible_nothrow_callback<callback_type, result_type>) {
            if constexpr (utility::callback<callback_type, void>) {
                if (ev == 0) {
                    std::invoke(callback, utility::void_success());
                } else {
                    std::invoke(callback, utility::fail(-ev));
                }
            } else if constexpr (utility::errorcode_callback<callback_type>) {
                std::invoke(callback, utility::make_system_error_code(-ev));
            } else {
                static_assert(utility::always_false<Callback>, "Unreachable");
            }
        }

        operation_identifier id = operation_identifier();
        [[no_unique_address]] callback_type callback;
    };

    // Pure timeout cancel operation, does nothing on completion.
    template<>
    class timeout_cancel_operation<void> : public operation_base
    {
    public:
        explicit timeout_cancel_operation(iouxx::ring& ring) noexcept :
            operation_base(iouxx::op_tag<timeout_cancel_operation>, ring)
        {}

        using callback_type = void;
        using result_type = void;

        static constexpr std::uint8_t opcode = IORING_OP_TIMEOUT_REMOVE;

        timeout_cancel_operation& target(operation_identifier identifier) & noexcept {
            id = identifier;
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            const std::uint64_t user_data = static_cast<std::uint64_t>(
                reinterpret_cast<std::uintptr_t>(id.user_data())
            );
            ::io_uring_prep_timeout_remove(sqe, user_data, 0);
        }

        void do_callback(int, std::int32_t) noexcept {}

        operation_identifier id = operation_identifier();
    };

    template<utility::not_tag F>
    timeout_cancel_operation(iouxx::ring&, F) -> timeout_cancel_operation<std::decay_t<F>>;

    template<typename F, typename... Args>
    timeout_cancel_operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...)
        -> timeout_cancel_operation<F>;

    timeout_cancel_operation(iouxx::ring&) -> timeout_cancel_operation<void>;

} // namespace iouxx::iouops

#endif // IOUXX_OPERATION_TIMEOUT_H
