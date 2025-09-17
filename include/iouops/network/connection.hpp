#pragma once
#include "cxxmodule_helper.hpp"
#ifndef IOUXX_OPERATION_NETWORK_SOCKET_CONNECTION_H
#define IOUXX_OPERATION_NETWORK_SOCKET_CONNECTION_H 1

#ifndef IOUXX_USE_CXX_MODULE

#include "sys/socket.h"

#include <utility>
#include <algorithm>
#include <functional>
#include <utility>
#include <type_traits>

#include "iouringxx.hpp"
#include "util/utility.hpp"
#include "macro_config.hpp"
#include "ip.hpp"
#include "supported.hpp"
#include "socket.hpp"

#endif // IOUXX_USE_CXX_MODULE

IOUXX_EXPORT
namespace iouxx::inline iouops::network {

    template<utility::eligible_callback<void> Callback>
    class socket_listen_operation : public operation_base
    {
    public:
        template<utility::not_tag F>
        explicit socket_listen_operation(iouxx::ring& ring, F&& f)
            noexcept(utility::nothrow_constructible_callback<F>) :
            operation_base(iouxx::op_tag<socket_listen_operation>, ring),
            callback(std::forward<F>(f))
        {}

        template<typename F, typename... Args>
        explicit socket_listen_operation(iouxx::ring& ring, std::in_place_type_t<F>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<F, Args...>) :
            operation_base(iouxx::op_tag<socket_listen_operation>, ring),
            callback(std::forward<Args>(args)...)
        {}

        using callback_type = Callback;
        using result_type = void;

        static constexpr std::uint8_t opcode = IORING_OP_LISTEN;

        socket_listen_operation& socket(const socket& s) & noexcept {
            this->sock = s;
            return *this;
        }

        static constexpr std::size_t DEFAULT_BACKLOG = 4096;

        socket_listen_operation& backlog(std::size_t backlog) & noexcept {
            this->bl = static_cast<int>(std::min(backlog, DEFAULT_BACKLOG));
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_listen(sqe, sock.native_handle(), bl);
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

        network::socket sock = {};
        int bl = DEFAULT_BACKLOG;
        [[no_unique_address]] callback_type callback;
    };

    template<utility::not_tag F>
    socket_listen_operation(iouxx::ring&, F) -> socket_listen_operation<std::decay_t<F>>;

    template<typename F, typename... Args>
    socket_listen_operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...)
        -> socket_listen_operation<F>;

    template<utility::eligible_callback<void> Callback>
    class socket_connect_operation : public operation_base
    {
    public:
        template<utility::not_tag F>
        explicit socket_connect_operation(iouxx::ring& ring, F&& f)
            noexcept(utility::nothrow_constructible_callback<F>) :
            operation_base(iouxx::op_tag<socket_connect_operation>, ring),
            callback(std::forward<F>(f))
        {}

        template<typename F, typename... Args>
        explicit socket_connect_operation(iouxx::ring& ring, std::in_place_type_t<F>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<F, Args...>) :
            operation_base(iouxx::op_tag<socket_connect_operation>, ring),
            callback(std::forward<Args>(args)...)
        {}

        using callback_type = Callback;
        using result_type = void;

        static constexpr std::uint8_t opcode = IORING_OP_CONNECT;

        socket_connect_operation& socket(const socket& s) & noexcept {
            this->sock = s;
            return *this;
        }

        template<typename SocketInfo>
            requires (std::ranges::contains(supported_domains, ip::get_domain<SocketInfo>()))
        socket_connect_operation& peer_socket_info(const SocketInfo& addr) & noexcept {
            this->sock_info = addr;
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            auto [addr, addrlen] = std::visit(
                [this](auto& si) -> utility::system_addrsock_info {
                    return { .addr = reinterpret_cast<::sockaddr*>(
                        new (&sockaddr_buf) auto(si.to_system_sockaddr())
                    ), .addrlen = sizeof(si.to_system_sockaddr()) };
                }, sock_info);
            ::io_uring_prep_connect(sqe, sock.native_handle(), addr, addrlen);
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

    template<utility::not_tag F>
    socket_connect_operation(iouxx::ring&, F) -> socket_connect_operation<std::decay_t<F>>;

    template<typename F, typename... Args>
    socket_connect_operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...)
        -> socket_connect_operation<F>;

