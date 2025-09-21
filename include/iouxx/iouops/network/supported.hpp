#pragma once
#ifndef IOUXX_OPERATION_NETWORK_SUPPORTED_H
#define IOUXX_OPERATION_NETWORK_SUPPORTED_H 1

/*
    * This file is a configuration header for supported socket types.
    * Currently only IPv4 and IPv6 are supported.
*/

#ifndef IOUXX_USE_CXX_MODULE

#include <array>
#include <variant>
#include <cstddef>

#include <sys/socket.h>
#include "socket.hpp"
#include "ip.hpp"

#endif // IOUXX_USE_CXX_MODULE

IOUXX_EXPORT
namespace iouxx::inline iouops::network {

    // Supported socket types.
    // Socket info type needs to provide:
    //   static constexpr domain domain;
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

    inline constexpr std::array domain_setters =
        []<std::size_t... Is>(std::index_sequence<Is...>) {
        return std::array<void(*)(supported_socket_type&), sizeof...(Is)>{
            +[](supported_socket_type& sock_info) noexcept {
                sock_info.emplace<Is>();
            }...
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

    constexpr std::size_t domain_to_index(socket_config::domain domain) noexcept {
        constexpr std::array domain_to_index_map = []{
            std::array<std::size_t, std::to_underlying(socket_config::domain::max)> map{};
            for (std::size_t i = 0; i < supported_domains.size(); ++i) {
                map[std::to_underlying(supported_domains[i])] = i;
            }
            return map;
        }();
        auto idx = std::to_underlying(domain);
        if (idx >= domain_to_index_map.size()) {
            return 0; // unspec
        }
        return domain_to_index_map[idx];
    }

} // namespace iouxx::iouops::network

#endif // IOUXX_OPERATION_NETWORK_SUPPORTED_H
