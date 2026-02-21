#pragma once
#ifndef IOUXX_OPERATION_NETWORK_SOCKET_PREPARE_H
#define IOUXX_OPERATION_NETWORK_SOCKET_PREPARE_H 1

#ifndef IOUXX_USE_CXX_MODULE

#include <cstddef>
#include <utility>
#include <variant>
#include <utility>
#include <type_traits>

#include "iouxx/iouringxx.hpp"
#include "iouxx/util/utility.hpp"
#include "iouxx/util/assertion.hpp"
#include "socket.hpp"
#include "iouxx/iouops/file/openclose.hpp"

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
            self.t = type | type::nonblock;
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
    class socket_open_operation final : public operation_base, public details::socket_prep_base
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
                std::to_underlying(t | type::cloexec),
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
    class fixed_socket_open_operation final : public operation_base, public details::socket_prep_base
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
        using result_type = fixed_socket;

        static constexpr std::uint8_t opcode = IORING_OP_SOCKET;

        fixed_socket_open_operation& index(int index = fileops::alloc_index) & noexcept {
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

        int file_index = fileops::alloc_index;
        [[no_unique_address]] callback_type callback;
    };

    template<utility::not_tag F>
    fixed_socket_open_operation(iouxx::ring&, F)
        -> fixed_socket_open_operation<std::decay_t<F>>;

    template<typename F, typename... Args>
    fixed_socket_open_operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...)
        -> fixed_socket_open_operation<F>;

    template<utility::eligible_callback<void> Callback>
    class socket_close_operation final : public operation_base
    {
    public:
        template<utility::not_tag F>
        explicit socket_close_operation(iouxx::ring& ring, F&& f)
            noexcept(utility::nothrow_constructible_callback<F>) :
            operation_base(iouxx::op_tag<socket_close_operation>, ring),
            callback(std::forward<F>(f))
        {}

        template<typename F, typename... Args>
        explicit socket_close_operation(iouxx::ring& ring, std::in_place_type_t<F>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<F, Args...>) :
            operation_base(iouxx::op_tag<socket_close_operation>, ring),
            callback(std::forward<Args>(args)...)
        {}

        using callback_type = Callback;
        using result_type = void;

        static constexpr std::uint8_t opcode = IORING_OP_CLOSE;

        socket_close_operation& socket(const socket& s) & noexcept {
            this->fd = s.native_handle();
            this->is_fixed = false;
            return *this;
        }

        socket_close_operation& socket(const fixed_socket& s) & noexcept {
            this->fd = s.index();
            this->is_fixed = true;
            return *this;
        }

        socket_close_operation& socket(const connection& c) & noexcept {
            this->fd = c.native_handle();
            this->is_fixed = false;
            return *this;
        }

        socket_close_operation& socket(const fixed_connection& c) & noexcept {
            this->fd = c.index();
            this->is_fixed = true;
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            if (is_fixed) {
                ::io_uring_prep_close_direct(sqe, fd);
            } else {
                ::io_uring_prep_close(sqe, fd);
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

        int fd = -1;
        bool is_fixed = false;
        [[no_unique_address]] callback_type callback;
    };

    template<typename SocketInfo>
    class socket_bind
    {
        using socket_info_type = SocketInfo;
        using system_sockaddr_type = decltype(std::declval<const socket_info_type&>().to_system_sockaddr());
    public:
        template<utility::eligible_callback<void> Callback>
        class operation final : public operation_base
        {
        public:
            template<utility::not_tag F>
            explicit operation(iouxx::ring& ring, F&& f)
                noexcept(utility::nothrow_constructible_callback<F>) :
                operation_base(iouxx::op_tag<operation>, ring),
                callback(std::forward<F>(f))
            {}

            template<typename F, typename... Args>
            explicit operation(iouxx::ring& ring, std::in_place_type_t<F>, Args&&... args)
                noexcept(std::is_nothrow_constructible_v<F, Args...>) :
                operation_base(iouxx::op_tag<operation>, ring),
                callback(std::forward<Args>(args)...)
            {}

            using callback_type = Callback;
            using result_type = void;

            static constexpr std::uint8_t opcode = IORING_OP_BIND;

            operation& socket(const socket& s) & noexcept {
                IOUXX_ASSERT(socket_info_type::domain == s.socket_domain());
                this->sock = s;
                return *this;
            }

            operation& socket(const fixed_socket& s) & noexcept {
                IOUXX_ASSERT(socket_info_type::domain == s.socket_domain());
                this->sock = s;
                return *this;
            }

            operation& socket_info(const socket_info_type& addr) & noexcept {
                new (&this->sockaddr) system_sockaddr_type(addr.to_system_sockaddr());
                return *this;
            }

        private:
            friend operation_base;
            void build(::io_uring_sqe* sqe) & noexcept {
                sock.visit(utility::overloaded{
                    [&, this](const network::socket& s) {
                        ::io_uring_prep_bind(sqe, s.native_handle(),
                            reinterpret_cast<::sockaddr*>(&this->sockaddr),
                            sizeof(system_sockaddr_type));
                    },
                    [&, this](const network::fixed_socket& s) {
                        ::io_uring_prep_bind(sqe, s.index(),
                            reinterpret_cast<::sockaddr*>(&this->sockaddr),
                            sizeof(system_sockaddr_type));
                        sqe->flags |= IOSQE_FIXED_FILE;
                    }
                });
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

            socket_variant sock;
            system_sockaddr_type sockaddr = {};
            [[no_unique_address]] callback_type callback;
        };

        template<utility::not_tag F>
        operation(iouxx::ring&, F) -> operation<std::decay_t<F>>;
        
        template<typename F, typename... Args>
        operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...)
            -> operation<F>;
    };

} // namespace iouxx::iouops::network

#endif // IOUXX_OPERATION_NETWORK_SOCKET_PREPARE_H