    template<utility::eligible_callback<connection> Callback>
    class socket_accept_operation : public operation_base
    {
    public:
        template<utility::not_tag F>
        explicit socket_accept_operation(iouxx::ring& ring, F&& f)
            noexcept(utility::nothrow_constructible_callback<F>) :
            operation_base(iouxx::op_tag<socket_accept_operation>, ring),
            callback(std::forward<F>(f))
        {}

        template<typename F, typename... Args>
        explicit socket_accept_operation(iouxx::ring& ring, std::in_place_type_t<F>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<F, Args...>) :
            operation_base(iouxx::op_tag<socket_accept_operation>, ring),
            callback(std::forward<Args>(args)...)
        {}

        using callback_type = Callback;
        using result_type = connection;

        static constexpr std::uint8_t opcode = IORING_OP_ACCEPT;

        socket_accept_operation& peer_socket(const socket& s) & noexcept {
            this->sock = s;
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            if (sock.native_handle() < 0) {
                ::io_uring_prep_accept(sqe, sock.native_handle(),
                    nullptr, nullptr, 0);
            } else {
                auto [addr, addrlen] = std::visit(
                    [this](auto& si) -> utility::system_addrsock_info {
                        return { .addr = reinterpret_cast<::sockaddr*>(
                            new (&sockaddr_buf) auto(si.to_system_sockaddr())
                        ), .addrlen = sizeof(si.to_system_sockaddr()) };
                    }, sock_info);
                int flags = SOCK_NONBLOCK | SOCK_CLOEXEC;
                this->addrlen = addrlen;
                ::io_uring_prep_accept(sqe, sock.native_handle(),
                    addr, &this->addrlen, flags);
            }
        }

        void do_callback(int ev, std::int32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
            utility::eligible_nothrow_callback<callback_type, result_type>) {
            if (ev >= 0) {
                std::invoke(callback, connection(sock, ev));
            } else {
                std::invoke(callback, utility::fail(-ev));
            }
        }

        alignas(std::max_align_t) sockaddr_buffer_type sockaddr_buf{};
        ::socklen_t addrlen = 0;
        network::socket sock;
        supported_socket_type sock_info;
        [[no_unique_address]] callback_type callback;
    };

    template<utility::not_tag F>
    socket_accept_operation(iouxx::ring&, F) -> socket_accept_operation<std::decay_t<F>>;

    template<typename F, typename... Args>
    socket_accept_operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...)
        -> socket_accept_operation<F>;

    struct multishot_accept_result {
        connection conn;
        bool more = false;
    };

    template<utility::eligible_callback<multishot_accept_result> Callback>
    class socket_multishot_accept_operation : public operation_base
    {
        static_assert(!utility::is_specialization_of_v<syncwait_callback, Callback>,
            "multishot operation does not support syncronous wait.");
        static_assert(!utility::is_specialization_of_v<awaiter_callback, Callback>,
            "multishot operation does not support coroutine await.");
    public:
        template<utility::not_tag F>
        explicit socket_multishot_accept_operation(iouxx::ring& ring, F&& f)
            noexcept(utility::nothrow_constructible_callback<F>) :
            operation_base(iouxx::op_tag<socket_multishot_accept_operation>, ring),
            callback(std::forward<F>(f))
        {}

        template<typename F, typename... Args>
        explicit socket_multishot_accept_operation(iouxx::ring& ring,
            std::in_place_type_t<F>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<F, Args...>) :
            operation_base(iouxx::op_tag<socket_multishot_accept_operation>, ring),
            callback(std::forward<Args>(args)...)
        {}

