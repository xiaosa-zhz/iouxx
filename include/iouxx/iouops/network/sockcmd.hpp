#pragma once
#ifndef IOUXX_IOUOPS_NETWORK_SOCKCMD_H
#define IOUXX_IOUOPS_NETWORK_SOCKCMD_H 1

#ifndef IOUXX_USE_CXX_MODULE

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#include <type_traits>
#include <chrono>
#include <string>
#include <string_view>
#include <functional>

#include "iouxx/util/utility.hpp"
#include "iouxx/iouringxx.hpp"
#include "iouxx/util/assertion.hpp"
#include "socket.hpp"
#include "ip.hpp"

#endif // IOUXX_USE_CXX_MODULE

IOUXX_EXPORT
namespace iouxx::inline iouops::network {

    struct linger_result {
        bool onoff;
        std::chrono::seconds linger;
    };

    struct ipv4_mreq_result {
        network::ip::address_v4 multiaddr;
        network::ip::address_v4 address;
        int interface;
    };

    struct ipv6_mreq_result {
        network::ip::address_v6 multiaddr;
        unsigned int interface = 0;
    };

} // namespace iouxx::iouops::network

namespace iouxx::details {

    template<typename T>
    struct sockopt_params {
        T* optval = nullptr;
        ::socklen_t optlen = sizeof(T);
    };

    class bool_optval_base
    {
    public:
        template<typename Self>
        Self& option(this Self& self, bool value) noexcept {
            ((bool_optval_base&)self).val = value ? 1 : 0;
            return self;
        }

        using result_type = bool;
        bool result() noexcept { return val != 0; }

        sockopt_params<int> params() noexcept {
            return { &val };
        }

        int val = 0;
    };

    class int_optval_base
    {
    public:
        template<typename Self>
        Self& option(this Self& self, int value) noexcept {
            ((int_optval_base&)self).val = value;
            return self;
        }

        using result_type = int;
        int result() noexcept { return val; }

        sockopt_params<int> params() noexcept {
            return { &val };
        }

        int val = 0;
    };

    class linger_optval_base
    {
    public:
        template<typename Self>
        Self& option(this Self& self, bool onoff, std::chrono::seconds linger) noexcept {
            ((linger_optval_base&)self).lg.l_onoff = onoff ? 1 : 0;
            ((linger_optval_base&)self).lg.l_linger = static_cast<int>(linger.count());
            return self;
        }

        using result_type = network::linger_result;
        network::linger_result result() noexcept {
            return { lg.l_onoff != 0, std::chrono::seconds(lg.l_linger) };
        }

        sockopt_params<::linger> params() noexcept {
            return { &lg };
        }

        ::linger lg{ .l_onoff = 0, .l_linger = 0 };
    };

    class timeval_optval_base
    {
    public:
        template<typename Self>
        Self& option(this Self& self, std::chrono::nanoseconds time) noexcept {
            auto sec = std::chrono::duration_cast<std::chrono::seconds>(time);
            auto usec = std::chrono::duration_cast<std::chrono::microseconds>(time - sec);
            ((timeval_optval_base&)self).tv.tv_sec = sec.count();
            ((timeval_optval_base&)self).tv.tv_usec = usec.count();
            return self;
        }

        using result_type = std::chrono::microseconds;
        std::chrono::microseconds result() noexcept {
            return std::chrono::seconds(tv.tv_sec)
                + std::chrono::microseconds(tv.tv_usec);
        }

        sockopt_params<::timeval> params() noexcept {
            return { &tv };
        }

        ::timeval tv{ .tv_sec = 0, .tv_usec = 0 };
    };

    class string_optval_base
    {
    public:
        template<typename Self>
        Self& option(this Self& self, std::string&& str) noexcept {
            ((string_optval_base&)self).str = std::move(str);
            return self;
        }

        template<typename Self>
        Self& option(this Self& self, std::string_view str) noexcept {
            ((string_optval_base&)self).str = str;
            return self;
        }

        using result_type = std::string;
        std::string result() noexcept {
            return std::move(str);
        }

        sockopt_params<char> params() noexcept {
            return { str.data(), static_cast<::socklen_t>(str.size()) };
        }

        std::string str;
    };

