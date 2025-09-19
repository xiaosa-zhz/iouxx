#pragma once
#ifndef IOUXX_OPERATION_NETWORK_IP_H
#define IOUXX_OPERATION_NETWORK_IP_H 1

#ifndef IOUXX_USE_CXX_MODULE

#include <netinet/in.h>
#include <sys/socket.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>
#include <bit>
#include <array>
#include <ranges>
#include <algorithm>
#include <string>
#include <string_view>
#include <expected>
#include <system_error>
#include <charconv>
#include <format>

#include "util/utility.hpp"
#include "socket.hpp"
#include "util/assertion.hpp"

#endif // IOUXX_USE_CXX_MODULE

IOUXX_EXPORT
namespace iouxx::inline iouops::network::ip {

    using v4raw = std::uint32_t; // network byte order
    using v6raw = std::array<std::uint16_t, 8>; // network byte order

    constexpr std::uint16_t hton_16(std::uint16_t host) noexcept {
        if constexpr (std::endian::native == std::endian::little) {
            return std::byteswap(host);
        } else {
            return host;
        }
    }

    constexpr v4raw hton(v4raw host) noexcept {
        if constexpr (std::endian::native == std::endian::little) {
            return std::byteswap(host);
        } else {
            return host;
        }
    }

    constexpr void hton_inplace(v4raw& host) noexcept {
        if constexpr (std::endian::native == std::endian::little) {
            host = std::byteswap(host);
        }
    }

    constexpr v6raw hton(const v6raw& host) noexcept {
        if constexpr (std::endian::native == std::endian::little) {
            v6raw net{};
            for (std::size_t i = 0; i < 8; ++i) {
                net[i] = std::byteswap(host[i]);
            }
            return net;
        } else {
            return host;
        }
    }

    constexpr void hton_inplace(v6raw& host) noexcept {
        if constexpr (std::endian::native == std::endian::little) {
            for (std::size_t i = 0; i < 8; ++i) {
                host[i] = std::byteswap(host[i]);
            }
        }
    }

    class address_v4
    {
    public:
        static constexpr socket_config::domain domain = socket_config::domain::ipv4;

        constexpr address_v4() = default;

        friend constexpr bool operator==(const address_v4&, const address_v4&) = default;

        // 127.0.0.1
        static consteval address_v4 loopback() noexcept {
            return address_v4(hton(0x7f000001));
        }

        // 0.0.0.0
        static consteval address_v4 any() noexcept {
            return address_v4(hton(0x00000000));
        }

        // 255.255.255.255
        static consteval address_v4 broadcast() noexcept {
            return address_v4(hton(0xffffffff));
        }

        constexpr explicit address_v4(v4raw net_order_raw) noexcept
            : addr(net_order_raw)
        {}

        constexpr v4raw raw() const noexcept { return addr; }

        // Warning: invalid input results in UNDEFINED BEHAVIOR
        static constexpr address_v4 from_string_uncheck(const std::string_view ipv4_str) noexcept {
            namespace stdr = std::ranges;
            namespace stdv = std::views;
            auto parts = ipv4_str | stdv::split('.');
            std::array<std::uint8_t, 4> part_results{};
            std::size_t count = 0;
            for (auto&& part : parts) {
                [[assume(count < 4)]];
                const std::string_view sub(stdr::data(part), stdr::size(part));
                auto& part_result = part_results[count];
                const auto first = sub.data();
                const auto last = sub.data() + sub.size();
                auto [ptr, ec] = std::from_chars(
                    first, last, part_result, 10);
                [[assume(ec == std::errc())]];
                [[assume(ptr == last)]];
                ++count;
            }
            return address_v4(std::bit_cast<v4raw>(part_results));
        }

        static constexpr auto from_string(const std::string_view ipv4_str)
            noexcept -> std::expected<address_v4, std::error_code> {
            // Valid address: d.d.d.d, where d is 0-255, without leading zeros
            if (ipv4_str.empty() || ipv4_str.size() > 15) {
                return utility::fail_invalid_argument();
            }
            namespace stdr = std::ranges;
            namespace stdv = std::views;
            auto parts = ipv4_str | stdv::split('.');
            std::array<std::uint8_t, 4> part_results{};
            std::size_t count = 0;
            for (auto&& part : parts) {
                if (count >= 4) {
                    return utility::fail_invalid_argument();
                }
                std::string_view sub(stdr::data(part), stdr::size(part));
                if (sub.empty() || sub.size() > 3) {
                    // Empty part or too long
                    return utility::fail_invalid_argument();
                }
                if (sub.size() > 1 && sub.front() == '0') {
                    // Leading zero
                    return utility::fail_invalid_argument();
                }
                auto& part_result = part_results[count];
                // std::from_chars will handle:
                // - empty string
                // - non-digit characters
                // - out of range (> 255)
                // - negative numbers (leading '-')
                // - spaces
                const auto first = sub.data();
                const auto last = sub.data() + sub.size();
                auto [ptr, ec] = std::from_chars(
                    first, last, part_result, 10);
                if (ec != std::errc()) {
                    // Conversion error
                    return utility::fail(ec);
                }
                if (ptr != last) {
                    // Not fully consumed
                    return utility::fail_invalid_argument();
                }
                ++count;
            }
            if (count != 4) {
                return utility::fail_invalid_argument();
            }
            return address_v4(std::bit_cast<v4raw>(part_results));
        }

