#pragma once
#ifndef IOUXX_OPERATION_NETWORK_SOCKET_CONNECTION_H
#define IOUXX_OPERATION_NETWORK_SOCKET_CONNECTION_H 1

#ifndef IOUXX_USE_CXX_MODULE

#include <sys/socket.h>

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

namespace iouxx::details {

    struct accept_addrinfo {
        ::sockaddr* addr = nullptr;
        ::socklen_t* addrlen = nullptr;
    };

    class peer_socket_info_base
    {
    protected:
        void set_socket_info_type(iouops::network::socket_config::domain domain) noexcept {
            std::size_t idx = domain_to_index(domain);
            iouops::network::domain_setters[idx](sock_info);
        }

        utility::system_addrsock_info to_system_sockaddr() noexcept {
            return sock_info.visit(
                [this](auto& si) -> utility::system_addrsock_info {
                    return { .addr = reinterpret_cast<::sockaddr*>(
                        new (sockaddr_buf.data()) auto(si.to_system_sockaddr())
                    ), .addrlen = sizeof(si.to_system_sockaddr()) };
                }
            );
        }

        void from_system_sockaddr() {
            if (addrlen > sizeof(sockaddr_buf)) {
                // Socket not supported yet
                sock_info.emplace<iouops::network::unspecified_socket_info>();
                return;
            }
            // TODO: start_lifetime_as
            ::sockaddr* addr = std::launder(reinterpret_cast<::sockaddr*>(sockaddr_buf.data()));
            ::socklen_t* addrlen = &this->addrlen;
            sock_info.visit([addr, addrlen](auto&& sock) {
                sock.from_system_sockaddr(addr, addrlen);
            });
        }

        accept_addrinfo get_accept_addr_params() noexcept {
            addrlen = sizeof(sockaddr_buf);
            return {
                .addr = reinterpret_cast<::sockaddr*>(sockaddr_buf.data()),
                .addrlen = &addrlen
            };
        }