        using callback_type = Callback;
        using result_type = multishot_accept_result;

        static constexpr std::uint8_t opcode = IORING_OP_ACCEPT;

        socket_multishot_accept_operation& socket(const socket& s) & noexcept {
            this->sock = s;
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            auto [addr, addrlen] = std::visit(
                [this](auto& si) -> utility::system_addrsock_info {
                    return { .addr = reinterpret_cast<::sockaddr*>(
                        new (&sockaddr_buf) auto(si.to_system_sockaddr())
                    ), .addrlen = sizeof(si.to_system_sockaddr()) };
                }, sock_info);
            int flags = SOCK_NONBLOCK | SOCK_CLOEXEC;
            ::io_uring_prep_multishot_accept(sqe, sock.native_handle(), addr, addrlen, flags);
        }

        void do_callback(int ev, std::int32_t cqe_flags) IOUXX_CALLBACK_NOEXCEPT_IF(
            utility::eligible_nothrow_callback<callback_type, result_type>) {
            if (ev >= 0) {
                std::invoke(callback, multishot_accept_result{
                    .conn = connection(sock, ev),
                    .more = (cqe_flags & IORING_CQE_F_MORE) != 0
                });
            } else {
                std::invoke(callback, utility::fail(-ev));
            }
        }

        alignas(std::max_align_t) sockaddr_buffer_type sockaddr_buf{};
        network::socket sock;
        supported_socket_type sock_info;
        [[no_unique_address]] callback_type callback;
    };

    template<utility::not_tag F>
    socket_multishot_accept_operation(iouxx::ring&, F)
        -> socket_multishot_accept_operation<std::decay_t<F>>;
    
    template<typename F, typename... Args>
    socket_multishot_accept_operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...)
        -> socket_multishot_accept_operation<F>;

    enum class shutdown_option {
        rd = SHUT_RD,
        wr = SHUT_WR,
        rdwr = SHUT_RDWR
    };

    template<utility::eligible_callback<void> Callback>
    class socket_shutdown_operation : public operation_base
    {
    public:
        template<utility::not_tag F>
        explicit socket_shutdown_operation(iouxx::ring& ring, F&& f)
            noexcept(utility::nothrow_constructible_callback<F>) :
            operation_base(iouxx::op_tag<socket_shutdown_operation>, ring),
            callback(std::forward<F>(f))
        {}

        template<typename F, typename... Args>
        explicit socket_shutdown_operation(iouxx::ring& ring, std::in_place_type_t<F>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<F, Args...>) :
            operation_base(iouxx::op_tag<socket_shutdown_operation>, ring),
            callback(std::forward<Args>(args)...)
        {}

        using callback_type = Callback;
        using result_type = void;

        static constexpr std::uint8_t opcode = IORING_OP_SHUTDOWN;

        socket_shutdown_operation& socket(const socket& s) & noexcept {
            this->fd = s.native_handle();
            return *this;
        }

        socket_shutdown_operation& socket(const connection& c) & noexcept {
            this->fd = c.native_handle();
            return *this;
        }

        socket_shutdown_operation& options(shutdown_option how) & noexcept {
            this->how_opt = how;
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_shutdown(sqe, fd, std::to_underlying(how_opt));
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

        shutdown_option how_opt = shutdown_option::rdwr;
        int fd = -1;
        [[no_unique_address]] callback_type callback;
    };

    template<utility::not_tag F>
    socket_shutdown_operation(iouxx::ring&, F) -> socket_shutdown_operation<std::decay_t<F>>;

    template<typename F, typename... Args>
    socket_shutdown_operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...)
        -> socket_shutdown_operation<F>;

} // namespace iouxx::iouops::network

#endif // IOUXX_OPERATION_NETWORK_SOCKET_CONNECTION_H