        [[nodiscard]]
        constexpr std::string to_string() const;

    private:
        v4raw addr = 0; // network byte order
    };

    class address_v6
    {
    public:
        static constexpr socket_config::domain domain = socket_config::domain::ipv6;

        constexpr address_v6() = default;

        friend constexpr bool operator==(const address_v6&, const address_v6&) = default;

        // ::
        static consteval address_v6 any() noexcept {
            return address_v6();
        }

        // ::1
        static consteval address_v6 loopback() noexcept {
            v6raw loop{0,0,0,0,0,0,0,1};
            return address_v6(hton(loop));
        }

        constexpr explicit address_v6(const v6raw& net_order_raw) noexcept
            : addr(net_order_raw)
        {}

        constexpr const v6raw& raw() const noexcept { return addr; }

        // Warning: invalid input results in UNDEFINED BEHAVIOR
        static constexpr address_v6 from_string_uncheck(const std::string_view ipv6_str) noexcept {
            namespace stdr = std::ranges;
            namespace stdv = std::views;
            using namespace std::literals;
            v6raw part_results{};
            std::size_t part_count = 0;
            std::size_t double_colon_count = 0;
            bool seen_ipv4 = false;

            auto parse_one_section = [&](const std::string_view sub)
                constexpr noexcept {
                [[assume(part_count < 8)]];
                if (sub.contains('.')) {
                    // Use address_v4 to parse
                    address_v4 v4_raw = address_v4::from_string_uncheck(sub);
                    std::array<std::uint16_t, 2> v4_parts =
                        std::bit_cast<std::array<std::uint16_t, 2>>(v4_raw);
                    // Use local endianness for now
                    part_results[part_count++] = hton_16(v4_parts[0]);
                    part_results[part_count++] = hton_16(v4_parts[1]);
                    seen_ipv4 = true;
                } else {
                    // Hex part
                    const auto first = sub.data();
                    const auto last = sub.data() + sub.size();
                    auto [ptr, ec] = std::from_chars(
                        first, last, part_results[part_count], 0x10);
                    [[assume(ec == std::errc())]];
                    [[assume(ptr == last)]];
                    ++part_count;
                }
            };

            auto parts_by_double_colon = ipv6_str | stdv::split("::"sv);
            for (auto&& part : parts_by_double_colon) {
                std::string_view sub(stdr::data(part), stdr::size(part));
                ++double_colon_count;
                if (double_colon_count == 1) {
                    // First part (before "::")
                    if (sub.empty()) {
                        // Leading "::"
                        continue;
                    }
                    auto parts_by_colon = sub | stdv::split(':');
                    for (auto&& part : parts_by_colon) {
                        std::string_view sub(stdr::data(part), stdr::size(part));
                        parse_one_section(sub);
                    }
                } else if (double_colon_count == 2) {
                    // Second part (after "::")
                    if (sub.empty()) {
                        // Trailing "::"
                        continue;
                    }
                    const std::size_t first_part_count = part_count;
                    auto parts_by_colon = sub | stdv::split(':');
                    for (auto&& part : parts_by_colon) {
                        std::string_view sub(stdr::data(part), stdr::size(part));
                        parse_one_section(sub);
                    }
                    // Deal with consecutive zeros
                    stdr::rotate(part_results.begin() + first_part_count,
                        part_results.begin() + part_count,
                        part_results.begin() + 8);
                } else {
                    // More than one "::"
                    std::unreachable();
                }
            }
            hton_inplace(part_results);
            return address_v6(part_results);
        }