    class ip_mreqn_optval_base
    {
        using address = network::ip::address_v4;
    public:
        template<typename Self>
        Self& option(this Self& self,
            const address& multiaddr,
            const address& address = network::ip::address_v4::any(),
            int interface = 0) noexcept {
            ((ip_mreqn_optval_base&)self).val = {
                .imr_multiaddr = multiaddr.to_system_addr(),
                .imr_address = address.to_system_addr(),
                .imr_ifindex = interface
            };
            return self;
        }

        using result_type = network::ipv4_mreq_result;
        network::ipv4_mreq_result result() noexcept {
            return {
                address::from_system_addr(val.imr_multiaddr),
                address::from_system_addr(val.imr_address),
                val.imr_ifindex
            };
        }

        sockopt_params<::ip_mreqn> params() noexcept {
            return { &val };
        }

        ::ip_mreqn val;
    };

    class ipv6_mreq_optval_base
    {
        using address = network::ip::address_v6;
    public:
        template<typename Self>
        Self& option(this Self& self,
            const address& multiaddr,
            unsigned int interface = 0) noexcept {
            ((ipv6_mreq_optval_base&)self).val = {
                .ipv6mr_multiaddr = multiaddr.to_system_addr(),
                .ipv6mr_interface = interface
            };
            return self;
        }

        using result_type = network::ipv6_mreq_result;
        network::ipv6_mreq_result result() noexcept {
            return {
                address::from_system_addr(val.ipv6mr_multiaddr),
                val.ipv6mr_interface  
            };
        }

        sockopt_params<::ipv6_mreq> params() noexcept {
            return { &val };
        }

        ::ipv6_mreq val;
    };

    template<typename OptvalBase>
    class sockopt_base : protected OptvalBase
    {
    public:
        using optval_base = OptvalBase;
        using result_type = optval_base::result_type;

        template<typename Self>
        Self& socket(this Self& self, const network::socket& s) noexcept {
            ((sockopt_base&)self).fd = s.native_handle();
            ((sockopt_base&)self).is_fixed = false;
            return self;
        }

        template<typename Self>
        Self& socket(this Self& self, const network::fixed_socket& s) noexcept {
            ((sockopt_base&)self).fd = s.index();
            ((sockopt_base&)self).is_fixed = true;
            return self;
        }

        template<typename Self>
        Self& socket(this Self& self, const network::connection& c) noexcept {
            ((sockopt_base&)self).fd = c.native_handle();
            ((sockopt_base&)self).is_fixed = false;
            return self;
        }

        template<typename Self>
        Self& socket(this Self& self, const network::fixed_connection& c) noexcept {
            ((sockopt_base&)self).fd = c.index();
            ((sockopt_base&)self).is_fixed = true;
            return self;
        }

        int fd = -1;
        bool is_fixed = false;
    };

} // namespace iouxx::details

IOUXX_EXPORT
namespace iouxx::inline iouops::network {
    
    namespace sockopts {

        namespace general {

            class reuseaddr : protected details::bool_optval_base
            {
            protected:
                static constexpr int level = SOL_SOCKET;
                static constexpr int optname = SO_REUSEADDR;
            };

            class reuseport : protected details::bool_optval_base
            {
            protected:
                static constexpr int level = SOL_SOCKET;
                static constexpr int optname = SO_REUSEPORT;
            };

            class keepalive : protected details::bool_optval_base
            {
            protected:
                static constexpr int level = SOL_SOCKET;
                static constexpr int optname = SO_KEEPALIVE;
            };

            class debug : protected details::bool_optval_base
            {
            protected:
                static constexpr int level = SOL_SOCKET;
                static constexpr int optname = SO_DEBUG;
            };

            class acceptconn : protected details::bool_optval_base
            {
            protected:
                static constexpr int level = SOL_SOCKET;
                static constexpr int optname = SO_ACCEPTCONN;
            };

            class dontroute : protected details::bool_optval_base
            {
            protected:
                static constexpr int level = SOL_SOCKET;
                static constexpr int optname = SO_DONTROUTE;
            };

            class broadcast : protected details::bool_optval_base
            {
            protected:
                static constexpr int level = SOL_SOCKET;
                static constexpr int optname = SO_BROADCAST;
            };

            class oobinline : protected details::bool_optval_base
            {
            protected:
                static constexpr int level = SOL_SOCKET;
                static constexpr int optname = SO_OOBINLINE;
            };

            class rcvbuf : protected details::int_optval_base
            {
            protected:
                static constexpr int level = SOL_SOCKET;
                static constexpr int optname = SO_RCVBUF;
            };

