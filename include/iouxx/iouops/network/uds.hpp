#pragma once

#ifndef IOUXX_OPERATION_NETWORK_UDS_H
#define IOUXX_OPERATION_NETWORK_UDS_H 1

#ifndef IOUXX_USE_CXX_MODULE

#include <sys/socket.h>
#include <sys/un.h>

#include <cstring>
#include <string_view>
#include <stdexcept>
#include <algorithm>

#include "iouxx/cxxmodule_helper.hpp"
#include "iouxx/util/utility.hpp"
#include "iouxx/util/assertion.hpp"
#include "socket.hpp"

#endif // IOUXX_USE_CXX_MODULE

IOUXX_EXPORT
namespace iouxx::inline iouops::network::uds {

    class uds_info
    {
    public:
        static constexpr socket_config::domain domain = socket_config::domain::unix;

        constexpr uds_info() = default;

        constexpr explicit uds_info(std::string_view path) {
            if (path.size() >= sizeof(::sockaddr_un::sun_path)) {
                throw std::invalid_argument("Path too long for sockaddr_un");
            }
            std::ranges::copy_n(path.data(), path.size(), addr.sun_path);
            addr.sun_path[path.size()] = '\0';
        }

        constexpr ::sockaddr_un to_system_sockaddr() const noexcept {
            return addr;
        }

        static uds_info from_system_sockaddr(
            const ::sockaddr* sockaddr, const ::socklen_t* addrlen) noexcept {
            IOUXX_ASSERT(*addrlen == sizeof(::sockaddr_un));
            uds_info info;
            std::memcpy(&info.addr, sockaddr, sizeof(::sockaddr_un));
            IOUXX_ASSERT(info.addr.sun_family == AF_UNIX);
            return info;
        }

        constexpr std::string_view path() const noexcept {
            return std::string_view(addr.sun_path);
        }

    private:
        ::sockaddr_un addr = { .sun_family = AF_UNIX, .sun_path = {} };
    };

} // namespace iouxx::iouops::network::uds

#endif // IOUXX_OPERATION_NETWORK_UDS_H