        static constexpr auto from_string(const std::string_view ipv6_str)
            noexcept -> std::expected<address_v6, std::error_code> {
            /*
                * There are three forms of IPv6 addresses:
                * 1. Full form: x:x:x:x:x:x:x:x (8 groups of 4 hex digits)
                * 2. Compressed form: x:x:x::x:x (one "::" to represent consecutive zeros)
                * 3. Mixed form: x:x:x:x:x:x:d.d.d.d (last 32 bits as IPv4)
                * and combinations thereof.
                * Hex digits may or may not have leading zeros.
            */
            if (ipv6_str.empty() || ipv6_str.size() > 45) {
                return utility::fail_invalid_argument();
            }
            namespace stdr = std::ranges;
            namespace stdv = std::views;
            using namespace std::literals;
            v6raw part_results{};
            std::size_t part_count = 0;
            std::size_t double_colon_count = 0;
            bool seen_ipv4 = false;

            auto parse_one_section = [&](const std::string_view sub)
                constexpr noexcept -> std::errc {
                if (sub.contains('.')) {
                    if (part_count > 6) {
                        // Not enough space for IPv4 (needs 2 parts)
                        return std::errc::invalid_argument;
                    }
                    // Use address_v4 to parse
                    auto v4 = address_v4::from_string(sub);
                    if (!v4) {
                        return std::errc::invalid_argument;
                    }
                    auto v4_raw = v4->raw();
                    std::array<std::uint16_t, 2> v4_parts =
                        std::bit_cast<std::array<std::uint16_t, 2>>(v4_raw);
                    // Use local endianness for now
                    part_results[part_count++] = hton_16(v4_parts[0]);
                    part_results[part_count++] = hton_16(v4_parts[1]);
                    seen_ipv4 = true;
                } else {
                    // Hex part
                    if (part_count >= 8) {
                        // Too many parts
                        return std::errc::invalid_argument;
                    }
                    if (sub.size() > 4) {
                        // Too long
                        return std::errc::invalid_argument;
                    }
                    const auto first = sub.data();
                    const auto last = sub.data() + sub.size();
                    auto [ptr, ec] = std::from_chars(
                        first, last, part_results[part_count], 0x10);
                    if (ec != std::errc()) {
                        // Conversion error
                        return ec;
                    }
                    if (ptr != last) {
                        // Not fully consumed
                        return std::errc::invalid_argument;
                    }
                    ++part_count;
                }
                return std::errc();
            };

            auto parts_by_double_colon = ipv6_str | stdv::split("::"sv);
            for (auto&& part : parts_by_double_colon) {
                std::string_view sub(stdr::data(part), stdr::size(part));
                ++double_colon_count;
                if (double_colon_count == 1) {
                    // First part (before "::")
                    if (sub.empty()) {
                        // Leading "::"
                        continue;
                    }
                    auto parts_by_colon = sub | stdv::split(':');
                    for (auto&& part : parts_by_colon) {
                        if (seen_ipv4) {
                            // IPv4 part must be at the end
                            return utility::fail_invalid_argument();
                        }
                        std::string_view sub(stdr::data(part), stdr::size(part));
                        if (sub.empty()) {
                            // Leading single colon
                            return utility::fail_invalid_argument();
                        }
                        auto ec = parse_one_section(sub);
                        if (ec != std::errc()) {
                            return utility::fail(ec);
                        }
                    }
                } else if (double_colon_count == 2) {
                    // Second part (after "::")
                    if (seen_ipv4) {
                        // IPv4 part must be at the end
                        return utility::fail_invalid_argument();
                    }
                    if (sub.empty()) {
                        // Trailing "::"
                        part_count = 8;
                        continue;
                    }
                    const std::size_t first_part_count = part_count;
                    auto parts_by_colon = sub | stdv::split(':');
                    for (auto&& part : parts_by_colon) {
                        if (seen_ipv4) {
                            // IPv4 part must be at the end
                            return utility::fail_invalid_argument();
                        }
                        std::string_view sub(stdr::data(part), stdr::size(part));
                        if (sub.empty()) {
                            // Pattern like ":::" or trailing single colon
                            return utility::fail_invalid_argument();
                        }
                        auto ec = parse_one_section(sub);
                        if (ec != std::errc()) {
                            return utility::fail(ec);
                        }
                    }
                    // Deal with consecutive zeros
                    if (part_count >= 8) {
                        // No space for zeros
                        return utility::fail_invalid_argument();
                    }
                    stdr::rotate(part_results.begin() + first_part_count,
                        part_results.begin() + part_count,
                        part_results.begin() + 8);
                    part_count = 8;
                } else {
                    // More than one "::"
                    return utility::fail_invalid_argument();
                }
            }
            if (part_count != 8) {
                // Not enough parts
                return utility::fail_invalid_argument();
            }
            hton_inplace(part_results);
            return address_v6(part_results);
        }

        [[nodiscard]]
        constexpr std::string to_string() const;

    private:
        v6raw addr{}; // network byte order
    };

    using portraw = std::uint16_t; // network byte order

    class port
    {
    public:
        constexpr port() = default;

        constexpr explicit port(portraw net_order_raw) noexcept
            : p(net_order_raw)
        {}

        constexpr portraw raw() const noexcept { return p; }

        friend constexpr bool operator==(const port&, const port&) = default;
        friend constexpr bool operator==(const port& lhs, portraw /* local */ rhs) noexcept {
            return lhs.p == hton_16(rhs);
        }

        static constexpr port from_string_uncheck(const std::string_view port_str) noexcept {
            portraw port_num = 0;
            const auto first = port_str.data();
            const auto last = port_str.data() + port_str.size();
            auto [ptr, ec] = std::from_chars(
                first, last, port_num, 10);
            [[assume(ec == std::errc())]];
            [[assume(ptr == last)]];
            return port(hton_16(port_num));
        }

