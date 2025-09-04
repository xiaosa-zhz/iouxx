#pragma once
#ifndef IOUXX_OPERATION_NETWORK_SOCKET_PREPARE_H
#define IOUXX_OPERATION_NETWORK_SOCKET_PREPARE_H 1

#include <liburing.h>

#include <cstddef>
#include <utility>
#include <algorithm>
#include <variant>

#include "iouringxx.hpp"
#include "socket.hpp"
#include "ip.hpp"
#include "supported.hpp"
#include "iouops/file/openclose.hpp"

namespace iouxx::inline iouops::network {

    template<utility::eligible_callback<socket> Callback>
    class socket_open_operation : public operation_base
    {
    public:
        template<typename F>
        explicit socket_open_operation(iouxx::io_uring_xx& ring, F&& f) :
            operation_base(iouxx::op_tag<socket_open_operation>, ring),
            callback(std::forward<F>(f))
        {}

        using callback_type = Callback;
        using result_type = socket;

        static constexpr std::uint8_t opcode = IORING_OP_SOCKET;

        socket_open_operation& domain(socket::domain domain) & noexcept {
            this->socket_domain = domain;
            return *this;
        }

        socket_open_operation& type(socket::type type) & noexcept {
            this->socket_type = type
                | socket::type::cloexec
                | socket::type::nonblock;
            return *this;
        }

        socket_open_operation& protocol(socket::protocol protocol) & noexcept {
            this->socket_protocol = protocol;
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_socket(
                sqe,
                std::to_underlying(socket_domain),
                std::to_underlying(socket_type),
                std::to_underlying(socket_protocol),
                0
            );
        }

        void do_callback(int ev, std::int32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
            utility::eligible_nothrow_callback<callback_type, result_type>) {
            if (ev >= 0) {
                std::invoke(callback, socket(
                    ev, socket_domain, socket_type, socket_protocol
                ));
            } else {
                std::invoke(callback, utility::fail(-ev));
            }
        }

        socket::domain socket_domain = socket::domain::unspec;
        socket::type socket_type = socket::type::stream;
        socket::protocol socket_protocol = socket::protocol::unknown;
        [[no_unique_address]] callback_type callback;
    };

    template<typename F>
    socket_open_operation(iouxx::io_uring_xx&, F) -> socket_open_operation<std::decay_t<F>>;

    template<typename Callback>
    class socket_close_operation : public file::file_close_operation<Callback>
    {
    private:
        using base = file::file_close_operation<Callback>;
    public:
        template<typename F>
        explicit socket_close_operation(iouxx::io_uring_xx& ring, F&& f)
            : base(ring, std::forward<F>(f))
        {}

        socket_close_operation& socket(const socket& s) & noexcept {
            this->base::file(s.native_handle());
            return *this;
        }

        // Shadow the file::file_close_operation's file() method to avoid misuse
        void file(int fd) & noexcept = delete;
    };

    template<typename F>
    socket_close_operation(iouxx::io_uring_xx&, F) -> socket_close_operation<std::decay_t<F>>;

    template<utility::eligible_callback<void> Callback>
    class socket_bind_operation : public operation_base
    {
    public:
        template<typename F>
        explicit socket_bind_operation(iouxx::io_uring_xx& ring, F&& f) :
            operation_base(iouxx::op_tag<socket_bind_operation>, ring),
            callback(std::forward<F>(f))
        {}

        using callback_type = Callback;
        using result_type = void;

        static constexpr std::uint8_t opcode = IORING_OP_BIND;

        socket_bind_operation& socket(const socket& s) & noexcept {
            this->sock = s;
            return *this;
        }

        template<typename SocketInfo>
            requires (std::ranges::contains(supported_domains, ip::get_domain<SocketInfo>()))
        socket_bind_operation& socket_info(const SocketInfo& addr) & noexcept {
            this->sock_info = addr;
            return *this;
        }

        // User may check the socket is valid before calling this method.
        // If not, wrong argument will cause error when completed.
        bool check() const noexcept {
            socket::domain domain = std::visit(
                [](auto& si) { return si.domain; },
                sock_info);
            return domain == sock.socket_domain();
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            assert(check());
            auto [addr, addrlen] = std::visit(
                [this](auto& si) -> utility::system_addrsock_info {
                    return { .addr = reinterpret_cast<::sockaddr*>(
                        new (&sockaddr_buf) auto(si.to_system_sockaddr())
                    ), .addrlen = sizeof(si.to_system_sockaddr()) };
                }, sock_info);
            ::io_uring_prep_bind(sqe, sock.native_handle(), addr, addrlen);
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

        alignas(std::max_align_t) sockaddr_buffer_type sockaddr_buf{};
        network::socket sock;
        supported_socket_type sock_info;
        [[no_unique_address]] callback_type callback;
    };

    template<typename F>
    socket_bind_operation(iouxx::io_uring_xx&, F) -> socket_bind_operation<std::decay_t<F>>;

} // namespace iouxx::iouops::network

#endif // IOUXX_OPERATION_NETWORK_SOCKET_PREPARE_H
