#pragma once
#ifndef IOUXX_OPERATION_SOCKETIO_H
#define IOUXX_OPERATION_SOCKETIO_H 1

#include "iouringxx.hpp"
#include "network/ip.hpp"

namespace iouxx::inline iouops::network {

	enum class socket_domain
	{
		unspec = AF_UNSPEC,
		local = AF_LOCAL,
		unix = AF_UNIX,
		ipv4 = AF_INET,
		ipv6 = AF_INET6,
		// ax25 = AF_AX25,
		// ipx = AF_IPX,
		// appletalk = AF_APPLETALK,
		// netrom = AF_NETROM,
		// brd = AF_BLUETOOTH,
		// atmpvc = AF_ATMPVC,
		// x25 = AF_X25,
		// packet = AF_PACKET,
		// alg = AF_ALG,
		// nfc = AF_NFC,
		// vnet = AF_VSOCK,
		// max = AF_MAX
	};

	template<typename Callback>
	class socket_operation : public operation_base
	{
	public:
		template<typename F>
		explicit socket_operation(iouxx::io_uring_xx& ring, F&& f) :
			operation_base(iouxx::op_tag<socket_operation>, &ring.native()),
			callback(std::forward<F>(f))
		{}

	private:
		friend operation_base;
		void build(::io_uring_sqe* sqe) & noexcept {
			// TODO
		}

		void do_callback(int ev, std::int32_t) {
			// TODO
		}

		Callback callback;
	};

	template<typename F>
	socket_operation(iouxx::io_uring_xx&, F) -> socket_operation<std::decay_t<F>>;

	template<typename Callback>
	class socket_bind_operation : public operation_base
	{
	public:
		template<typename F>
		explicit socket_bind_operation(iouxx::io_uring_xx& ring, F&& f) :
			operation_base(iouxx::op_tag<socket_bind_operation>, &ring.native()),
			callback(std::forward<F>(f))
		{}

	private:
		friend operation_base;
		void build(::io_uring_sqe* sqe) & noexcept {
			// TODO
		}

		void do_callback(int ev, std::int32_t) {
			// TODO
		}

		Callback callback;
	};

} // namespace iouxx::iouops::network

#endif // IOUXX_OPERATION_SOCKETIO_H
