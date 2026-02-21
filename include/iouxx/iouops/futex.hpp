#pragma once
#ifndef IOUXX_OPERATION_MUTEX_H
#define IOUXX_OPERATION_MUTEX_H 1

#ifndef IOUXX_USE_CXX_MODULE

#include <linux/futex.h>

#include <cstdint>
#include <span>

#include "iouxx/iouringxx.hpp"
#include "iouxx/util/utility.hpp"
#include "iouxx/macro_config.hpp" // IWYU pragma: keep
#include "iouxx/cxxmodule_helper.hpp" // IWYU pragma: keep

#endif // IOUXX_USE_CXX_MODULE

// Currently io_uring only supports futex2 with u32 size
#ifndef FUTEX2_SIZE_U32

// No futex operations here!

#else // FUTEX2_SIZE_U32

/*
 * Although futex should be intergrated with userspace facilities
 * to provide proper locking mechanism, existed facilities are
 * various, so this library only provides low-level futex operations.
*/

namespace iouxx::details {

    class futex_operation_base
    {
    public:
        template<typename Self>
        Self& futex_word(this Self& self, const std::uint32_t& word) noexcept {
            self.futex_addr = &word;
            return self;
        }

        template<typename Self>
        Self& futex_mask(this Self& self, std::uint32_t mask) noexcept {
            self.mask = mask;
            return self;
        }

        template<typename Self>
        Self& private_futex(this Self& self, bool is_private = true) noexcept {
            self.is_private = is_private;
            return self;
        }

    protected:
        const std::uint32_t* futex_addr = nullptr;
        std::uint32_t mask = FUTEX_BITSET_MATCH_ANY;
        bool is_private = true;
    };

    constexpr std::uint32_t build_futex2_flags(bool is_private) noexcept {
        return (is_private ? FUTEX2_PRIVATE : 0) | FUTEX2_SIZE_U32;
    }

} // namespace iouxx::details

IOUXX_EXPORT
namespace iouxx::inline iouops {

    template<utility::eligible_maybe_void_callback<void> Callback>
    class futex_wait_operation final : public operation_base, public details::futex_operation_base
    {
    public:
        template<utility::not_tag F>
        explicit futex_wait_operation(iouxx::ring& ring, F&& f)
            noexcept(utility::nothrow_constructible_callback<F>) :
            operation_base(iouxx::op_tag<futex_wait_operation>, ring),
            callback(std::forward<F>(f))
        {}

        template<typename F, typename... Args>
        explicit futex_wait_operation(iouxx::ring& ring, std::in_place_type_t<F>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<F, Args...>) :
            operation_base(iouxx::op_tag<futex_wait_operation>, ring),
            callback(std::forward<Args>(args)...)
        {}

        using callback_type = Callback;
        using result_type = void;

        static constexpr std::uint8_t opcode = IORING_OP_FUTEX_WAIT;

        futex_wait_operation& expected_value(std::uint32_t value) & noexcept {
            this->last_value = value;
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_futex_wait(sqe,
                futex_addr,
                last_value,
                mask,
                details::build_futex2_flags(is_private),
                0);
        }

        void do_callback(int ev, std::int32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
            std::is_nothrow_invocable_v<callback_type>) {
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

        std::uint32_t last_value = 0;
        [[no_unique_address]] callback_type callback;
    };

    template<utility::not_tag F>
    futex_wait_operation(iouxx::ring&, F)
        -> futex_wait_operation<std::decay_t<F>>;

    template<typename F, typename... Args>
    futex_wait_operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...)
        -> futex_wait_operation<F>;

    template<utility::eligible_callback<std::size_t> Callback>
    class futex_wake_operation final : public operation_base, public details::futex_operation_base
    {
    public:
        template<utility::not_tag F>
        explicit futex_wake_operation(iouxx::ring& ring, F&& f)
            noexcept(utility::nothrow_constructible_callback<F>) :
            operation_base(iouxx::op_tag<futex_wake_operation>, ring),
            callback(std::forward<F>(f))
        {}

