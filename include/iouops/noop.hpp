#pragma once
#ifndef IOUXX_OPERATION_NOOP_H
#define IOUXX_OPERATION_NOOP_H 1

#ifndef IOUXX_USE_CXX_MODULE

#include <utility>
#include <functional>
#include <type_traits>

#include "iouringxx.hpp"
#include "macro_config.hpp" // IWYU pragma: keep
#include "cxxmodule_helper.hpp" // IWYU pragma: keep
#include "util/utility.hpp"

#endif // IOUXX_USE_CXX_MODULE

IOUXX_EXPORT
namespace iouxx::inline iouops {

    // Noop operation with user-defined callback.
    template<utility::eligible_maybe_void_callback<void> Callback>
    class noop_operation : public operation_base
    {
    public:
        template<utility::not_tag F>
        explicit noop_operation(iouxx::ring& ring, F&& f)
            noexcept(utility::nothrow_constructible_callback<F>) :
            operation_base(iouxx::op_tag<noop_operation>, ring),
            callback(std::forward<F>(f))
        {}

        template<typename F, typename... Args>
        explicit noop_operation(iouxx::ring& ring, std::in_place_type_t<F>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<F, Args...>) :
            operation_base(iouxx::op_tag<noop_operation>, ring),
            callback(std::forward<Args>(args)...)
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
        explicit noop_operation(iouxx::ring& ring)
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

    template<utility::not_tag F>
    noop_operation(iouxx::ring&, F) -> noop_operation<std::decay_t<F>>;

    template<typename F, typename... Args>
    noop_operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...) -> noop_operation<F>;

    noop_operation(iouxx::ring&) -> noop_operation<void>;

} // namespace iouxx::iouops

#endif // IOUXX_OPERATION_NOOP_H
