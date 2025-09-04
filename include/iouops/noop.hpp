#pragma once
#ifndef IOUXX_OPERATION_NOOP_H
#define IOUXX_OPERATION_NOOP_H 1

#include <functional>

#include "iouringxx.hpp"
#include "util/utility.hpp"
#include "macro_config.hpp"

namespace iouxx::inline iouops {

    // Noop operation with user-defined callback.
    template<utility::eligible_maybe_void_callback<void> Callback>
    class noop_operation : public operation_base
    {
    public:
        template<typename F>
        explicit noop_operation(iouxx::io_uring_xx& ring, F&& f)
            noexcept(utility::nothrow_constructible_callback<F>) :
            operation_base(iouxx::op_tag<noop_operation>, ring),
            callback(std::forward<F>(f))
        {}

        using callback_type = Callback;
        using result_type = void;

        static constexpr std::uint8_t opcode = IORING_OP_NOP;

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_nop(sqe);
        }

        void do_callback(int ev, std::int32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
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

        [[no_unique_address]] callback_type callback;
    };

    // Pure noop operation, does nothing on completion.
    // Mainly used for waking up the kernel thread or testing.
    template<>
    class noop_operation<void> : public operation_base
    {
    public:
        explicit noop_operation(iouxx::io_uring_xx& ring)
            : operation_base(iouxx::op_tag<noop_operation>, ring)
        {}

        using callback_type = void;
        using result_type = void;

        static constexpr std::uint8_t opcode = IORING_OP_NOP;

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_nop(sqe);
        }

        void do_callback(int, std::int32_t) noexcept {}
    };

    template<typename F>
    noop_operation(iouxx::io_uring_xx&, F) -> noop_operation<std::decay_t<F>>;

    noop_operation(iouxx::io_uring_xx&) -> noop_operation<void>;

} // namespace iouxx::iouops

#endif // IOUXX_OPERATION_NOOP_H
