#pragma once
#ifndef IOUXX_OPERATION_NOOP_H
#define IOUXX_OPERATION_NOOP_H 1

#include <functional>
#include <expected>

#include "iouringxx.hpp"
#include "util/utility.hpp"
#include "macro_config.hpp"

namespace iouxx::inline iouops {

    // Noop operation with user-defined callback.
    template<typename Callback>
        requires (std::is_void_v<Callback>)
        || std::invocable<Callback, std::expected<void, std::error_code>>
        || std::invocable<Callback, std::error_code>
    class noop_operation : public operation_base
    {
    public:
        template<typename F>
        explicit noop_operation(iouxx::io_uring_xx& ring, F&& f) :
            operation_base(iouxx::op_tag<noop_operation>, ring),
            callback(std::forward<F>(f))
        {}

        using callback_type = Callback;
        using result_type = void;

        static constexpr std::uint8_t IORING_OPCODE = IORING_OP_NOP;

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_nop(sqe);
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

        static constexpr std::uint8_t IORING_OPCODE = IORING_OP_NOP;

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
