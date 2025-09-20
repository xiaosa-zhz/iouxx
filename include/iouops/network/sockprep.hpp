#pragma once
#ifndef IOUXX_OPERATION_NETWORK_SOCKET_PREPARE_H
#define IOUXX_OPERATION_NETWORK_SOCKET_PREPARE_H 1

#ifndef IOUXX_USE_CXX_MODULE

#include <cstddef>
#include <utility>
#include <algorithm>
#include <variant>
#include <utility>
#include <type_traits>

#include "iouringxx.hpp"
#include "socket.hpp"
#include "ip.hpp"
#include "supported.hpp"
#include "iouops/file/openclose.hpp"

#endif // IOUXX_USE_CXX_MODULE

namespace iouxx::details {

    class socket_prep_base : protected iouops::network::socket_config
    {
    public:
        template<typename Self>
        Self& domain(this Self& self, domain domain) noexcept {
            self.d = domain;
            return self;
        }

        template<typename Self>
        Self& type(this Self& self, type type) noexcept {
            self.t = type
                | type::cloexec
                | type::nonblock;
            return self;
        }

        template<typename Self>
        Self& protocol(this Self& self, protocol protocol) noexcept {
            self.p = protocol;
            return self;
        }
    };

} // namespace iouxx::iouops::network

IOUXX_EXPORT
namespace iouxx::inline iouops::network {

    template<utility::eligible_callback<socket> Callback>
    class socket_open_operation
        : public operation_base, public details::socket_prep_base
    {
    public:
        template<utility::not_tag F>
        explicit socket_open_operation(iouxx::ring& ring, F&& f)
            noexcept(utility::nothrow_constructible_callback<F>) :
            operation_base(iouxx::op_tag<socket_open_operation>, ring),
            callback(std::forward<F>(f))
        {}

        template<typename F, typename... Args>
        explicit socket_open_operation(iouxx::ring& ring, std::in_place_type_t<F>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<F, Args...>) :
            operation_base(iouxx::op_tag<socket_open_operation>, ring),
            callback(std::forward<Args>(args)...)
        {}

        using callback_type = Callback;
        using result_type = socket;

        static constexpr std::uint8_t opcode = IORING_OP_SOCKET;

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_socket(
                sqe,
                std::to_underlying(d),
                std::to_underlying(t),
                std::to_underlying(p),
                0
            );
        }

        void do_callback(int ev, std::uint32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
            utility::eligible_nothrow_callback<callback_type, result_type>) {
            if (ev >= 0) {
                std::invoke(callback, socket(ev, d, t, p));
            } else {
                std::invoke(callback, utility::fail(-ev));
            }
        }

        [[no_unique_address]] callback_type callback;
    };

    template<utility::not_tag F>
    socket_open_operation(iouxx::ring&, F) -> socket_open_operation<std::decay_t<F>>;

    template<typename F, typename... Args>
    socket_open_operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...)
        -> socket_open_operation<F>;

    template<utility::eligible_callback<fixed_socket> Callback>
    class fixed_socket_open_operation
        : public operation_base, public details::socket_prep_base
    {
    public:
        template<utility::not_tag F>
        explicit fixed_socket_open_operation(iouxx::ring& ring, F&& f)
            noexcept(utility::nothrow_constructible_callback<F>) :
            operation_base(iouxx::op_tag<fixed_socket_open_operation>, ring),
            callback(std::forward<F>(f))
        {}

        template<typename F, typename... Args>
        explicit fixed_socket_open_operation(iouxx::ring& ring, std::in_place_type_t<F>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<F, Args...>) :
            operation_base(iouxx::op_tag<fixed_socket_open_operation>, ring),
            callback(std::forward<Args>(args)...)
        {}

        using callback_type = Callback;
        using result_type = void;

        static constexpr std::uint8_t opcode = IORING_OP_SOCKET;

        fixed_socket_open_operation& index(int index = IORING_FILE_INDEX_ALLOC) & noexcept {
            this->file_index = index;
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_socket_direct(sqe,
                std::to_underlying(d),
                std::to_underlying(t),
                std::to_underlying(p),
                file_index,
                0
            );
        }

        void do_callback(int ev, std::uint32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
            utility::eligible_nothrow_callback<callback_type, result_type>) {
            if (ev >= 0) {
                std::invoke(callback, fixed_socket(ev, d, t, p));
            } else {
                std::invoke(callback, utility::fail(-ev));
            }
        }

        int file_index = IORING_FILE_INDEX_ALLOC;
        [[no_unique_address]] callback_type callback;
    };

    template<utility::not_tag F>
    fixed_socket_open_operation(iouxx::ring&, F)
        -> fixed_socket_open_operation<std::decay_t<F>>;

    template<typename F, typename... Args>
    fixed_socket_open_operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...)
        -> fixed_socket_open_operation<F>;

    template<typename Callback>
    class socket_close_operation : public file::file_close_operation<Callback>
    {
        using base = file::file_close_operation<Callback>;
    public:
        using base::base; // Inherit constructors

        socket_close_operation& socket(const socket& s) & noexcept {
            this->base::file(s);
            return *this;
        }

        socket_close_operation& socket(const fixed_socket& s) & noexcept {
            this->base::file(s);
            return *this;
        }

        // Shadow the file_close_operation::file methods to avoid misuse
        void file(const file::file&) & noexcept = delete;
        void file(const file::fixed_file&) & noexcept = delete;
    };

    template<utility::eligible_callback<void> Callback>
    class socket_bind_operation : public operation_base
    {
    public:
        template<utility::not_tag F>
        explicit socket_bind_operation(iouxx::ring& ring, F&& f)
            noexcept(utility::nothrow_constructible_callback<F>) :
            operation_base(iouxx::op_tag<socket_bind_operation>, ring),
            callback(std::forward<F>(f))
        {}

        template<typename F, typename... Args>
        explicit socket_bind_operation(iouxx::ring& ring, std::in_place_type_t<F>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<F, Args...>) :
            operation_base(iouxx::op_tag<socket_bind_operation>, ring),
            callback(std::forward<Args>(args)...)
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
            socket_config::domain domain = std::visit(
                [](auto& si) { return si.domain; },
                sock_info);
            return domain == sock.visit(
                [](auto& s) { return s.socket_domain(); }
            );
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            assert(check());
            auto [addr, addrlen] = sock_info.visit(
                [this](auto& si) -> utility::system_addrsock_info {
                    return { .addr = reinterpret_cast<::sockaddr*>(
                        new (&sockaddr_buf) auto(si.to_system_sockaddr())
                    ), .addrlen = sizeof(si.to_system_sockaddr()) };
                }
            );
            sock.visit(utility::overloaded{
                [&, this](const network::socket& s) {
                    ::io_uring_prep_bind(sqe, s.native_handle(), addr, addrlen);
                },
                [&, this](const network::fixed_socket& s){
                    ::io_uring_prep_bind(sqe, s.index(), addr, addrlen);
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

        alignas(std::max_align_t) sockaddr_buffer_type sockaddr_buf{};
        socket_variant sock;
        supported_socket_type sock_info;
        [[no_unique_address]] callback_type callback;
    };

    template<utility::not_tag F>
    socket_bind_operation(iouxx::ring&, F) -> socket_bind_operation<std::decay_t<F>>;
    
    template<typename F, typename... Args>
    socket_bind_operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...)
        -> socket_bind_operation<F>;

} // namespace iouxx::iouops::network

#endif // IOUXX_OPERATION_NETWORK_SOCKET_PREPARE_H
