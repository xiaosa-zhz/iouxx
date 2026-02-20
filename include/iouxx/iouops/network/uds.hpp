#pragma once

#ifndef IOUXX_OPERATION_NETWORK_UDS_H
#define IOUXX_OPERATION_NETWORK_UDS_H 1

#ifndef IOUXX_USE_CXX_MODULE

#include <sys/socket.h>
#include <sys/un.h>

#include <algorithm>
#include <string_view>
#include <stdexcept>

#include "iouxx/cxxmodule_helper.hpp"
#include "iouxx/util/utility.hpp"
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

        constexpr static uds_info from_system_sockaddr(const ::sockaddr* sockaddr) noexcept {
            const ::sockaddr_un* un_addr = reinterpret_cast<const ::sockaddr_un*>(sockaddr);
            uds_info info;
            std::ranges::copy_n(un_addr->sun_path, sizeof(un_addr->sun_path), info.addr.sun_path);
            return info;
        }

        std::string_view path() const noexcept {
            return std::string_view(&addr.sun_path[0]);
        }

    private:
        ::sockaddr_un addr = { .sun_family = AF_UNIX, .sun_path = {} };
    };

} // namespace iouxx::iouops::network::uds

#endif // IOUXX_OPERATION_NETWORK_UDS_H
