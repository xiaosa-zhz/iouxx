#pragma once
#include <sys/socket.h>
#ifndef IOUXX_OPERATION_NETWORK_SUPPORTED_H
#define IOUXX_OPERATION_NETWORK_SUPPORTED_H 1

/*
    * This file is a configuration header for supported socket types.
    * Currently only IPv4 and IPv6 are supported.
*/

#include <array>
#include <variant>
#include <cstddef>

#include "socket.hpp"
#include "ip.hpp"

namespace iouxx::inline iouops::network {

    // Supported socket types.
	// Socket info type needs to provide:
	//   static constexpr socket::domain domain;
	//   /* system sockaddr type */ to_system_sockaddr() const noexcept;
	using supported_socket_type = std::variant<
        unspecified_socket_info,
        ip::socket_v4_info,
        ip::socket_v6_info
    >;

	inline constexpr std::array supported_domains =
		[]<std::size_t... Is>(std::index_sequence<Is...>) {
		return std::array{
			std::variant_alternative_t<Is, supported_socket_type>::domain...
		};
	}(std::make_index_sequence<std::variant_size_v<supported_socket_type>>{});

	inline constexpr std::size_t sockaddr_buffer_size =
		[]<std::size_t... Is>(std::index_sequence<Is...>) {
		return std::ranges::max({ sizeof(::sockaddr),
			sizeof(std::declval<std::variant_alternative_t<Is, supported_socket_type>&>()
				.to_system_sockaddr())...
		});
	}(std::make_index_sequence<std::variant_size_v<supported_socket_type>>{});

	using sockaddr_buffer_type = std::array<std::byte, sockaddr_buffer_size>;

} // namespace iouxx::iouops::network

#endif // IOUXX_OPERATION_NETWORK_SUPPORTED_H
