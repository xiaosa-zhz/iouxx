#pragma once

#ifndef IOUXX_OPERATION_NETWORK_SOCKET_SEND_RECEIVE_H
#define IOUXX_OPERATION_NETWORK_SOCKET_SEND_RECEIVE_H 1

#include <cstddef>
#include <utility>
#include <functional>
#include <utility>
#include <type_traits>

#include "iouringxx.hpp"
#include "util/utility.hpp"
#include "macro_config.hpp"
#include "socket.hpp"

namespace iouxx::inline iouops::network {

    enum class send_flag {
        none      = 0,
        confirm   = MSG_CONFIRM,
        dontroute = MSG_DONTROUTE,
        dontwait  = MSG_DONTWAIT,
        eor       = MSG_EOR,
        more      = MSG_MORE,
        nosignal  = MSG_NOSIGNAL,
        oob       = MSG_OOB,
        fastopen  = MSG_FASTOPEN,
    };

    constexpr send_flag operator|(send_flag lhs, send_flag rhs) noexcept {
        return static_cast<send_flag>(
            std::to_underlying(lhs) | std::to_underlying(rhs)
        );
    }

    template<utility::eligible_callback<std::size_t> Callback>
    class socket_send_operation : public operation_base
    {
    public:
        template<utility::not_tag F>
        explicit socket_send_operation(iouxx::io_uring_xx& ring, F&& f)
            noexcept(utility::nothrow_constructible_callback<F>) :
            operation_base(iouxx::op_tag<socket_send_operation>, ring),
            callback(std::forward<F>(f))
        {}

        template<typename F, typename... Args>
        explicit socket_send_operation(iouxx::io_uring_xx& ring, std::in_place_type_t<F>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<F, Args...>) :
            operation_base(iouxx::op_tag<socket_send_operation>, ring),
            callback(std::forward<Args>(args)...)
        {}

        using callback_type = Callback;
        using result_type = std::size_t;

        static constexpr std::uint8_t opcode = IORING_OP_SEND;

        socket_send_operation& socket(const socket& s) & noexcept {
            this->fd = s.native_handle();
            return *this;
        }

        socket_send_operation& connection(const connection& c) & noexcept {
            this->fd = c.native_handle();
            return *this;
        }

        socket_send_operation& options(send_flag flags) & noexcept {
            this->flags = flags;
            return *this;
        }

        template<utility::readonly_buffer_like Buffer>
        socket_send_operation& buffer(Buffer&& buf) & noexcept {
            auto span = utility::to_readonly_buffer(std::forward<Buffer>(buf));
            this->buf = span.data();
            this->len = span.size();
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_send(sqe, fd, buf, len,
                std::to_underlying(flags));
        }

        void do_callback(int ev, std::int32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
            utility::eligible_nothrow_callback<callback_type, result_type>) {
            if (ev >= 0) {
                std::invoke(callback, static_cast<std::size_t>(ev));
            } else {
                std::invoke(callback, utility::fail(-ev));
            } 
        }

        const void* buf = nullptr;
        std::size_t len = 0;
        int fd = -1;
        send_flag flags = send_flag::none;
        [[no_unique_address]] callback_type callback;
    };

    template<utility::not_tag F>
    socket_send_operation(iouxx::io_uring_xx&, F) -> socket_send_operation<std::decay_t<F>>;

    template<typename F, typename... Args>
    socket_send_operation(iouxx::io_uring_xx&, std::in_place_type_t<F>, Args&&...)
        -> socket_send_operation<F>;

    enum class recv_flag {
        none         = 0,
        cmsg_cloexec = MSG_CMSG_CLOEXEC,
        dontwait     = MSG_DONTWAIT,
        errqueue     = MSG_ERRQUEUE,
        oob          = MSG_OOB,
        peek         = MSG_PEEK,
        trunc        = MSG_TRUNC,
        waitall      = MSG_WAITALL,
    };

    constexpr recv_flag operator|(recv_flag lhs, recv_flag rhs) noexcept {
        return static_cast<recv_flag>(
            std::to_underlying(lhs) | std::to_underlying(rhs)
        );
    }

    template<utility::eligible_callback<std::size_t> Callback>
    class socket_recv_operation : public operation_base
    {
    public:
        template<utility::not_tag F>
        explicit socket_recv_operation(iouxx::io_uring_xx& ring, F&& f)
            noexcept(utility::nothrow_constructible_callback<F>) :
            operation_base(iouxx::op_tag<socket_recv_operation>, ring),
            callback(std::forward<F>(f))
        {}

        template<typename F, typename... Args>
        explicit socket_recv_operation(iouxx::io_uring_xx& ring, std::in_place_type_t<F>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<F, Args...>) :
            operation_base(iouxx::op_tag<socket_recv_operation>, ring),
            callback(std::forward<Args>(args)...)
        {}

        using callback_type = Callback;
        using result_type = std::size_t;

        static constexpr std::uint8_t opcode = IORING_OP_RECV;

        socket_recv_operation& socket(const socket& s) & noexcept {
            this->fd = s.native_handle();
            return *this;
        }

        socket_recv_operation& connection(const connection& c) & noexcept {
            this->fd = c.native_handle();
            return *this;
        }

        socket_recv_operation& options(recv_flag flags) & noexcept {
            this->flags = flags;
            return *this;
        }

        template<utility::buffer_like Buffer>
        socket_recv_operation& buffer(Buffer&& buf) & noexcept {
            auto span = utility::to_buffer(std::forward<Buffer>(buf));
            this->buf = span.data();
            this->len = span.size();
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_recv(sqe, fd, buf, len,
                std::to_underlying(flags));
        }

        void do_callback(int ev, std::int32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
            utility::eligible_nothrow_callback<callback_type, result_type>) {
            if (ev >= 0) {
                std::invoke(callback, static_cast<std::size_t>(ev));
            } else {
                std::invoke(callback, utility::fail(-ev));
            }
        }

        void* buf = nullptr;
        std::size_t len = 0;
        int fd = -1;
        recv_flag flags = recv_flag::none;
        [[no_unique_address]] callback_type callback;
    };

    template<utility::not_tag F>
    socket_recv_operation(iouxx::io_uring_xx&, F) -> socket_recv_operation<std::decay_t<F>>;

    template<typename F, typename... Args>
    socket_recv_operation(iouxx::io_uring_xx&, std::in_place_type_t<F>, Args&&...)
        -> socket_recv_operation<F>;

} // namespace iouxx::iouops::network

#endif // IOUXX_OPERATION_NETWORK_SOCKET_SEND_RECEIVE_H