            class sndbuf : protected details::int_optval_base
            {
            protected:
                static constexpr int level = SOL_SOCKET;
                static constexpr int optname = SO_SNDBUF;
            };

            class rcvlowat : protected details::int_optval_base
            {
            protected:
                static constexpr int level = SOL_SOCKET;
                static constexpr int optname = SO_RCVLOWAT;
            };

            class sndlowat : protected details::int_optval_base
            {
            protected:
                static constexpr int level = SOL_SOCKET;
                static constexpr int optname = SO_SNDLOWAT;
            };

            class linger : protected details::linger_optval_base
            {
            protected:
                static constexpr int level = SOL_SOCKET;
                static constexpr int optname = SO_LINGER;
            };

            class rcvtimeo : protected details::timeval_optval_base
            {
            protected:
                static constexpr int level = SOL_SOCKET;
                static constexpr int optname = SO_RCVTIMEO;
            };

            class sndtimeo : protected details::timeval_optval_base
            {
            protected:
                static constexpr int level = SOL_SOCKET;
                static constexpr int optname = SO_SNDTIMEO;
            };

            class priority : protected details::int_optval_base
            {
            protected:
                static constexpr int level = SOL_SOCKET;
                static constexpr int optname = SO_PRIORITY;
            };

            class bindtodevice : protected details::string_optval_base
            {
            protected:
                static constexpr int level = SOL_SOCKET;
                static constexpr int optname = SO_BINDTODEVICE;
            };

        } // namespace iouxx::iouops::network::sockopts::general

        namespace tcp {

#ifdef TCP_NODELAY
            class nodelay : protected details::bool_optval_base
            {
            protected:
                static constexpr int level = IPPROTO_TCP;
                static constexpr int optname = TCP_NODELAY;
            };
#endif // TCP_NODELAY

#ifdef TCP_CORK
            class cork : protected details::bool_optval_base
            {
            protected:
                static constexpr int level = IPPROTO_TCP;
                static constexpr int optname = TCP_CORK;
            };
#endif // TCP_CORK

#ifdef TCP_QUICKACK
            class quickack : protected details::bool_optval_base
            {
            protected:
                static constexpr int level = IPPROTO_TCP;
                static constexpr int optname = TCP_QUICKACK;
            };
#endif // TCP_QUICKACK

#ifdef TCP_SYNCNT
            class syncnt : protected details::int_optval_base
            {
            protected:
                static constexpr int level = IPPROTO_TCP;
                static constexpr int optname = TCP_SYNCNT;
            };
#endif // TCP_SYNCNT

#ifdef TCP_MAXSEG
            class maxseg : protected details::int_optval_base
            {
            protected:
                static constexpr int level = IPPROTO_TCP;
                static constexpr int optname = TCP_MAXSEG;
            };
#endif // TCP_MAXSEG

#ifdef TCP_WINDOW_CLAMP
            class window_clamp : protected details::int_optval_base
            {
            protected:
                static constexpr int level = IPPROTO_TCP;
                static constexpr int optname = TCP_WINDOW_CLAMP;
            };
#endif // TCP_WINDOW_CLAMP

#ifdef TCP_KEEPIDLE
            class keepidle : protected details::int_optval_base
            {
            protected:
                static constexpr int level = IPPROTO_TCP;
                static constexpr int optname = TCP_KEEPIDLE;
            };
#endif // TCP_KEEPIDLE

#ifdef TCP_KEEPINTVL
            class keepintvl : protected details::int_optval_base
            {
            protected:
                static constexpr int level = IPPROTO_TCP;
                static constexpr int optname = TCP_KEEPINTVL;
            };
#endif // TCP_KEEPINTVL

#ifdef TCP_KEEPCNT
            class keepcnt : protected details::int_optval_base
            {
            protected:
                static constexpr int level = IPPROTO_TCP;
                static constexpr int optname = TCP_KEEPCNT;
            };
#endif // TCP_KEEPCNT

#ifdef TCP_USER_TIMEOUT
            class user_timeout : protected details::int_optval_base
            {
            protected:
                static constexpr int level = IPPROTO_TCP;
                static constexpr int optname = TCP_USER_TIMEOUT;
            };
#endif // TCP_USER_TIMEOUT

        } // namespace iouxx::iouops::network::sockopts::tcp

