#pragma once
#ifndef IOUXX_OPERATION_NETWORK_SOCKET_SEND_RECEIVE_H
#define IOUXX_OPERATION_NETWORK_SOCKET_SEND_RECEIVE_H 1

#ifndef IOUXX_USE_CXX_MODULE

#include <cstddef>
#include <utility>
#include <functional>
#include <utility>
#include <type_traits>
#include <variant>

#include "iouringxx.hpp"
#include "util/utility.hpp"
#include "macro_config.hpp"
#include "socket.hpp"

#endif // IOUXX_USE_CXX_MODULE

IOUXX_EXPORT
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

    constexpr send_flag& operator|=(send_flag& lhs, send_flag rhs) noexcept {
        lhs = lhs | rhs;
        return lhs;
    }

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

    constexpr recv_flag& operator|=(recv_flag& lhs, recv_flag rhs) noexcept {
        lhs = lhs | rhs;
        return lhs;
    }

    enum class ioprio {
        none = 0,
        rs_poll_first = IORING_RECVSEND_POLL_FIRST,
        r_multishot = IORING_RECV_MULTISHOT,
        rs_fixed_buf = IORING_RECVSEND_FIXED_BUF,
        s_zc_report_usage = IORING_SEND_ZC_REPORT_USAGE,
        rs_bundle = IORING_RECVSEND_BUNDLE,
    };

    constexpr ioprio operator|(ioprio lhs, ioprio rhs) noexcept {
        return static_cast<ioprio>(
            std::to_underlying(lhs) | std::to_underlying(rhs)
        );
    }

    constexpr ioprio& operator|=(ioprio& lhs, ioprio rhs) noexcept {
        lhs = lhs | rhs;
        return lhs;
    }

} // namespace iouxx::iouops::network

namespace iouxx::details {

    class send_recv_socket_base
    {
    public:
        template<typename Self>
        Self& socket(this Self& self, const iouops::network::socket& s) noexcept {
            self.fd = s.native_handle();
            self.is_fixed = false;
            return self;
        }

        template<typename Self>
        Self& socket(this Self& self, const iouops::network::fixed_socket& s) noexcept {
            self.fd = s.index();
            self.is_fixed = true;
            return self;
        }

        template<typename Self>
        Self& socket(this Self& self, const iouops::network::connection& c) noexcept {
            self.fd = c.native_handle();
            self.is_fixed = false;
            return self;
        }

        template<typename Self>
        Self& socket(this Self& self, const iouops::network::fixed_connection& c) noexcept {
            self.fd = c.index();
            self.is_fixed = true;
            return self;
        }

    protected:
        int fd = -1;
        bool is_fixed = false;
    };

    class send_op_base
    {
    public:
        using send_flag = iouops::network::send_flag;
        using ioprio = iouops::network::ioprio;

        template<typename Self, utility::readonly_buffer_like Buffer>
        Self& buffer(this Self& self, Buffer&& buf, int buf_index = -1) noexcept {
            auto span = utility::to_readonly_buffer(std::forward<Buffer>(buf));
            self.buf = span.data();
            self.len = span.size();
            self.buf_index = buf_index;
            return self;
        }

        template<typename Self>
        Self& options(this Self& self, send_flag flags) noexcept {
            self.flags = flags;
            return self;
        }

        template<typename Self>
        Self& ring_options(this Self& self, ioprio ring_flags) noexcept {
            self.ring_flags = ring_flags;
            return self;
        }

    protected:
        const void* buf = nullptr;
        std::size_t len = 0;
        send_flag flags = send_flag::none;
        ioprio ring_flags = ioprio::none;
        int buf_index = -1;
    };

} // namespace iouxx::details

IOUXX_EXPORT
namespace iouxx::inline iouops::network {

    template<utility::eligible_callback<std::size_t> Callback>
    class socket_send_operation :
        public operation_base,
        public details::send_recv_socket_base,
        public details::send_op_base
    {
    public:
        template<utility::not_tag F>
        explicit socket_send_operation(iouxx::ring& ring, F&& f)
            noexcept(utility::nothrow_constructible_callback<F>) :
            operation_base(iouxx::op_tag<socket_send_operation>, ring),
            callback(std::forward<F>(f))
        {}

        template<typename F, typename... Args>
        explicit socket_send_operation(iouxx::ring& ring, std::in_place_type_t<F>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<F, Args...>) :
            operation_base(iouxx::op_tag<socket_send_operation>, ring),
            callback(std::forward<Args>(args)...)
        {}

        using callback_type = Callback;
        using result_type = std::size_t;

        static constexpr std::uint8_t opcode = IORING_OP_SEND;

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_send(sqe, fd, buf, len,
                std::to_underlying(flags));
            sqe->ioprio = std::to_underlying(ring_flags);
            if (is_fixed) {
                sqe->flags |= IOSQE_FIXED_FILE;
            }
            if (buf_index >= 0) {
                sqe->buf_index = buf_index;
                sqe->ioprio |= IORING_RECVSEND_FIXED_BUF;
            }
        }