        static constexpr auto from_string(const std::string_view port_str)
            noexcept -> std::expected<port, std::error_code> {
            if (port_str.size() > 1 && port_str.front() == '0') {
                // Leading zero
                return utility::fail_invalid_argument();
            }
            portraw port_num = 0;
            const auto first = port_str.data();
            const auto last = port_str.data() + port_str.size();
            auto [ptr, ec] = std::from_chars(
                first, last, port_num, 10);
            if (ec != std::errc()) {
                // Conversion error
                return utility::fail(ec);
            }
            if (ptr != last) {
                // Not fully consumed
                return utility::fail_invalid_argument();
            }
            return port(hton_16(port_num));
        }

        [[nodiscard]]
        constexpr std::string to_string() const;

    private:
        portraw p = 0; // network byte order
    };

    class socket_v4_info
    {
    public:
        static constexpr socket_config::domain domain = address_v4::domain;

        constexpr socket_v4_info() = default;

        constexpr socket_v4_info(const address_v4& addr, const port& p) noexcept
            : addr(addr), p(p)
        {}

        constexpr void address(const address_v4& addr) noexcept { this->addr = addr; }
        constexpr void port(const port& p) noexcept { this->p = p; }

        constexpr const address_v4& address() const noexcept { return addr; }
        constexpr const ip::port& port() const noexcept { return p; }

        constexpr ::sockaddr_in to_system_sockaddr() const noexcept {
            return {
                .sin_family = AF_INET,
                .sin_port = p.raw(),
                .sin_addr = std::bit_cast<::in_addr>(addr.raw()),
                .sin_zero = {}
            };
        }

        constexpr void from_system_sockaddr(
            const ::sockaddr* addr, const ::socklen_t* addrlen) noexcept {
            assert(*addrlen == sizeof(::sockaddr_in));
            ::sockaddr_in addr4;
            std::memcpy(&addr4, addr, sizeof(::sockaddr_in));
            *this = socket_v4_info(
                address_v4(std::bit_cast<v4raw>(addr4.sin_addr)),
                ip::port(addr4.sin_port)
            );
        }

        friend constexpr bool operator==(const socket_v4_info&, const socket_v4_info&) = default;

        // Warning: invalid input results in UNDEFINED BEHAVIOR
        static constexpr socket_v4_info from_string_uncheck(const std::string_view str) noexcept {
            namespace stdr = std::ranges;
            namespace stdv = std::views;
            char seperator = ':';
            if (str.contains('/')) {
                seperator = '/';
            }
            auto parts = str | stdv::split(seperator);
            std::size_t part_count = 0;
            address_v4 addr;
            ip::port p;
            for (auto&& part : parts) {
                ++part_count;
                std::string_view sub(stdr::data(part), stdr::size(part));
                if (part_count == 1) {
                    addr = address_v4::from_string_uncheck(sub);
                } else {
                    p = ip::port::from_string_uncheck(sub);
                }
            }
            return socket_v4_info(addr, p);
        }

        static constexpr auto from_string(const std::string_view str)
            noexcept -> std::expected<socket_v4_info, std::error_code> {
            // ddd.ddd.ddd.ddd:ppppp or ddd.ddd.ddd.ddd/ppppp
            namespace stdr = std::ranges;
            namespace stdv = std::views;
            char seperator = '\0';
            if (str.contains(':')) {
                seperator = ':';
            } else if (str.contains('/')) {
                seperator = '/';
            } else {
                return utility::fail_invalid_argument();
            }
            auto parts = str | stdv::split(seperator);
            std::size_t part_count = 0;
            address_v4 addr;
            ip::port p;
            for (auto&& part : parts) {
                ++part_count;
                if (part_count > 2) {
                    return utility::fail_invalid_argument();
                }
                std::string_view sub(stdr::data(part), stdr::size(part));
                if (part_count == 1) {
                    auto addr_res = address_v4::from_string(sub);
                    if (!addr_res) {
                        return std::unexpected(addr_res.error());
                    }
                    addr = *addr_res;
                } else {
                    auto port_res = ip::port::from_string(sub);
                    if (!port_res) {
                        return std::unexpected(port_res.error());
                    }
                    p = *port_res;
                }
            }
            if (part_count != 2) {
                return utility::fail_invalid_argument();
            }
            return socket_v4_info(addr, p);
        }

        [[nodiscard]]
        constexpr std::string to_string() const;

    private:
        address_v4 addr;
        ip::port p;
    };

    class socket_v6_info
    {
    public:
        static constexpr socket_config::domain domain = address_v6::domain;

        constexpr socket_v6_info() = default;

        constexpr socket_v6_info(const address_v6& addr, const port& p) noexcept
            : addr(addr), p(p)
        {}

        constexpr void address(const address_v6& a) noexcept { addr = a; }
        constexpr void port(const port& prt) noexcept { p = prt; }

