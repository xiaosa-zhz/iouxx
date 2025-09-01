#pragma once
#ifndef IOUXX_OPERATION_TIMEOUT_H
#define IOUXX_OPERATION_TIMEOUT_H 1

#include <chrono>
#include <functional>
#include <expected>
#include <concepts>
#include <system_error>

#include "iouringxx.hpp"
#include "boottime_clock.hpp"
#include "util/utility.hpp"
#include "macro_config.hpp"

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

namespace iouxx::inline iouops {

    // Timeout operation with user-defined callback.
    template<typename Callback>
        requires std::invocable<Callback, std::error_code>
        || std::invocable<Callback, std::expected<void, std::error_code>>
    class timeout_operation : public operation_base
    {
    public:
        template<typename F>
        explicit timeout_operation(iouxx::io_uring_xx& ring, F&& f) :
            operation_base(iouxx::op_tag<timeout_operation>, ring),
            callback(std::forward<F>(f))
        {}

        using callback_type = Callback;
        using result_type = void;

        static constexpr std::uint8_t IORING_OPCODE = IORING_OP_TIMEOUT;

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

        void do_callback(int ev, std::int32_t) IOUXX_CALLBACK_NOEXCEPT {
            if (ev == -ETIME) {
                ev = 0; // not an error for pure timeout
            }
            if constexpr (std::invocable<Callback, std::expected<void, std::error_code>>) {
                if (ev == 0) {
                    std::invoke(callback, std::expected<void, std::error_code>{});
                } else {
                    std::invoke(callback, std::unexpected(
                        utility::make_system_error_code(-ev)
                    ));
                }
            } else if constexpr (std::invocable<Callback, std::error_code>) {
                std::invoke(callback, utility::make_system_error_code(-ev));
            } else {
                static_assert(utility::always_false<Callback>, "Unreachable");
            }
        }

        ::__kernel_timespec ts{};
        unsigned flags = 0;
        [[no_unique_address]] Callback callback;
    };

    template<typename F>
    timeout_operation(iouxx::io_uring_xx&, F) -> timeout_operation<std::decay_t<F>>;

    // Timeout that triggers multiple times.
    // On success, callback receive boolean indicating whether there are more shots.
    // Warning:
    // The operation object must outlive the whole execution of the multishot
    template<std::invocable<std::expected<bool, std::error_code>> Callback>
    class multishot_timeout_operation : public operation_base
    {
    public:
        template<typename F>
        explicit multishot_timeout_operation(iouxx::io_uring_xx& ring, F&& f) :
            operation_base(iouxx::op_tag<multishot_timeout_operation>, ring),
            callback(std::forward<F>(f))
        {}

        using callback_type = Callback;
        using result_type = bool;
        
        static constexpr std::uint8_t IORING_OPCODE = IORING_OP_TIMEOUT;

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

        void do_callback(int ev, std::int32_t cqe_flags) IOUXX_CALLBACK_NOEXCEPT {
            if (ev == -ETIME) {
                ev = 0; // not an error for pure timeout
            }
            const bool more = cqe_flags & IORING_CQE_F_MORE;
            if (ev == 0) {
                std::invoke(callback, more);
            } else {
                std::invoke(callback, std::unexpected(
                    utility::make_system_error_code(-ev)
                ));
            }
        }

        ::__kernel_timespec ts{};
        std::size_t count = 1;
        unsigned flags = IORING_TIMEOUT_MULTISHOT;
        [[no_unique_address]] Callback callback;
    };

    template<typename F>
    multishot_timeout_operation(iouxx::io_uring_xx&, F)
        -> multishot_timeout_operation<std::decay_t<F>>;

    // Cancel a previously submitted timeout by its identifier.
    template<typename Callback>
        requires (std::is_void_v<Callback>)
        || std::invocable<Callback, std::error_code>
        || std::invocable<Callback, std::expected<void, std::error_code>>
    class timeout_cancel_operation : public operation_base
    {
    public:
        template<typename F>
        timeout_cancel_operation(iouxx::io_uring_xx& ring, F&& f) noexcept :
            operation_base(iouxx::op_tag<timeout_cancel_operation>, ring),
            callback(std::forward<F>(f))
        {}

        using callback_type = Callback;
        using result_type = void;

        static constexpr std::uint8_t IORING_OPCODE = IORING_OP_TIMEOUT_REMOVE;

        timeout_cancel_operation& target(operation_identifier identifier) & noexcept {
            id = identifier;
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_timeout_remove(sqe, id.user_data64(), 0);
        }

        void do_callback(int ev, std::int32_t) IOUXX_CALLBACK_NOEXCEPT {
            if constexpr (std::invocable<Callback, std::expected<void, std::error_code>>) {
                if (ev == 0) {
                    std::invoke(callback, std::expected<void, std::error_code>{});
                } else {
                    std::invoke(callback, std::unexpected(
                        utility::make_system_error_code(-ev)
                    ));
                }
            } else if constexpr (std::invocable<Callback, std::error_code>) {
                std::invoke(callback, utility::make_system_error_code(-ev));
            } else {
                static_assert(utility::always_false<Callback>, "Unreachable");
            }
        }

        operation_identifier id = operation_identifier();
        [[no_unique_address]] Callback callback;
    };

    // Pure timeout cancel operation, does nothing on completion.
    template<>
    class timeout_cancel_operation<void> : public operation_base
    {
    public:
        explicit timeout_cancel_operation(iouxx::io_uring_xx& ring) noexcept :
            operation_base(iouxx::op_tag<timeout_cancel_operation>, ring)
        {}

        static constexpr std::uint8_t IORING_OPCODE = IORING_OP_TIMEOUT_REMOVE;

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

    template<typename F>
    timeout_cancel_operation(iouxx::io_uring_xx&, F) -> timeout_cancel_operation<std::decay_t<F>>;

    timeout_cancel_operation(iouxx::io_uring_xx&) -> timeout_cancel_operation<void>;

} // namespace iouxx::iouops

#endif // IOUXX_OPERATION_TIMEOUT_H