        namespace udp {

#ifdef UDP_CORK
            class cork : protected details::bool_optval_base
            {
            protected:
                static constexpr int level = IPPROTO_UDP;
                static constexpr int optname = UDP_CORK;
            };
#endif // UDP_CORK

#ifdef SO_NO_CHECK
            class no_check : protected details::bool_optval_base
            {
            protected:
                static constexpr int level = SOL_SOCKET;
                static constexpr int optname = SO_NO_CHECK;
            };
#endif // SO_NO_CHECK

        } // namespace iouxx::iouops::network::sockopts::udp

        namespace ipv4 {

#ifdef IP_TOS
            class tos : protected details::int_optval_base
            {
            protected:
                static constexpr int level = IPPROTO_IP;
                static constexpr int optname = IP_TOS;
            };
#endif // IP_TOS

#ifdef IP_TTL
            class ttl : protected details::int_optval_base
            {
            protected:
                static constexpr int level = IPPROTO_IP;
                static constexpr int optname = IP_TTL;
            };
#endif // IP_TTL

#ifdef IP_MTU_DISCOVER
            class mtu_discover : protected details::int_optval_base
            {
            protected:
                static constexpr int level = IPPROTO_IP;
                static constexpr int optname = IP_MTU_DISCOVER;
            };
#endif // IP_MTU_DISCOVER

#ifdef IP_FREEBIND
            class freebind : protected details::bool_optval_base
            {
            protected:
                static constexpr int level = IPPROTO_IP;
                static constexpr int optname = IP_FREEBIND;
            };
#endif // IP_FREEBIND

#ifdef IP_TRANSPARENT
            class transparent : protected details::bool_optval_base
            {
            protected:
                static constexpr int level = IPPROTO_IP;
                static constexpr int optname = IP_TRANSPARENT;
            };
#endif // IP_TRANSPARENT

#ifdef IP_PKTINFO
            class pktinfo : protected details::bool_optval_base
            {
            protected:
                static constexpr int level = IPPROTO_IP;
                static constexpr int optname = IP_PKTINFO;
            };
#endif // IP_PKTINFO

#ifdef IP_RECVTTL
            class recvttl : protected details::bool_optval_base
            {
            protected:
                static constexpr int level = IPPROTO_IP;
                static constexpr int optname = IP_RECVTTL;
            };
#endif // IP_RECVTTL

#ifdef IP_RECVTOS
            class recvtos : protected details::bool_optval_base
            {
            protected:
                static constexpr int level = IPPROTO_IP;
                static constexpr int optname = IP_RECVTOS;
            };
#endif // IP_RECVTOS

#ifdef IP_MULTICAST_TTL
            class multicast_ttl : protected details::int_optval_base
            {
            protected:
                static constexpr int level = IPPROTO_IP;
                static constexpr int optname = IP_MULTICAST_TTL;
            };
#endif // IP_MULTICAST_TTL

#ifdef IP_MULTICAST_LOOP
            class multicast_loop : protected details::bool_optval_base
            {
            protected:
                static constexpr int level = IPPROTO_IP;
                static constexpr int optname = IP_MULTICAST_LOOP;
            };
#endif // IP_MULTICAST_LOOP

#ifdef IP_ADD_MEMBERSHIP
            class add_membership : protected details::ip_mreqn_optval_base
            {
            protected:
                static constexpr int level = IPPROTO_IP;
                static constexpr int optname = IP_ADD_MEMBERSHIP;
            };
#endif // IP_ADD_MEMBERSHIP

#ifdef IP_DROP_MEMBERSHIP
            class drop_membership : protected details::ip_mreqn_optval_base
            {
            protected:
                static constexpr int level = IPPROTO_IP;
                static constexpr int optname = IP_DROP_MEMBERSHIP;
            };
#endif // IP_DROP_MEMBERSHIP

        } // namespace iouxx::iouops::network::sockopts::ipv4

