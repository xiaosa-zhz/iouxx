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

#include "iouxx/iouringxx.hpp"
#include "iouxx/util/utility.hpp"
#include "iouxx/macro_config.hpp"
#include "socket.hpp"

#endif // IOUXX_USE_CXX_MODULE

namespace iouxx::details {

    struct accept_addrinfo {
        ::sockaddr* addr = nullptr;
        ::socklen_t* addrlen = nullptr;
    };

    template<typename Info>
    class peer_socket_info_base
    {
        using info_type = Info;
        using system_sockaddr_type = decltype(std::declval<const info_type&>().to_system_sockaddr());
    public:
        template<typename Self>
        Self& peer_socket_info(this Self& self, const info_type& addr) noexcept {
            new (&self.sockaddr) system_sockaddr_type(addr.to_system_sockaddr());
            return self;
        }

    protected:
        ::sockaddr* addrinfo() noexcept {
            return reinterpret_cast<::sockaddr*>(&sockaddr);
        }

        static constexpr std::size_t addrlen() noexcept {
            return sizeof(system_sockaddr_type);
        }

        system_sockaddr_type sockaddr = {};
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

        static constexpr std::size_t DEFAULT_BACKLOG = 4096;

        socket_listen_operation& socket(const network::socket& s) & noexcept {
            this->sock = s;
            return *this;
        }

        socket_listen_operation& socket(const network::fixed_socket& s) & noexcept {
            this->sock = s;
            return *this;
        }

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

        socket_variant sock = {};
        int bl = DEFAULT_BACKLOG;
        [[no_unique_address]] callback_type callback;
    };

    template<utility::not_tag F>
    socket_listen_operation(iouxx::ring&, F) -> socket_listen_operation<std::decay_t<F>>;