        constexpr const address_v6& address() const noexcept { return addr; }
        constexpr const ip::port& port() const noexcept { return p; }

        constexpr ::sockaddr_in6 to_system_sockaddr() const noexcept {
            return {
                .sin6_family = AF_INET6,
                .sin6_port = p.raw(),
                .sin6_flowinfo = 0,
                .sin6_addr = std::bit_cast<in6_addr>(addr.raw()),
                .sin6_scope_id = 0
            };
        }

        constexpr void from_system_sockaddr(
            const ::sockaddr* addr, const ::socklen_t* addrlen) noexcept {
            assert(*addrlen == sizeof(::sockaddr_in6));
            ::sockaddr_in6 addr6;
            std::memcpy(&addr6, addr, sizeof(::sockaddr_in6));
            *this = socket_v6_info(
                address_v6(std::bit_cast<v6raw>(addr6.sin6_addr)),
                ip::port(addr6.sin6_port)
            );
        }

        friend constexpr bool operator==(const socket_v6_info&, const socket_v6_info&) = default;

        // Warning: invalid input results in UNDEFINED BEHAVIOR
        static constexpr socket_v6_info from_string_uncheck(const std::string_view str) noexcept {
            namespace stdr = std::ranges;
            namespace stdv = std::views;
            using namespace std::literals;
            auto parts = str | stdv::split("]:"sv);
            std::size_t part_count = 0;
            address_v6 addr;
            ip::port p;
            for (auto&& part : parts) {
                ++part_count;
                std::string_view sub(stdr::data(part), stdr::size(part));
                if (part_count == 1) {
                    sub.remove_prefix(1); // remove leading '['
                    addr = address_v6::from_string_uncheck(sub);
                } else {
                    p = ip::port::from_string_uncheck(sub);
                }
            }
            return socket_v6_info(addr, p);
        }

        static constexpr auto from_string(const std::string_view str)
            noexcept -> std::expected<socket_v6_info, std::error_code> {
            // Only recognize RFC 5952 recommended form
            // [ipv6]:port
            namespace stdr = std::ranges;
            namespace stdv = std::views;
            using namespace std::literals;
            if (!str.contains("]:"sv) || str.front() != '[') {
                return utility::fail_invalid_argument();
            }
            auto parts = str | stdv::split("]:"sv);
            std::size_t part_count = 0;
            address_v6 addr;
            ip::port p;
            for (auto&& part : parts) {
                ++part_count;
                if (part_count > 2) {
                    return utility::fail_invalid_argument();
                }
                std::string_view sub(stdr::data(part), stdr::size(part));
                if (part_count == 1) {
                    sub.remove_prefix(1); // remove leading '['
                    auto addr_res = address_v6::from_string(sub);
                    if (!addr_res) {
                        return std::unexpected(addr_res.error());
                    }
                    addr = *addr_res;
                } else {
                    auto port_res = ip::port::from_string(sub);
                    if (!port_res) {
                        return std::unexpected(port_res.error());
                    }
                    p = *port_res;
                }
            }
            if (part_count != 2) {
                return utility::fail_invalid_argument();
            }
            return socket_v6_info(addr, p);
        }

        [[nodiscard]]
        constexpr std::string to_string() const;

    private:
        address_v6 addr;
        ip::port p;
    };

    template<typename T>
    consteval socket_config::domain get_domain() noexcept {
        return std::remove_cvref_t<T>::domain;
    }

} // namespace iouxx::iouops::network::ip

IOUXX_EXPORT
namespace iouxx::literals::inline network_literals {

    consteval network::ip::address_v4 operator""_ipv4(const char* str, std::size_t) noexcept {
        return network::ip::address_v4::from_string(str).value();
    }

    consteval network::ip::address_v6 operator""_ipv6(const char* str, std::size_t) noexcept {
        return network::ip::address_v6::from_string(str).value();
    }

    consteval network::ip::socket_v4_info operator""_sockv4(const char* str, std::size_t) noexcept {
        return network::ip::socket_v4_info::from_string(str).value();
    }

    consteval network::ip::socket_v6_info operator""_sockv6(const char* str, std::size_t) noexcept {
        return network::ip::socket_v6_info::from_string(str).value();
    }

} // namespace iouxx::literals::network_literals

// std::formatter specializations for IPv4 / IPv6 addresses
IOUXX_EXPORT
namespace std {

    template<>
    struct formatter<iouxx::network::ip::address_v4, char> {
        using ipv4 = iouxx::network::ip::address_v4;

        // No format spec supported; ensure it's either empty or '}' reached
        constexpr auto parse(format_parse_context& ctx) -> format_parse_context::iterator {
            auto it = ctx.begin();
            auto end = ctx.end();
            if (it != end && *it != '}') {
                throw format_error("invalid format spec for ipv4 address");
            }
            return it;
        }

