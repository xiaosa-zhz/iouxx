#pragma once
#ifndef IOUXX_OPERATION_TIMEOUT_H
#define IOUXX_OPERATION_TIMEOUT_H 1

#ifndef IOUXX_USE_CXX_MODULE

#include <concepts>
#include <chrono>
#include <functional>
#include <utility>
#include <type_traits>

#include "iouxx/iouringxx.hpp"
#include "iouxx/clock.hpp"
#include "iouxx/util/utility.hpp"
#include "iouxx/macro_config.hpp"

#endif // IOUXX_USE_CXX_MODULE

namespace iouxx::details {

    // libcxx has not implemented is_clock_v yet
#if defined(__cpp_lib_chrono) && __cpp_lib_chrono >= 201907L
    template<typename Clock>
    concept clock = std::chrono::is_clock_v<Clock>;
#else // ! __cpp_lib_chrono

    template<typename Dur>
    constexpr bool is_std_duration_v = false;

    template<auto Num, auto Denom, typename Rep>
        requires std::is_arithmetic_v<Rep>
    constexpr bool is_std_duration_v<std::chrono::duration<Rep, std::ratio<Num, Denom>>> = true;

    template<typename TimePoint, typename Clock>
    constexpr bool is_matched_time_point_v = false;

    template<typename TimePointClock, typename Clock>
        requires std::same_as<TimePointClock, Clock>
    constexpr bool is_matched_time_point_v<std::chrono::time_point<TimePointClock, typename Clock::duration>, Clock> = true;

    // Fallback implementation
    template<typename Clock>
    concept clock = requires {
        typename Clock::duration;
        requires is_std_duration_v<typename Clock::duration>;
        typename Clock::rep;
        requires std::same_as<typename Clock::rep, typename Clock::duration::rep>;
        typename Clock::period;
        requires std::same_as<typename Clock::period, typename Clock::duration::period>;
        requires std::same_as<std::chrono::duration<typename Clock::rep, typename Clock::period>, typename Clock::duration>;
        typename Clock::time_point;
        requires is_matched_time_point_v<typename Clock::time_point, Clock>;
        Clock::is_steady;
        requires std::same_as<decltype(Clock::is_steady), const bool>;
        { Clock::now() } -> std::same_as<typename Clock::time_point>;
    };

#endif // __cpp_lib_chrono

    template<clock Clock>
    using clock_duration_t = typename Clock::duration;

    // io_uring only supports CLOCK_MONOTONIC, CLOCK_REALTIME, and CLOCK_BOOTTIME.
    // Corresponding to std::chrono::steady_clock, std::chrono::system_clock,
    // and iouxx::boottime_clock (provided by this library).
    template<clock Clock>
    consteval void check_supported_clock() noexcept {
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
            check_supported_clock<Clock>();
        }
    }

    class timeout_base
    {
    public:
        template<typename Self, clock Clock = std::chrono::steady_clock>
        Self& wait_for(this Self& self, clock_duration_t<Clock> duration,
            Clock clock = Clock{}) noexcept {
            check_supported_clock<Clock>();
            self.ts = utility::to_kernel_timespec(duration);
            set_clock_flag<Clock>(self.flags);
            self.flags &= ~IORING_TIMEOUT_ABS;
            return self;
        }

        template<typename Self, clock Clock, typename Duration>
        Self& wait_until(this Self& self, std::chrono::time_point<Clock, Duration> time_point) noexcept {
            check_supported_clock<Clock>();
            self.ts = utility::to_kernel_timespec(time_point.time_since_epoch());
            set_clock_flag<Clock>(self.flags);
            self.flags |= IORING_TIMEOUT_ABS;
            return self;
        }

    protected:
        ::__kernel_timespec ts{};
        unsigned flags = 0;
    };

} // namespace iouxx::details

IOUXX_EXPORT
namespace iouxx::inline iouops {

    // Timeout operation with user-defined callback.
    template<utility::eligible_callback<void> Callback>
    class timeout_operation : public operation_base, public details::timeout_base
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
            if constexpr (utility::stdexpected_callback<callback_type, void>) {
                if (ev == 0) {
                    std::invoke(callback, utility::void_success());
                } else {
                    std::invoke(callback, utility::fail(-ev));
                }
            } else if constexpr (utility::errorcode_callback<callback_type>) {
                std::invoke(callback, utility::make_system_error_code(-ev));
            } else {
                static_assert(false, "Unreachable");
            }
        }

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
    class multishot_timeout_operation : public operation_base, public details::timeout_base
    {
        static_assert(!utility::is_specialization_of_v<syncwait_callback, Callback>,
            "Multishot operation does not support syncronous wait.");
        static_assert(!utility::is_specialization_of_v<awaiter_callback, Callback>,
            "Multishot operation does not support coroutine await.");
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

        // n = 0 means infinite
        multishot_timeout_operation& repeat(std::size_t n) & noexcept {
            count = n;
            return *this;
        }

        multishot_timeout_operation& repeat_forever() & noexcept {
            return repeat(0);
        }

        // Poison overload to disable wait_until
        template<details::clock Clock, typename Duration>
        multishot_timeout_operation& wait_until(std::chrono::time_point<Clock, Duration>) & noexcept = delete;

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            flags |= IORING_TIMEOUT_MULTISHOT;
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

        std::size_t count = 1;
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
            if constexpr (utility::stdexpected_callback<callback_type, void>) {
                if (ev == 0) {
                    std::invoke(callback, utility::void_success());
                } else {
                    std::invoke(callback, utility::fail(-ev));
                }
            } else if constexpr (utility::errorcode_callback<callback_type>) {
                std::invoke(callback, utility::make_system_error_code(-ev));
            } else {
                static_assert(false, "Unreachable");
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

        explicit timeout_cancel_operation(iouxx::ring& ring, std::in_place_type_t<void>) noexcept :
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
            ::io_uring_prep_timeout_remove(sqe, id.user_data64(), 0);
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

    timeout_cancel_operation(iouxx::ring&, std::in_place_type_t<void>)
        -> timeout_cancel_operation<void>;

} // namespace iouxx::iouops

#endif // IOUXX_OPERATION_TIMEOUT_H
