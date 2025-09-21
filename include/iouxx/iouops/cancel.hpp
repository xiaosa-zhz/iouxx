#pragma once
#ifndef IOUXX_OPERATION_CANCEL_H
#define IOUXX_OPERATION_CANCEL_H 1

#ifndef IOUXX_USE_CXX_MODULE

#include <cstdint>
#include <functional>
#include <utility>
#include <type_traits>

#include "iouxx/iouringxx.hpp"
#include "iouxx/util/utility.hpp"
#include "iouxx/macro_config.hpp" // IWYU pragma: keep
#include "iouxx/cxxmodule_helper.hpp" // IWYU pragma: keep

#endif // IOUXX_USE_CXX_MODULE

IOUXX_EXPORT
namespace iouxx::inline iouops {

    // Cancel operation with user-defined callback by provided identifier.
    // On success, callback receive a number indicating how many operations were cancelled.
    template<utility::eligible_maybe_void_callback<std::size_t> Callback>
    class cancel_operation : public operation_base
    {
    public:
        template<utility::not_tag F>
        cancel_operation(iouxx::ring& ring, F&& f) noexcept :
            operation_base(iouxx::op_tag<cancel_operation>, ring),
            callback(std::forward<F>(f))
        {}

        template<typename F, typename... Args>
        cancel_operation(iouxx::ring& ring, std::in_place_type_t<F>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<F, Args...>) :
            operation_base(iouxx::op_tag<cancel_operation>, ring),
            callback(std::forward<Args>(args)...)
        {}

        using callback_type = Callback;
        using result_type = std::size_t;

        static constexpr std::uint8_t opcode = IORING_OP_ASYNC_CANCEL;

        cancel_operation& target(operation_identifier identifier) & noexcept {
            id = identifier;
            return *this;
        }

        cancel_operation& cancel_one() & noexcept {
            flags &= ~IORING_ASYNC_CANCEL_ALL;
            return *this;
        }

        cancel_operation& cancel_all() & noexcept {
            flags |= IORING_ASYNC_CANCEL_ALL;
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_cancel(sqe, id.user_data(), flags);
        }

        void do_callback(int ev, std::uint32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
            utility::eligible_nothrow_callback<callback_type, result_type>) {
            std::size_t cancelled_count = 0;
            if (ev >= 0) {
                if (flags & IORING_ASYNC_CANCEL_ALL) {
                    cancelled_count = static_cast<std::size_t>(ev);
                } else {
                    cancelled_count = 1;
                }
                ev = 0;
            }
            if (ev == 0) {
                std::invoke(callback, cancelled_count);
            } else {
                std::invoke(callback, utility::fail(-ev));
            }
        }

        operation_identifier id = operation_identifier();
        unsigned flags = IORING_ASYNC_CANCEL_USERDATA;
        [[no_unique_address]] callback_type callback;
    };

    // Pure cancel operation, does nothing on completion.
    template<>
    class cancel_operation<void> : public operation_base
    {
    public:
        explicit cancel_operation(iouxx::ring& ring) noexcept :
            operation_base(iouxx::op_tag<cancel_operation>, ring)
        {}

        using callback_type = void;
        using result_type = void;

        static constexpr std::uint8_t opcode = IORING_OP_ASYNC_CANCEL;

        cancel_operation& target(operation_identifier identifier) & noexcept {
            id = identifier;
            return *this;
        }

        cancel_operation& cancel_one() & noexcept {
            flags &= ~IORING_ASYNC_CANCEL_ALL;
            return *this;
        }

        cancel_operation& cancel_all() & noexcept {
            flags |= IORING_ASYNC_CANCEL_ALL;
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_cancel(sqe, id.user_data(), flags);
        }

        void do_callback(int, std::int32_t) noexcept {}

        operation_identifier id = operation_identifier();
        unsigned flags = 0;
    };

    template<utility::not_tag F>
    cancel_operation(iouxx::ring&, F) -> cancel_operation<std::decay_t<F>>;