        template<typename F, typename... Args>
        explicit futex_wake_operation(iouxx::ring& ring, std::in_place_type_t<F>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<F, Args...>) :
            operation_base(iouxx::op_tag<futex_wake_operation>, ring),
            callback(std::forward<Args>(args)...)
        {}

        using callback_type = Callback;
        using result_type = std::size_t;

        static constexpr std::uint8_t opcode = IORING_OP_FUTEX_WAKE;

        futex_wake_operation& notify(std::size_t n) & noexcept {
            this->wakeups = n;
            return *this;
        }

        futex_wake_operation& notify_all() & noexcept {
            this->wakeups = INT_MAX;
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_futex_wake(sqe,
                futex_addr,
                wakeups,
                mask,
                details::build_futex2_flags(is_private),
                0);
        }

        void do_callback(int ev, std::uint32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
            utility::eligible_nothrow_callback<callback_type, result_type>) {
            if (ev >= 0) {
                std::invoke(callback, static_cast<std::size_t>(ev));
            } else {
                std::invoke(callback, utility::fail(-ev));
            }
        }

        std::size_t wakeups = 1;
        [[no_unique_address]] callback_type callback;
    };

    template<utility::not_tag F>
    futex_wake_operation(iouxx::ring&, F)
        -> futex_wake_operation<std::decay_t<F>>;
    
    template<typename F, typename... Args>
    futex_wake_operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...)
        -> futex_wake_operation<F>;

    inline ::futex_waitv make_futex_waitv(
        const std::uint32_t* addr, std::uint32_t expected, bool is_private = true) noexcept {
        ::futex_waitv fwv = {};
        fwv.uaddr = reinterpret_cast<std::uintptr_t>(addr);
        fwv.val = expected;
        fwv.flags = details::build_futex2_flags(is_private);
        return fwv;
    }

    template<utility::eligible_callback<std::size_t> Callback>
    class futex_waitv_operation final : public operation_base
    {
    public:
        template<utility::not_tag F>
        explicit futex_waitv_operation(iouxx::ring& ring, F&& f)
            noexcept(utility::nothrow_constructible_callback<F>) :
            operation_base(iouxx::op_tag<futex_waitv_operation>, ring),
            callback(std::forward<F>(f))
        {}

        template<typename F, typename... Args>
        explicit futex_waitv_operation(iouxx::ring& ring, std::in_place_type_t<F>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<F, Args...>) :
            operation_base(iouxx::op_tag<futex_waitv_operation>, ring),
            callback(std::forward<Args>(args)...)
        {}

        using callback_type = Callback;
        using result_type = std::size_t;

        static constexpr std::uint8_t opcode = IORING_OP_FUTEX_WAITV;

        futex_waitv_operation& waitv(std::span<::futex_waitv> args) & noexcept {
            this->waitv_args = args;
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_futex_waitv(sqe,
                waitv_args.data(),
                static_cast<std::uint32_t>(waitv_args.size()),
                0);
        }

        void do_callback(int ev, std::uint32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
            utility::eligible_nothrow_callback<callback_type, result_type>) {
            if (ev >= 0) {
                std::invoke(callback, static_cast<std::size_t>(ev));
            } else {
                std::invoke(callback, utility::fail(-ev));
            }
        }

        std::span<::futex_waitv> waitv_args;
        [[no_unique_address]] callback_type callback;
    };

    template<utility::not_tag F>
    futex_waitv_operation(iouxx::ring&, F)
        -> futex_waitv_operation<std::decay_t<F>>;

    template<typename F, typename... Args>
    futex_waitv_operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...)
        -> futex_waitv_operation<F>;

} // namespace iouxx::iouops

#endif // FUTEX2_SIZE_U32

#endif // IOUXX_OPERATION_MUTEX_H