        template<class FormatContext>
        constexpr auto format(const ipv4& addr, FormatContext& ctx) const {
            const std::array<std::uint8_t, 4> parts =
                std::bit_cast<std::array<std::uint8_t, 4>>(addr);
            return std::format_to(ctx.out(), "{}.{}.{}.{}",
                parts[0], parts[1], parts[2], parts[3]);
        }
    };

    // IPv6: accept optional spec describing representation
    // Spec characters:
    //  r | R (recommended, default) :
    //   - RFC 5952 recommended form, which is:
    //   - compressed
    //   - remove leading zeros
    //   - use lowercase hex digits
    //   - and special rule for embedded IPv4, which is applied when:
    //     - it is IPv4-mapped (::ffff:d.d.d.d) or IPv4-compatible (::d.d.d.d)
    //     - and it is not that all but last 16 bits are zeros (not ::abcd)
    //   (This special rule can not be achieved by other format spec combinations.)
    //  This character can only combine with 'n', 'N', 'u', 'U'.
    //   
    //  When useing customized format, default is:
    //   - compressed, leading zeros removed, no mixed, lowercase hex digits.
    //  Other characters (can be combined):
    //  f | F (full) : full form, no compression
    //  z | Z (keep leading zeros) : do not remove leading zeros
    //  m | M (mixed) : force mixed form with embedded IPv4 address
    //  n | N (no mixed) : do not use mixed form
    //   - the way to get default customized format (by {:n} or {:N})
    //   - can combine with 'r' or 'R' to disable the special rule
    //   - can not combine with 'm' or 'M'
    //  u | U (uppercase) : use uppercase hex digits
    template<>
    struct formatter<iouxx::network::ip::address_v6, char> {
        using ipv4 = iouxx::network::ip::address_v4;
        using ipv6 = iouxx::network::ip::address_v6;
        bool seen_r = false;
        bool seen_f = false;
        bool seen_z = false;
        bool seen_m = false;
        bool seen_n = false;
        bool seen_u = false;

        constexpr auto parse(format_parse_context& ctx) -> format_parse_context::iterator {
            auto it = ctx.begin();
            auto end = ctx.end();
            if (it == end || *it == '}') {
                // empty spec, use recommended form
                seen_r = true; // default
                return it;
            }
            for (; it != end && *it != '}'; ++it) {
                switch (*it) {
                case 'r': case 'R':
                    if (seen_r) {
                        throw format_error("duplicate 'r' or 'R' in ipv6 format spec");
                    }
                    seen_r = true;
                    break;
                case 'f': case 'F':
                    if (seen_f) {
                        throw format_error("duplicate 'f' or 'F' in ipv6 format spec");
                    }
                    seen_f = true;
                    break;
                case 'z': case 'Z':
                    if (seen_z) {
                        throw format_error("duplicate 'z' or 'Z' in ipv6 format spec");
                    }
                    seen_z = true;
                    break;
                case 'm': case 'M':
                    if (seen_m) {
                        throw format_error("duplicate 'm' or 'M' in ipv6 format spec");
                    }
                    seen_m = true;
                    break;
                case 'n': case 'N':
                    if (seen_n) {
                        throw format_error("duplicate 'n' or 'N' in ipv6 format spec");
                    }
                    seen_n = true;
                    break;
                case 'u': case 'U':
                    if (seen_u) {
                        throw format_error("duplicate 'u' or 'U' in ipv6 format spec");
                    }
                    seen_u = true;
                    break;
                default:
                    throw format_error("invalid character in ipv6 format spec");
                }
            }
            // Validate combinations
            if (seen_r) {
                if (seen_f) {
                    throw format_error("cannot combine 'r' and 'f' in ipv6 format spec");
                }
                if (seen_z) {
                    throw format_error("cannot combine 'r' and 'z' in ipv6 format spec");
                }
                if (seen_m) {
                    throw format_error("cannot combine 'r' and 'm' in ipv6 format spec");
                }
            } else {
                if (seen_m && seen_n) {
                    throw format_error("cannot combine 'm' and 'n' in ipv6 format spec");
                }
            }
            return it;
        }

        template<class FormatContext>
        constexpr auto format(const ipv6& addr, FormatContext& ctx) const {
            using namespace iouxx::network::ip;
            auto out = ctx.out();
            const v6raw local = hton(addr.raw());
            const bool recommended = seen_r;
            const bool full = seen_f;
            const bool removed = !seen_z;
            const bool uppercase = seen_u;
            const bool mixed = recommended
                ? !seen_n && check_mixed(local)
                : seen_m;
            return do_format(out, addr, local, recommended, full, removed, mixed, uppercase);
        }