        void do_callback(int ev, std::uint32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
            utility::eligible_nothrow_callback<callback_type, result_type>) {
            if (ev >= 0) {
                std::invoke(callback, static_cast<std::size_t>(ev));
            } else {
                std::invoke(callback, utility::fail(-ev));
            } 
        }

        [[no_unique_address]] callback_type callback;
    };

    template<utility::not_tag F>
    socket_send_operation(iouxx::ring&, F) -> socket_send_operation<std::decay_t<F>>;

    template<typename F, typename... Args>
    socket_send_operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...)
        -> socket_send_operation<F>;

    struct buffer_free_notification {};

    struct send_result_more {
        std::size_t bytes_sent;
    };

    struct send_result_nomore {
        std::size_t bytes_sent;
    };
    
    using send_zc_result =
        std::variant<buffer_free_notification, send_result_more, send_result_nomore>;

    template<utility::eligible_callback<send_zc_result> Callback>
    class socket_send_zc_operation :
        public operation_base,
        public details::send_recv_socket_base,
        public details::send_op_base
    {
    public:
        template<utility::not_tag F>
        explicit socket_send_zc_operation(iouxx::ring& ring, F&& f)
            noexcept(utility::nothrow_constructible_callback<F>) :
            operation_base(iouxx::op_tag<socket_send_zc_operation>, ring),
            callback(std::forward<F>(f))
        {}

        template<typename F, typename... Args>
        explicit socket_send_zc_operation(iouxx::ring& ring, std::in_place_type_t<F>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<F, Args...>) :
            operation_base(iouxx::op_tag<socket_send_zc_operation>, ring),
            callback(std::forward<Args>(args)...)
        {}

        using callback_type = Callback;
        using result_type = send_zc_result;

        static constexpr std::uint8_t opcode = IORING_OP_SEND_ZC;

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_send_zc(sqe, fd, buf, len,
                std::to_underlying(flags),
                std::to_underlying(ring_flags));
            if (is_fixed) {
                sqe->flags |= IOSQE_FIXED_FILE;
            }
            if (buf_index >= 0) {
                sqe->buf_index = buf_index;
                sqe->ioprio |= IORING_RECVSEND_FIXED_BUF;
            }
        }

        void do_callback(int ev, std::uint32_t cqe_flags) IOUXX_CALLBACK_NOEXCEPT_IF(
            utility::eligible_nothrow_callback<callback_type, result_type>) {
            if (ev >= 0) {
                // Send ZC may produce two CQEs, one for bytes sent,
                // and one for notification of buffer is free to reuse.
                const bool more = (cqe_flags & IORING_CQE_F_MORE) != 0;
                if (more) {
                    std::invoke(callback, send_zc_result(
                        send_result_more{ .bytes_sent = static_cast<std::size_t>(ev) }
                    ));
                } else {
                    const bool notify = (cqe_flags & IORING_CQE_F_NOTIF) != 0;
                    if (notify) {
                        std::invoke(callback, send_zc_result(
                            buffer_free_notification{}
                        ));
                    } else {
                        std::invoke(callback, send_zc_result(
                            send_result_nomore{ .bytes_sent = static_cast<std::size_t>(ev) }
                        ));
                    }
                }
            } else {
                std::invoke(callback, utility::fail(-ev));
            }
        }

        [[no_unique_address]] callback_type callback;
    };

    template<utility::eligible_callback<std::size_t> Callback>
    class socket_recv_operation
        : public operation_base, public details::send_recv_socket_base
    {
    public:
        template<utility::not_tag F>
        explicit socket_recv_operation(iouxx::ring& ring, F&& f)
            noexcept(utility::nothrow_constructible_callback<F>) :
            operation_base(iouxx::op_tag<socket_recv_operation>, ring),
            callback(std::forward<F>(f))
        {}

        template<typename F, typename... Args>
        explicit socket_recv_operation(iouxx::ring& ring, std::in_place_type_t<F>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<F, Args...>) :
            operation_base(iouxx::op_tag<socket_recv_operation>, ring),
            callback(std::forward<Args>(args)...)
        {}

        using callback_type = Callback;
        using result_type = std::size_t;

        static constexpr std::uint8_t opcode = IORING_OP_RECV;

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
            if (is_fixed) {
                sqe->flags |= IOSQE_FIXED_FILE;
            }
        }

        void do_callback(int ev, std::uint32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
            utility::eligible_nothrow_callback<callback_type, result_type>) {
            if (ev >= 0) {
                std::invoke(callback, static_cast<std::size_t>(ev));
            } else {
                std::invoke(callback, utility::fail(-ev));
            }
        }

        void* buf = nullptr;
        std::size_t len = 0;
        recv_flag flags = recv_flag::none;
        [[no_unique_address]] callback_type callback;
    };

    template<utility::not_tag F>
    socket_recv_operation(iouxx::ring&, F) -> socket_recv_operation<std::decay_t<F>>;

    template<typename F, typename... Args>
    socket_recv_operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...)
        -> socket_recv_operation<F>;

} // namespace iouxx::iouops::network

#endif // IOUXX_OPERATION_NETWORK_SOCKET_SEND_RECEIVE_H