        namespace ipv6 {

#ifdef IPV6_V6ONLY
            class v6only : protected details::bool_optval_base
            {
            protected:
                static constexpr int level = IPPROTO_IPV6;
                static constexpr int optname = IPV6_V6ONLY;
            };
#endif // IPV6_V6ONLY

#ifdef IPV6_TCLASS
            class tclass : protected details::int_optval_base
            {
            protected:
                static constexpr int level = IPPROTO_IPV6;
                static constexpr int optname = IPV6_TCLASS;
            };
#endif // IPV6_TCLASS

#ifdef IPV6_UNICAST_HOPS
            class unicast_hops : protected details::int_optval_base
            {
            protected:
                static constexpr int level = IPPROTO_IPV6;
                static constexpr int optname = IPV6_UNICAST_HOPS;
            };
#endif // IPV6_UNICAST_HOPS

#ifdef IPV6_RECVERR
            class recverr : protected details::bool_optval_base
            {
            protected:
                static constexpr int level = IPPROTO_IPV6;
                static constexpr int optname = IPV6_RECVERR;
            };
#endif // IPV6_RECVERR

#ifdef IPV6_PKTINFO
            class pktinfo : protected details::bool_optval_base
            {
            protected:
                static constexpr int level = IPPROTO_IPV6;
                static constexpr int optname = IPV6_PKTINFO;
            };
#endif // IPV6_PKTINFO

#ifdef IPV6_RECVPKTINFO
            class recvpktinfo : protected details::bool_optval_base
            {
            protected:
                static constexpr int level = IPPROTO_IPV6;
                static constexpr int optname = IPV6_RECVPKTINFO;
            };
#endif // IPV6_RECVPKTINFO

#ifdef IPV6_DONTFRAG
            class dontfrag : protected details::bool_optval_base
            {
            protected:
                static constexpr int level = IPPROTO_IPV6;
                static constexpr int optname = IPV6_DONTFRAG;
            };
#endif // IPV6_DONTFRAG

#ifdef IPV6_RECVTCLASS
            class recvtclass : protected details::bool_optval_base
            {
            protected:
                static constexpr int level = IPPROTO_IPV6;
                static constexpr int optname = IPV6_RECVTCLASS;
            };
#endif // IPV6_RECVTCLASS

#ifdef IPV6_MULTICAST_HOPS
            class multicast_hops : protected details::int_optval_base
            {
            protected:
                static constexpr int level = IPPROTO_IPV6;
                static constexpr int optname = IPV6_MULTICAST_HOPS;
            };
#endif // IPV6_MULTICAST_HOPS

#ifdef IPV6_MULTICAST_LOOP
            class multicast_loop : protected details::bool_optval_base
            {
            protected:
                static constexpr int level = IPPROTO_IPV6;
                static constexpr int optname = IPV6_MULTICAST_LOOP;
            };
#endif // IPV6_MULTICAST_LOOP

#ifdef IPV6_JOIN_GROUP
            class join_group : protected details::ipv6_mreq_optval_base
            {
            protected:
                static constexpr int level = IPPROTO_IPV6;
                static constexpr int optname = IPV6_JOIN_GROUP;
            };
#endif // IPV6_JOIN_GROUP

#ifdef IPV6_LEAVE_GROUP
            class leave_group : protected details::ipv6_mreq_optval_base
            {
            protected:
                static constexpr int level = IPPROTO_IPV6;
                static constexpr int optname = IPV6_LEAVE_GROUP;
            };
#endif // IPV6_LEAVE_GROUP

#ifdef IPV6_UNICAST_IF
            class unicast_if : protected details::int_optval_base
            {
            protected:
                static constexpr int level = IPPROTO_IPV6;
                static constexpr int optname = IPV6_UNICAST_IF;
            };
#endif // IPV6_UNICAST_IF

        } // namespace iouxx::iouops::network::sockopts::ipv6

    } // namespace iouxx::iouops::network::sockopts

    template<typename SockOpt>
    class socket_setoption
    {
        using base = details::sockopt_base<SockOpt>;
    public:
        template<utility::eligible_callback<void> Callback>
        class operation final : public operation_base, private base
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

            static constexpr std::uint8_t opcode = IORING_OP_URING_CMD;

            using base::socket;
            using base::option;

        private:
            friend operation_base;
            void build(::io_uring_sqe* sqe) & noexcept {
                auto [optval, optlen] = base::params();
                ::io_uring_prep_cmd_sock(sqe, SOCKET_URING_OP_SETSOCKOPT, this->fd,
                    base::level, base::optname, optval, optlen);
                if (this->is_fixed) {
                    sqe->flags |= IOSQE_FIXED_FILE;
                }
            }

             void do_callback(int ev, std::uint32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
                utility::eligible_nothrow_callback<callback_type, result_type>) {
                if (ev == 0) {
                    std::invoke_r<void>(callback, utility::void_success());
                } else {
                    std::invoke_r<void>(callback, utility::fail(-ev));
                }
            }

            [[no_unique_address]] callback_type callback;
        };