        static constexpr bool check_mixed(const iouxx::network::ip::v6raw& local) noexcept {
            // Examine if it is IPv4-compatible or IPv4-mapped
            // IPv4-compatible: ::d.d.d.d
            // IPv4-mapped: ::ffff:d.d.d.d
            // If all but last 16 bits are zeros, it will not be considered as mixed
            for (std::size_t i = 0; i < 5; ++i) {
                if (local[i] != 0) {
                    return false;
                }
            }
            if (local[5] != 0xffff && (local[5] != 0 || local[6] == 0)) {
                return false;
            }
            return true;
        }

        template<typename OutIt>
        static constexpr OutIt do_format(OutIt out,
            const ipv6& addr,
            const iouxx::network::ip::v6raw& local,
            bool recommended, bool full, bool removed,
            bool mixed, bool uppercase) {
            if (full) {
                if (mixed) {
                    out = mixed_full_ipv6(out, local, removed, uppercase);
                    return ipv4_part(out, addr);
                } else {
                    return full_ipv6(out, local, removed, uppercase);
                }
            } else {
                out = compressed_ipv6(out, local, removed, mixed, uppercase);
                if (mixed) {
                    return ipv4_part(out, addr);
                } else {
                    return out;
                }
            }
        }

        template<typename OutIt>
        static constexpr OutIt full_ipv6(OutIt out,
            const iouxx::network::ip::v6raw& local,
            bool removed, bool uppercase) {
            if (!uppercase) {
                if (!removed) {
                    // full, leading zeros kept
                    return std::format_to(out,
                        "{:04x}:{:04x}:{:04x}:{:04x}:{:04x}:{:04x}:{:04x}:{:04x}",
                        local[0], local[1], local[2], local[3],
                        local[4], local[5], local[6], local[7]);
                } else {
                    // full, leading zeros removed
                    return std::format_to(out,
                        "{:x}:{:x}:{:x}:{:x}:{:x}:{:x}:{:x}:{:x}",
                        local[0], local[1], local[2], local[3],
                        local[4], local[5], local[6], local[7]);
                }
            } else {
                if (!removed) {
                    // full, leading zeros kept, uppercase
                    return std::format_to(out,
                        "{:04X}:{:04X}:{:04X}:{:04X}:{:04X}:{:04X}:{:04X}:{:04X}",
                        local[0], local[1], local[2], local[3],
                        local[4], local[5], local[6], local[7]);
                } else {
                    // full, leading zeros removed, uppercase
                    return std::format_to(out,
                        "{:X}:{:X}:{:X}:{:X}:{:X}:{:X}:{:X}:{:X}",
                        local[0], local[1], local[2], local[3],
                        local[4], local[5], local[6], local[7]);
                }
            }
        }

        template<typename OutIt>
        static constexpr OutIt mixed_full_ipv6(OutIt out,
            const iouxx::network::ip::v6raw& local,
            bool removed, bool uppercase) {
            if (!uppercase) {
                if (!removed) {
                    // leading zeros kept
                    return std::format_to(out,
                        "{:04x}:{:04x}:{:04x}:{:04x}:{:04x}:{:04x}:",
                        local[0], local[1], local[2], local[3], local[4], local[5]);
                } else {
                    // leading zeros removed
                    return std::format_to(out,
                        "{:x}:{:x}:{:x}:{:x}:{:x}:{:x}:",
                        local[0], local[1], local[2], local[3], local[4], local[5]);
                }
            } else {
                if (!removed) {
                    // leading zeros kept
                    return std::format_to(out,
                        "{:04X}:{:04X}:{:04X}:{:04X}:{:04X}:{:04X}:",
                        local[0], local[1], local[2], local[3], local[4], local[5]);
                } else {
                    // leading zeros removed
                    return std::format_to(out,
                        "{:X}:{:X}:{:X}:{:X}:{:X}:{:X}:",
                        local[0], local[1], local[2], local[3], local[4], local[5]);
                }
            }
        }

        template<typename OutIt>
        static constexpr OutIt compressed_ipv6(OutIt out,
            const iouxx::network::ip::v6raw& local,
            bool removed, bool mixed, bool uppercase) {
            // Find the longest consecutive zeros
            std::size_t best_start = 0;
            std::size_t best_len = 0;
            std::size_t cur_start = 0;
            std::size_t cur_len = 0;
            const std::size_t limit = mixed ? 6 : 8;
            for (std::size_t i = 0; i < limit; ++i) {
                if (local[i] == 0) {
                    if (cur_len == 0) {
                        cur_start = i;
                    }
                    ++cur_len;
                    if (cur_len > best_len) {
                        best_len = cur_len;
                        best_start = cur_start;
                    }
                } else {
                    cur_len = 0;
                }
            }
            if (best_len < 2) {
                // No compression
                if (mixed) {
                    return mixed_full_ipv6(out, local, removed, uppercase);
                } else {
                    return full_ipv6(out, local, removed, uppercase);
                }
            }
            if (best_start == 0 && best_len == limit) {
                // All zeros
                *out++ = ':';
                *out++ = ':';
                return out;
            }
            // Before compression
            for (std::size_t i = 0; i < best_start; ++i) {
                if (!uppercase) {
                    if (removed) {
                        out = std::format_to(out, "{:x}:", local[i]);
                    } else {
                        out = std::format_to(out, "{:04x}:", local[i]);
                    }
                } else {
                    if (removed) {
                        out = std::format_to(out, "{:X}:", local[i]);
                    } else {
                        out = std::format_to(out, "{:04X}:", local[i]);
                    }
                }
            }
            if (best_start == 0) {
                // Leading "::"
                *out++ = ':';
            }
            // After compression
            for (std::size_t i = best_start + best_len; i < limit; ++i) {
                if (!uppercase) {
                    if (removed) {
                        out = std::format_to(out, ":{:x}", local[i]);
                    } else {
                        out = std::format_to(out, ":{:04x}", local[i]);
                    }
                } else {
                    if (removed) {
                        out = std::format_to(out, ":{:X}", local[i]);
                    } else {
                        out = std::format_to(out, ":{:04X}", local[i]);
                    }
                }
            }
            if (best_start + best_len == limit || mixed) {
                // Trailing "::" or mixed (add colon after last hex group)
                *out++ = ':';
            }
            return out;
        }