    template<typename F, typename... Args>
    socket_listen_operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...)
        -> socket_listen_operation<F>;

    template<typename PeerInfo>
    class socket_connect
    {
        using info_type = PeerInfo;
    public:
        template<utility::eligible_callback<void> Callback>
        class operation : public operation_base,
            public details::peer_socket_info_base<info_type>
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

            static constexpr std::uint8_t opcode = IORING_OP_CONNECT;

            operation& socket(const network::socket& s) & noexcept {
                this->sock = s;
                return *this;
            }

            operation& socket(const network::fixed_socket& s) & noexcept {
                this->sock = s;
                return *this;
            }

        private:
            friend operation_base;
            void build(::io_uring_sqe* sqe) & noexcept {
                sock.visit(utility::overloaded{
                    [&, this](const network::socket& s) {
                        ::io_uring_prep_connect(sqe, s.native_handle(),
                            this->addrinfo(), this->addrlen());
                    },
                    [&, this](const network::fixed_socket& s) {
                        ::io_uring_prep_connect(sqe, s.index(),
                            this->addrinfo(), this->addrlen());
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

            socket_variant sock = {};
            [[no_unique_address]] callback_type callback;
        };

        template<utility::not_tag F>
        operation(iouxx::ring&, F) -> operation<std::decay_t<F>>;

        template<typename F, typename... Args>
        operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...)
            -> operation<F>;
    };

    template<typename PeerInfo>
    struct accept_result {
        using info_type = PeerInfo;
        connection conn;
        info_type peer;
    };

    template<typename PeerInfo>
    class socket_accept
    {
        using info_type = PeerInfo;
        using accept_result_type = accept_result<info_type>;
    public:
        template<typename Callback>
            requires utility::eligible_alternative_callback<Callback, accept_result_type, connection>
        class operation : public operation_base,
            public details::peer_socket_info_base<info_type>
        {
            using accept_result = accept_result_type;
            using chosen_traits = utility::chosen_result<
                Callback, accept_result, connection
            >;
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
            using result_type = chosen_traits::type;

            static constexpr std::uint8_t opcode = IORING_OP_ACCEPT;

            operation& socket(const socket& s) & noexcept {
                this->sock = s;
                return *this;
            }

        private:
            friend operation_base;
            void build(::io_uring_sqe* sqe) & noexcept {
                int flags = SOCK_NONBLOCK | SOCK_CLOEXEC;
                addrlen_out = this->addrlen();
                ::io_uring_prep_accept(sqe, sock.native_handle(),
                    this->addrinfo(), &addrlen_out, flags);
            }

            void do_callback(int ev, std::uint32_t) IOUXX_CALLBACK_NOEXCEPT_IF(chosen_traits::nothrow) {
                if (ev >= 0) {
                    if constexpr (std::same_as<result_type, accept_result>) {
                        std::invoke(callback, accept_result{
                            .conn = connection(sock, ev),
                            .peer = info_type::from_system_sockaddr(
                                this->addrinfo(),
                                &addrlen_out
                            )
                        });
                    } else {
                        std::invoke(callback, connection(sock, ev));
                    }
                } else {
                    std::invoke(callback, utility::fail(-ev));
                }
            }

            ::socklen_t addrlen_out = 0;
            network::socket sock;
            [[no_unique_address]] callback_type callback;
        };

        template<utility::not_tag F>
        operation(iouxx::ring&, F) -> operation<std::decay_t<F>>;

        template<typename F, typename... Args>
        operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...)
            -> operation<F>;
    };

    template<>
    class socket_accept<void>
    {
    public:
        template<typename Callback>
            requires utility::eligible_callback<Callback, connection>
        class operation : public operation_base
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
            using result_type = connection;

            static constexpr std::uint8_t opcode = IORING_OP_ACCEPT;

            operation& socket(const socket& s) & noexcept {
                this->sock = s;
                return *this;
            }

        private:
            friend operation_base;
            void build(::io_uring_sqe* sqe) & noexcept {
                int flags = SOCK_NONBLOCK | SOCK_CLOEXEC;
                ::io_uring_prep_accept(sqe, sock.native_handle(),
                    nullptr, nullptr, flags);
            }

            void do_callback(int ev, std::uint32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
                utility::eligible_nothrow_callback<callback_type, result_type>) {
                if (ev >= 0) {
                    std::invoke(callback, connection(sock, ev));
                } else {
                    std::invoke(callback, utility::fail(-ev));
                }
            }

            ::socklen_t addrlen_out = 0;
            network::socket sock;
            [[no_unique_address]] callback_type callback;
        };

        template<utility::not_tag F>
        operation(iouxx::ring&, F) -> operation<std::decay_t<F>>;

        template<typename F, typename... Args>
        operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...)
            -> operation<F>;
    };

    template<typename Callback>
    using socket_accept_operation = typename socket_accept<void>::template operation<Callback>;

    template<typename PeerInfo>
    struct fixed_accept_result {
        using info_type = PeerInfo;
        fixed_connection conn;
        info_type peer;
    };

    template<typename PeerInfo>
    class fixed_socket_accept
    {
        using info_type = PeerInfo;
        using accept_result_type = fixed_accept_result<info_type>;
    public:
        template<typename Callback>
            requires utility::eligible_alternative_callback<Callback, accept_result_type, fixed_connection>
        class operation : public operation_base,
            public details::peer_socket_info_base<info_type>
        {
            using fixed_accept_result = accept_result_type;
            using chosen_traits = utility::chosen_result<
                Callback, fixed_accept_result, fixed_connection
            >;
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
            using result_type = chosen_traits::type;

            static constexpr std::uint8_t opcode = IORING_OP_ACCEPT;

            operation& socket(const fixed_socket& s) & noexcept {
                this->sock = s;
                return *this;
            }

            operation& index(int index = IORING_FILE_INDEX_ALLOC) & noexcept {
                this->file_index = index;
                return *this;
            }

        private:
            friend operation_base;
            void build(::io_uring_sqe* sqe) & noexcept {
                int flags = SOCK_NONBLOCK | SOCK_CLOEXEC;
                addrlen_out = this->addrlen();
                ::io_uring_prep_accept_direct(sqe, sock.index(),
                    this->addrinfo(), &addrlen_out, flags, file_index);
                sqe->flags |= IOSQE_FIXED_FILE;
            }

            void do_callback(int ev, std::uint32_t) IOUXX_CALLBACK_NOEXCEPT_IF(chosen_traits::nothrow) {
                if (ev >= 0) {
                    if constexpr (std::same_as<result_type, fixed_accept_result>) {
                        std::invoke(callback, fixed_accept_result{
                            .conn = fixed_connection(sock, ev),
                            .peer = info_type::from_system_sockaddr(
                                this->addrinfo(),
                                &addrlen_out
                            )
                        });
                    } else {
                        std::invoke(callback, fixed_connection(sock, ev));
                    }
                } else {
                    std::invoke(callback, utility::fail(-ev));
                }
            }

            int file_index = IORING_FILE_INDEX_ALLOC;
            ::socklen_t addrlen_out = 0;
            network::fixed_socket sock;
            [[no_unique_address]] callback_type callback;
        };

        template<utility::not_tag F>
        operation(iouxx::ring&, F) -> operation<std::decay_t<F>>;

        template<typename F, typename... Args>
        operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...)
            -> operation<F>;
    };

    template<>
    class fixed_socket_accept<void>
    {
    public:
        template<typename Callback>
            requires utility::eligible_callback<Callback, fixed_connection>
        class operation : public operation_base
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
            using result_type = fixed_connection;

            static constexpr std::uint8_t opcode = IORING_OP_ACCEPT;

            operation& socket(const fixed_socket& s) & noexcept {
                this->sock = s;
                return *this;
            }

            operation& index(int index = IORING_FILE_INDEX_ALLOC) & noexcept {
                this->file_index = index;
                return *this;
            }

        private:
            friend operation_base;
            void build(::io_uring_sqe* sqe) & noexcept {
                int flags = SOCK_NONBLOCK | SOCK_CLOEXEC;
                ::io_uring_prep_accept_direct(sqe, sock.index(),
                    nullptr, nullptr, flags, file_index);
                sqe->flags |= IOSQE_FIXED_FILE;
            }

            void do_callback(int ev, std::uint32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
                utility::eligible_nothrow_callback<callback_type, result_type>) {
                if (ev >= 0) {
                    std::invoke(callback, fixed_connection(sock, ev));
                } else {
                    std::invoke(callback, utility::fail(-ev));
                }
            }

            int file_index = IORING_FILE_INDEX_ALLOC;
            ::socklen_t addrlen_out = 0;
            network::fixed_socket sock;
            [[no_unique_address]] callback_type callback;
        };

        template<utility::not_tag F>
        operation(iouxx::ring&, F) -> operation<std::decay_t<F>>;

        template<typename F, typename... Args>
        operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...)
            -> operation<F>;
    };

    template<typename Callback>
    using fixed_socket_accept_operation = typename fixed_socket_accept<void>::template operation<Callback>;

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
            int flags = SOCK_NONBLOCK | SOCK_CLOEXEC;
            ::io_uring_prep_multishot_accept(sqe, sock.native_handle(),
                nullptr, nullptr, flags);
        }

        void do_callback(int ev, std::uint32_t cqe_flags) IOUXX_CALLBACK_NOEXCEPT_IF(
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
        bool more = false;
    };

    // Note: multishot_accept_direct always allocates all new slots,
    //  so user cannot specify index here.
    template<utility::eligible_callback<multishot_fixed_accept_result> Callback>
    class fixed_socket_multishot_accept_operation : public operation_base
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
            ::io_uring_prep_multishot_accept_direct(sqe, sock.index(),
                nullptr, nullptr, flags);
            sqe->flags |= IOSQE_FIXED_FILE;
        }

        void do_callback(int ev, std::uint32_t cqe_flags) IOUXX_CALLBACK_NOEXCEPT_IF(
            utility::eligible_nothrow_callback<callback_type, result_type>) {
            if (ev >= 0) {
                std::invoke(callback, multishot_fixed_accept_result{
                    .conn = fixed_connection(sock, ev),
                    .more = (cqe_flags & IORING_CQE_F_MORE) != 0
                });
            } else {
                std::invoke(callback, utility::fail(-ev));
            }
        }

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