        template<utility::not_tag F>
        operation(iouxx::ring&, F) -> operation<std::decay_t<F>>;

        template<typename F, typename... Args>
        operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...)
            -> operation<F>;
    };

    template<typename SockOpt>
    class socket_getoption
    {
        using base = details::sockopt_base<SockOpt>;
        using result = base::result_type;
    public:
        template<utility::eligible_callback<result> Callback>
        class operation final : public operation_base, private base
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
            using result_type = result;

            static constexpr std::uint8_t opcode = IORING_OP_URING_CMD;

            using base::socket;

        private:
            friend operation_base;
            void build(::io_uring_sqe* sqe) & noexcept {
                auto [optval, optlen] = base::params();
                ::io_uring_prep_cmd_sock(sqe, SOCKET_URING_OP_GETSOCKOPT, this->fd,
                    base::level, base::optname, optval, optlen);
                if (this->is_fixed) {
                    sqe->flags |= IOSQE_FIXED_FILE;
                }
            }

             void do_callback(int ev, std::uint32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
                utility::eligible_nothrow_callback<callback_type, result_type>) {
                if (ev > 0) {
                    // updated optlen is in ev, discard it
                    std::invoke_r<void>(callback, base::result());
                } else {
                    std::invoke_r<void>(callback, utility::fail(-ev));
                }
            }

            [[no_unique_address]] callback_type callback;
        };

        template<utility::not_tag F>
        operation(iouxx::ring&, F) -> operation<std::decay_t<F>>;

        template<typename F, typename... Args>
        operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...)
            -> operation<F>;
    };

    template<typename SockInfo>
    class socket_getsockname
    {
        using info_type = SockInfo;
        using system_sockaddr_type = decltype(std::declval<const info_type&>().to_system_sockaddr());
    public:
        template<utility::eligible_callback<info_type> Callback>
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
            using result_type = info_type;

            static constexpr std::uint8_t opcode = IORING_OP_URING_CMD;

            operation& socket(const network::socket& s) & noexcept {
                IOUXX_ASSERT(info_type::domain == s.socket_domain());
                this->fd = s.native_handle();
                this->is_fixed = false;
                return *this;
            }

            operation& socket(const network::fixed_socket& s) & noexcept {
                IOUXX_ASSERT(info_type::domain == s.socket_domain());
                this->fd = s.index();
                this->is_fixed = true;
                return *this;
            }

            operation& socket(const network::connection& c) & noexcept {
                IOUXX_ASSERT(info_type::domain == c.socket_domain());
                this->fd = c.native_handle();
                this->is_fixed = false;
                return *this;
            }

            operation& socket(const network::fixed_connection& c) & noexcept {
                IOUXX_ASSERT(info_type::domain == c.socket_domain());
                this->fd = c.index();
                this->is_fixed = true;
                return *this;
            }

            operation& peer(bool is_peer = true) & noexcept {
                this->is_peer = is_peer;
                return *this;
            }

        private:
            friend operation_base;
            void build(::io_uring_sqe* sqe) & noexcept {
                ::io_uring_prep_cmd_getsockname(sqe, this->fd,
                    reinterpret_cast<::sockaddr*>(&this->sockaddr),
                    &this->socklen_out,
                    this->is_peer ? 1 : 0);
                if (this->is_fixed) {
                    sqe->flags |= IOSQE_FIXED_FILE;
                }
            }

            void do_callback(int ev, std::uint32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
                utility::eligible_nothrow_callback<callback_type, result_type>) {
                if (ev >= 0) {
                    std::invoke_r<void>(callback, info_type::from_system_sockaddr(
                        reinterpret_cast<::sockaddr*>(&this->sockaddr), &this->socklen_out
                    ));
                } else {
                    std::invoke_r<void>(callback, utility::fail(-ev));
                }
            }

            int fd = -1;
            ::socklen_t socklen_out = sizeof(system_sockaddr_type);
            bool is_peer = false;
            bool is_fixed = false;
            system_sockaddr_type sockaddr{};
            [[no_unique_address]] callback_type callback;
        };

        template<utility::not_tag F>
        operation(iouxx::ring&, F) -> operation<std::decay_t<F>>;

        template<typename F, typename... Args>
        operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...)
            -> operation<F>;
    };

} // namespace iouxx::iouops::network

#endif // IOUXX_IOUOPS_NETWORK_SOCKCMD_HPP