        template<typename OutIt>
        static constexpr OutIt ipv4_part(OutIt out, const ipv6& addr) {
            const auto& raw = addr.raw();
            std::array<std::uint16_t, 2> raw_v4_part = { raw[6], raw[7] };
            using v4raw = iouxx::network::ip::v4raw;
            const ipv4 v4_part(std::bit_cast<v4raw>(raw_v4_part));
            return std::format_to(out, "{}", v4_part);
        }
    };

    template<>
    struct formatter<iouxx::network::ip::port, char> {
        using port = iouxx::network::ip::port;

        // No format spec supported; ensure it's either empty or '}' reached
        constexpr auto parse(format_parse_context& ctx) -> format_parse_context::iterator {
            auto it = ctx.begin();
            auto end = ctx.end();
            if (it != end && *it != '}') {
                throw format_error("invalid format spec for port");
            }
            return it;
        }

        template<class FormatContext>
        constexpr auto format(const port& p, FormatContext& ctx) const {
            return std::format_to(ctx.out(), "{}", iouxx::network::ip::hton_16(p.raw()));
        }
    };

    template<>
    struct formatter<iouxx::network::ip::socket_v4_info, char> {
        using socketv4 = iouxx::network::ip::socket_v4_info;
        char seperator = ':'; // default

        // Requires empty spec or single '/'
        constexpr auto parse(format_parse_context& ctx) -> format_parse_context::iterator {
            auto it = ctx.begin();
            auto end = ctx.end();
            if (it == end || *it == '}') {
                // empty spec, use ':'
                return it;
            }
            if (*it == '/') {
                seperator = '/';
                ++it;
            }
            if (it != end && *it != '}') {
                throw format_error("invalid format spec for socketv4");
            }
            return it;
        }

        template<class FormatContext>
        constexpr auto format(const socketv4& s, FormatContext& ctx) const {
            if (seperator == ':') {
                return std::format_to(ctx.out(), "{}:{}", s.address(), s.port());
            } else {
                return std::format_to(ctx.out(), "{}/{}", s.address(), s.port());
            }
        }
    };

    template<>
    struct formatter<iouxx::network::ip::socket_v6_info, char> {
        using socketv6 = iouxx::network::ip::socket_v6_info;

        // No format spec supported; ensure it's either empty or '}' reached
        constexpr auto parse(format_parse_context& ctx) -> format_parse_context::iterator {
            auto it = ctx.begin();
            auto end = ctx.end();
            if (it != end && *it != '}') {
                throw format_error("invalid format spec for socketv6");
            }
            return it;
        }

        template<class FormatContext>
        constexpr auto format(const socketv6& s, FormatContext& ctx) const {
            // Always use RFC 5952 recommended form
            return std::format_to(ctx.out(), "[{}]:{}", s.address(), s.port());
        }
    };

} // namespace std

IOUXX_EXPORT
namespace iouxx::inline iouops::network::ip {
    
    [[nodiscard]]
    constexpr std::string address_v4::to_string() const {
        return std::format("{}", *this);
    }

    // Use RFC 5952 recommended form for IPv6 to_string()
    [[nodiscard]]
    constexpr std::string address_v6::to_string() const {
        return std::format("{}", *this);
    }

    [[nodiscard]]
    constexpr std::string port::to_string() const {
        return std::format("{}", *this);
    }

    [[nodiscard]]
    constexpr std::string socket_v4_info::to_string() const {
        return std::format("{}", *this);
    }

    // Use RFC 5952 recommended form for IPv6 socket to_string()
    [[nodiscard]]
    constexpr std::string socket_v6_info::to_string() const {
        return std::format("{}", *this);
    }

} // iouxx::iouops::network::ip

#endif // IOUXX_OPERATION_NETWORK_IP_H