        alignas(std::max_align_t) iouops::network::sockaddr_buffer_type sockaddr_buf{};
        ::socklen_t addrlen = 0;
        iouops::network::supported_socket_type sock_info;
    };

} // namespace iouxx::details

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

        socket_listen_operation& socket(const fixed_socket& s) & noexcept {
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
            sock.visit(utility::overloaded{
                [&, this](const network::socket& s) {
                    ::io_uring_prep_listen(sqe, s.native_handle(), bl);
                },
                [&, this](const network::fixed_socket& s) {
                    ::io_uring_prep_listen(sqe, s.index(), bl);
                    sqe->flags |= IOSQE_FIXED_FILE;
                }
            });
        }

        void do_callback(int ev, std::uint32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
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

        socket_variant sock = {};
        int bl = DEFAULT_BACKLOG;
        [[no_unique_address]] callback_type callback;
    };

    template<utility::not_tag F>
    socket_listen_operation(iouxx::ring&, F) -> socket_listen_operation<std::decay_t<F>>;

    template<typename F, typename... Args>
    socket_listen_operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...)
        -> socket_listen_operation<F>;

    template<utility::eligible_callback<void> Callback>
    class socket_connect_operation
        : public operation_base, protected details::peer_socket_info_base
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

        socket_connect_operation& socket(const fixed_socket& s) & noexcept {
            this->sock = s;
            return *this;
        }

        template<typename Self, typename SocketInfo>
            requires (std::ranges::contains(supported_domains, ip::get_domain<SocketInfo>()))
        Self& peer_socket_info(this Self& self, const SocketInfo& addr) noexcept {
            self.sock_info = addr;
            return self;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            auto [addr, addrlen] = this->to_system_sockaddr();
            sock.visit(utility::overloaded{
                [&, this](const network::socket& s) {
                    ::io_uring_prep_connect(sqe, s.native_handle(), addr, addrlen);
                },
                [&, this](const network::fixed_socket& s) {
                    ::io_uring_prep_connect(sqe, s.index(), addr, addrlen);
                    sqe->flags |= IOSQE_FIXED_FILE;
                }
            });
        }

        void do_callback(int ev, std::uint32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
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

        socket_variant sock;
        [[no_unique_address]] callback_type callback;
    };

    template<utility::not_tag F>
    socket_connect_operation(iouxx::ring&, F) -> socket_connect_operation<std::decay_t<F>>;

    template<typename F, typename... Args>
    socket_connect_operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...)
        -> socket_connect_operation<F>;

    struct accept_result {
        connection conn;
        supported_socket_type peer;
    };

    template<typename Callback>
        requires utility::eligible_callback<Callback, accept_result>
        || utility::eligible_callback<Callback, connection>
    class socket_accept_operation
        : public operation_base, protected details::peer_socket_info_base
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
        using result_type = std::conditional_t<
            utility::eligible_callback<Callback, accept_result>,
            accept_result,
            connection
        >;

        static constexpr std::uint8_t opcode = IORING_OP_ACCEPT;

        socket_accept_operation& socket(const socket& s) & noexcept {
            this->sock = s;
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            if constexpr (std::same_as<result_type, accept_result>) {
                this->set_socket_info_type(sock.socket_domain());
            }
            int flags = SOCK_NONBLOCK | SOCK_CLOEXEC;
            if (std::holds_alternative<unspecified_socket_info>(sock_info)) {
                ::io_uring_prep_accept(sqe, sock.native_handle(),
                    nullptr, nullptr, flags);
            } else {
                auto [addr, addrlen] = this->get_accept_addr_params();
                ::io_uring_prep_accept(sqe, sock.native_handle(),
                    addr, addrlen, flags);
            }
        }

        void do_callback(int ev, std::uint32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
            utility::eligible_nothrow_callback<callback_type, result_type>) {
            if (ev >= 0) {
                if constexpr (std::same_as<result_type, accept_result>) {
                    this->from_system_sockaddr();
                    std::invoke(callback, accept_result{
                        .conn = connection(sock, ev),
                        .peer = std::move(sock_info)
                    });
                } else {
                    std::invoke(callback, connection(sock, ev));
                }
            } else {
                std::invoke(callback, utility::fail(-ev));
            }
        }

        network::socket sock;
        [[no_unique_address]] callback_type callback;
    };

    template<utility::not_tag F>
    socket_accept_operation(iouxx::ring&, F) -> socket_accept_operation<std::decay_t<F>>;

    template<typename F, typename... Args>
    socket_accept_operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...)
        -> socket_accept_operation<F>;

    struct fixed_accept_result {
        fixed_connection conn;
        supported_socket_type peer;
    };

    template<typename Callback>
        requires utility::eligible_callback<Callback, fixed_accept_result>
        || utility::eligible_callback<Callback, fixed_connection>
    class fixed_socket_accept_operation
        : public operation_base, protected details::peer_socket_info_base
    {
    public:
        template<utility::not_tag F>
        explicit fixed_socket_accept_operation(iouxx::ring& ring, F&& f)
            noexcept(utility::nothrow_constructible_callback<F>) :
            operation_base(iouxx::op_tag<fixed_socket_accept_operation>, ring),
            callback(std::forward<F>(f))
        {}

        template<typename F, typename... Args>
        explicit fixed_socket_accept_operation(iouxx::ring& ring,
            std::in_place_type_t<F>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<F, Args...>) :
            operation_base(iouxx::op_tag<fixed_socket_accept_operation>, ring),
            callback(std::forward<Args>(args)...)
        {}

        using callback_type = Callback;
        using result_type = std::conditional_t<
            utility::eligible_callback<Callback, fixed_accept_result>,
            fixed_accept_result,
            fixed_connection
        >;

        static constexpr std::uint8_t opcode = IORING_OP_ACCEPT;

        fixed_socket_accept_operation& socket(const fixed_socket& s) & noexcept {
            this->sock = s;
            return *this;
        }

        fixed_socket_accept_operation& index(int index = IORING_FILE_INDEX_ALLOC) & noexcept {
            this->file_index = index;
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            int flags = SOCK_NONBLOCK | SOCK_CLOEXEC;
            if constexpr (std::same_as<result_type, fixed_accept_result>) {
                this->set_socket_info_type(sock.socket_domain());
            }
            if (std::holds_alternative<unspecified_socket_info>(sock_info)) {
                ::io_uring_prep_accept_direct(sqe, sock.index(),
                    nullptr, nullptr, flags, file_index);
            } else {
                auto [addr, addrlen] = this->get_accept_addr_params();
                ::io_uring_prep_accept_direct(sqe, sock.index(),
                    addr, addrlen, flags, file_index);
            }
            sqe->flags |= IOSQE_FIXED_FILE;
        }

        void do_callback(int ev, std::uint32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
            utility::eligible_nothrow_callback<callback_type, result_type>) {
            if (ev >= 0) {
                if constexpr (std::same_as<result_type, fixed_accept_result>) {
                    this->from_system_sockaddr();
                    std::invoke(callback, fixed_accept_result{
                        .conn = fixed_connection(sock, ev),
                        .peer = std::move(sock_info)
                    });
                } else {
                    std::invoke(callback, fixed_connection(sock, ev));
                }
            } else {
                std::invoke(callback, utility::fail(-ev));
            }
        }

        int file_index = IORING_FILE_INDEX_ALLOC;
        network::fixed_socket sock;
        [[no_unique_address]] callback_type callback;
    };

    template<utility::not_tag F>
    fixed_socket_accept_operation(iouxx::ring&, F)
        -> fixed_socket_accept_operation<std::decay_t<F>>;

    template<typename F, typename... Args>
    fixed_socket_accept_operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...)
        -> fixed_socket_accept_operation<F>;

    struct multishot_accept_result {
        connection conn;
        supported_socket_type peer;
        bool more = false;
    };

    template<utility::eligible_callback<multishot_accept_result> Callback>
    class socket_multishot_accept_operation
        : public operation_base, protected details::peer_socket_info_base
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
            int flags = SOCK_NONBLOCK | SOCK_CLOEXEC;
            this->set_socket_info_type(sock.socket_domain());
            if (std::holds_alternative<unspecified_socket_info>(sock_info)) {
                ::io_uring_prep_multishot_accept(sqe, sock.native_handle(),
                    nullptr, nullptr, flags);
            } else {
                auto [addr, addrlen] = this->get_accept_addr_params();
                ::io_uring_prep_multishot_accept(sqe, sock.native_handle(),
                    addr, addrlen, flags);
            }
        }

        void do_callback(int ev, std::uint32_t cqe_flags) IOUXX_CALLBACK_NOEXCEPT_IF(
            utility::eligible_nothrow_callback<callback_type, result_type>) {
            if (ev >= 0) {
                this->from_system_sockaddr();
                std::invoke(callback, multishot_accept_result{
                    .conn = connection(sock, ev),
                    .peer = std::move(sock_info),
                    .more = (cqe_flags & IORING_CQE_F_MORE) != 0
                });
            } else {
                std::invoke(callback, utility::fail(-ev));
            }
        }

        network::socket sock;
        [[no_unique_address]] callback_type callback;
    };

    template<utility::not_tag F>
    socket_multishot_accept_operation(iouxx::ring&, F)
        -> socket_multishot_accept_operation<std::decay_t<F>>;
    
    template<typename F, typename... Args>
    socket_multishot_accept_operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...)
        -> socket_multishot_accept_operation<F>;

    struct multishot_fixed_accept_result {
        fixed_connection conn;
        supported_socket_type peer;
        bool more = false;
    };

    // Note: multishot_accept_direct always allocates all new slots,
    //  so user cannot specify index here.
    template<utility::eligible_callback<multishot_fixed_accept_result> Callback>
    class fixed_socket_multishot_accept_operation
        : public operation_base, protected details::peer_socket_info_base
    {
        static_assert(!utility::is_specialization_of_v<syncwait_callback, Callback>,
            "multishot operation does not support syncronous wait.");
        static_assert(!utility::is_specialization_of_v<awaiter_callback, Callback>,
            "multishot operation does not support coroutine await.");
    public:
        template<utility::not_tag F>
        explicit fixed_socket_multishot_accept_operation(iouxx::ring& ring, F&& f)
            noexcept(utility::nothrow_constructible_callback<F>) :
            operation_base(iouxx::op_tag<fixed_socket_multishot_accept_operation>, ring),
            callback(std::forward<F>(f))
        {}

        template<typename F, typename... Args>
        explicit fixed_socket_multishot_accept_operation(iouxx::ring& ring,
            std::in_place_type_t<F>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<F, Args...>) :
            operation_base(iouxx::op_tag<fixed_socket_multishot_accept_operation>, ring),
            callback(std::forward<Args>(args)...)
        {}

        using callback_type = Callback;
        using result_type = multishot_fixed_accept_result;

        static constexpr std::uint8_t opcode = IORING_OP_ACCEPT;

        fixed_socket_multishot_accept_operation& socket(const fixed_socket& s) & noexcept {
            this->sock = s;
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            int flags = SOCK_NONBLOCK | SOCK_CLOEXEC;
            this->set_socket_info_type(sock.socket_domain());
            if (std::holds_alternative<unspecified_socket_info>(sock_info)) {
                ::io_uring_prep_multishot_accept_direct(sqe, sock.index(),
                    nullptr, nullptr, flags);
            } else {
                auto [addr, addrlen] = this->get_accept_addr_params();
                ::io_uring_prep_multishot_accept_direct(sqe, sock.index(),
                    addr, addrlen, flags);
            }
            sqe->flags |= IOSQE_FIXED_FILE;
        }

        void do_callback(int ev, std::uint32_t cqe_flags) IOUXX_CALLBACK_NOEXCEPT_IF(
            utility::eligible_nothrow_callback<callback_type, result_type>) {
            if (ev >= 0) {
                this->from_system_sockaddr();
                std::invoke(callback, multishot_fixed_accept_result{
                    .conn = fixed_connection(sock, ev),
                    .peer = std::move(sock_info),
                    .more = (cqe_flags & IORING_CQE_F_MORE) != 0
                });
            } else {
                std::invoke(callback, utility::fail(-ev));
            }
        }

        ::socklen_t addrlen = 0;
        network::fixed_socket sock;
        [[no_unique_address]] callback_type callback;
    };

    template<utility::not_tag F>
    fixed_socket_multishot_accept_operation(iouxx::ring&, F)
        -> fixed_socket_multishot_accept_operation<std::decay_t<F>>;

    template<typename F, typename... Args>
    fixed_socket_multishot_accept_operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...)
        -> fixed_socket_multishot_accept_operation<F>;

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
            this->is_fixed = false;
            return *this;
        }

        socket_shutdown_operation& socket(const fixed_socket& s) & noexcept {
            this->fd = s.index();
            this->is_fixed = true;
            return *this;
        }

        socket_shutdown_operation& socket(const connection& c) & noexcept {
            this->fd = c.native_handle();
            this->is_fixed = false;
            return *this;
        }

        socket_shutdown_operation& socket(const fixed_connection& c) & noexcept {
            this->fd = c.index();
            this->is_fixed = true;
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
            if (is_fixed) {
                sqe->flags |= IOSQE_FIXED_FILE;
            }
        }

        void do_callback(int ev, std::uint32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
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
        bool is_fixed = false;
        [[no_unique_address]] callback_type callback;
    };

    template<utility::not_tag F>
    socket_shutdown_operation(iouxx::ring&, F) -> socket_shutdown_operation<std::decay_t<F>>;

    template<typename F, typename... Args>
    socket_shutdown_operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...)
        -> socket_shutdown_operation<F>;

} // namespace iouxx::iouops::network

#endif // IOUXX_OPERATION_NETWORK_SOCKET_CONNECTION_H