    template<typename F, typename... Args>
    cancel_operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...) -> cancel_operation<F>;

    cancel_operation(iouxx::ring&) -> cancel_operation<void>;

    // Cancel operation with user-defined callback by provided file descriptor.
    // On success, callback receive a number indicating how many operations were cancelled.
    template<utility::eligible_maybe_void_callback<std::size_t> Callback>
    class cancel_fd_operation : public operation_base
    {
    public:
        template<utility::not_tag F>
        cancel_fd_operation(iouxx::ring& ring, F&& f) noexcept :
            operation_base(iouxx::op_tag<cancel_fd_operation>, ring),
            callback(std::forward<F>(f))
        {}

        template<typename F, typename... Args>
        cancel_fd_operation(iouxx::ring& ring, std::in_place_type_t<F>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<F, Args...>) :
            operation_base(iouxx::op_tag<cancel_fd_operation>, ring),
            callback(std::forward<Args>(args)...)
        {}

        using callback_type = Callback;
        using result_type = std::size_t;

        static constexpr std::uint8_t opcode = IORING_OP_ASYNC_CANCEL;

        cancel_fd_operation& target(int file_descriptor) & noexcept {
            fd = file_descriptor;
            flags &= ~IORING_ASYNC_CANCEL_FD_FIXED;
            return *this;
        }

        // Provide file descriptor that is an 'direct descripor' of io_uring
        cancel_fd_operation& target_direct(int file_descriptor) & noexcept {
            fd = file_descriptor;
            flags |= IORING_ASYNC_CANCEL_FD_FIXED;
            return *this;
        }

        cancel_fd_operation& cancel_one() & noexcept {
            flags &= ~IORING_ASYNC_CANCEL_ALL;
            return *this;
        }

        cancel_fd_operation& cancel_all() & noexcept {
            flags |= IORING_ASYNC_CANCEL_ALL;
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_cancel_fd(sqe, fd, flags);
        }

        void do_callback(int ev, std::uint32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
            utility::eligible_nothrow_callback<callback_type, result_type>) {
            std::size_t cancelled_count = 0;
            if (ev >= 0) {
                if (flags & IORING_ASYNC_CANCEL_ALL) {
                    cancelled_count = static_cast<std::size_t>(ev);
                } else {
                    cancelled_count = 1;
                }
                ev = 0;
            }
            if (ev == 0) {
                std::invoke(callback, cancelled_count);
            } else {
                std::invoke(callback, utility::fail(-ev));
            }
        }

        int fd = -1;
        unsigned flags = IORING_ASYNC_CANCEL_FD;
        [[no_unique_address]] callback_type callback;
    };

    // Pure cancel fd operation, does nothing on completion.
    template<>
    class cancel_fd_operation<void> : public operation_base
    {
    public:
        explicit cancel_fd_operation(iouxx::ring& ring) noexcept :
            operation_base(iouxx::op_tag<cancel_fd_operation>, ring)
        {}

        using callback_type = void;
        using result_type = void;

        static constexpr std::uint8_t opcode = IORING_OP_ASYNC_CANCEL;

        cancel_fd_operation& target(int file_descriptor) & noexcept {
            fd = file_descriptor;
            flags &= ~IORING_ASYNC_CANCEL_FD_FIXED;
            return *this;
        }

        // Provide file descriptor that is an 'direct descripor' of io_uring
        cancel_fd_operation& target_direct(int file_descriptor) & noexcept {
            fd = file_descriptor;
            flags |= IORING_ASYNC_CANCEL_FD_FIXED;
            return *this;
        }

        cancel_fd_operation& cancel_one() & noexcept {
            flags &= ~IORING_ASYNC_CANCEL_ALL;
            return *this;
        }

        cancel_fd_operation& cancel_all() & noexcept {
            flags |= IORING_ASYNC_CANCEL_ALL;
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_cancel_fd(sqe, fd, flags);
        }

        void do_callback(int, std::int32_t) noexcept {}

        int fd = -1;
        unsigned flags = IORING_ASYNC_CANCEL_FD;
    };

    template<utility::not_tag F>
    cancel_fd_operation(iouxx::ring&, F) -> cancel_fd_operation<std::decay_t<F>>;

    template<typename F, typename... Args>
    cancel_fd_operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...) -> cancel_fd_operation<F>;

    cancel_fd_operation(iouxx::ring&) -> cancel_fd_operation<void>;

} // namespace iouxx::iouops

#endif // IOUXX_OPERATION_CANCEL_H
