#pragma once
#ifndef IOUXX_OPERATION_NOOP_H
#define IOUXX_OPERATION_NOOP_H 1

#ifndef IOUXX_USE_CXX_MODULE

#include <utility>
#include <functional>
#include <type_traits>
#include <system_error>

#include "iouxx/iouringxx.hpp"
#include "iouxx/macro_config.hpp" // IWYU pragma: keep
#include "iouxx/cxxmodule_helper.hpp" // IWYU pragma: keep
#include "iouxx/util/utility.hpp"

#endif // IOUXX_USE_CXX_MODULE

IOUXX_EXPORT
namespace iouxx::inline iouops {

    // Noop operation with user-defined callback.
    template<utility::eligible_maybe_void_callback<void> Callback>
    class noop_operation final : public operation_base
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

        // Set a pseudo result code to be passed to callback.
        // This is useful for debugging or testing.
        // Set to 0 will not enable this feature.
        // Note: the 'void' callback version does not support this,
        //  since there is no way to obtain the result code.
        noop_operation& pseudo_result(std::uint32_t res = 0) & noexcept {
            result_code = -res;
            return *this;
        }

        noop_operation& pseudo_result(std::error_code ec) & noexcept {
            pseudo_result(ec.value());
            return *this;
        }

        noop_operation& pseudo_result(std::error_condition ec) & noexcept {
            pseudo_result(ec.value());
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_nop(sqe);
            if (result_code != 0) {
                // io_uring_prep_nop has not support for injecting result,
                // we have to set the flag and result manually.
                sqe->nop_flags |= IORING_NOP_INJECT_RESULT;
                sqe->len = result_code;
            }
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

        int result_code = 0;
        [[no_unique_address]] callback_type callback;
    };

    // Pure noop operation, does nothing on completion.
    // Mainly used for waking up the kernel thread or testing.
    template<>
    class noop_operation<void> final : public operation_base
    {
    public:
        explicit noop_operation(iouxx::ring& ring)
            : operation_base(iouxx::op_tag<noop_operation>, ring)
        {}

        explicit noop_operation(iouxx::ring& ring, std::in_place_type_t<void>)
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

    noop_operation(iouxx::ring&, std::in_place_type_t<void>) -> noop_operation<void>;

} // namespace iouxx::iouops

#endif // IOUXX_OPERATION_NOOP_H
