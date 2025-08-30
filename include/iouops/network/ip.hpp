#pragma once
#ifndef IOUXX_OPERATION_NETWORK_IP_H
#define IOUXX_OPERATION_NETWORK_IP_H 1

#include <cstddef>
#include <cstdint>
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

namespace iouxx::details {

    inline std::error_code make_invalid_argument_error() noexcept {
        return std::make_error_code(std::errc::invalid_argument);
    }

    inline constexpr std::uint16_t hton_16(std::uint16_t host) noexcept {
        if constexpr (std::endian::native == std::endian::little) {
            return std::byteswap(host);
        } else {
            return host;
        }
    }

} // iouxx::details

namespace iouxx::inline iouops::network::ip {

    using v4raw = std::uint32_t; // network byte order
    using v6raw = std::array<std::uint16_t, 8>; // network byte order

    inline constexpr v4raw hton(v4raw host) noexcept {
        if constexpr (std::endian::native == std::endian::little) {
            return std::byteswap(host);
        } else {
            return host;
        }
    }

    inline constexpr void hton_inplace(v4raw& host) noexcept {
        if constexpr (std::endian::native == std::endian::little) {
            host = std::byteswap(host);
        }
    }

    inline constexpr v6raw hton(const v6raw& host) noexcept {
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

    inline constexpr void hton_inplace(v6raw& host) noexcept {
        if constexpr (std::endian::native == std::endian::little) {
            for (std::size_t i = 0; i < 8; ++i) {
                host[i] = std::byteswap(host[i]);
            }
        }
    }

    class address_v4
    {
    public:
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

        v4raw raw() const noexcept { return addr; }

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
                return std::unexpected(details::make_invalid_argument_error());
            }
            namespace stdr = std::ranges;
            namespace stdv = std::views;
            auto parts = ipv4_str | stdv::split('.');
            std::array<std::uint8_t, 4> part_results{};
            std::size_t count = 0;
            for (auto&& part : parts) {
                if (count >= 4) {
                    return std::unexpected(details::make_invalid_argument_error());
                }
                std::string_view sub(stdr::data(part), stdr::size(part));
                if (sub.empty() || sub.size() > 3) {
                    // Empty part or too long
                    return std::unexpected(details::make_invalid_argument_error());
                }
                if (sub[0] == '0' && sub.size() > 1) {
                    // Leading zero
                    return std::unexpected(details::make_invalid_argument_error());
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
                    return std::unexpected(std::make_error_code(ec));
                }
                if (ptr != last) {
                    // Not fully consumed
                    return std::unexpected(details::make_invalid_argument_error());
                }
                ++count;
            }
            if (count != 4) {
                return std::unexpected(details::make_invalid_argument_error());
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

        const v6raw& raw() const noexcept { return addr; }

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
                    part_results[part_count++] = details::hton_16(v4_parts[0]);
                    part_results[part_count++] = details::hton_16(v4_parts[1]);
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
                return std::unexpected(details::make_invalid_argument_error());
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
                    part_results[part_count++] = details::hton_16(v4_parts[0]);
                    part_results[part_count++] = details::hton_16(v4_parts[1]);
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
                            return std::unexpected(details::make_invalid_argument_error());
                        }
                        std::string_view sub(stdr::data(part), stdr::size(part));
                        if (sub.empty()) {
                            // Leading single colon
                            return std::unexpected(details::make_invalid_argument_error());
                        }
                        auto ec = parse_one_section(sub);
                        if (ec != std::errc()) {
                            return std::unexpected(std::make_error_code(ec));
                        }
                    }
                } else if (double_colon_count == 2) {
                    // Second part (after "::")
                    if (seen_ipv4) {
                        // IPv4 part must be at the end
                        return std::unexpected(details::make_invalid_argument_error());
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
                            return std::unexpected(details::make_invalid_argument_error());
                        }
                        std::string_view sub(stdr::data(part), stdr::size(part));
                        if (sub.empty()) {
                            // Pattern like ":::" or trailing single colon
                            return std::unexpected(details::make_invalid_argument_error());
                        }
                        auto ec = parse_one_section(sub);
                        if (ec != std::errc()) {
                            return std::unexpected(std::make_error_code(ec));
                        }
                    }
                    // Deal with consecutive zeros
                    if (part_count >= 8) {
                        // No space for zeros
                        return std::unexpected(details::make_invalid_argument_error());
                    }
                    stdr::rotate(part_results.begin() + first_part_count,
                        part_results.begin() + part_count,
                        part_results.begin() + 8);
                    part_count = 8;
                } else {
                    // More than one "::"
                    return std::unexpected(details::make_invalid_argument_error());
                }
            }
            if (part_count != 8) {
                // Not enough parts
                return std::unexpected(details::make_invalid_argument_error());
            }
            hton_inplace(part_results);
            return address_v6(part_results);
        }

        [[nodiscard]]
        constexpr std::string to_string() const;

    private:
        v6raw addr{}; // network byte order
    };

} // namespace iouxx::iouops::network::ip

namespace iouxx::literals::inline network_literals {

    consteval network::ip::address_v4 operator""_ipv4(const char* str, std::size_t) noexcept {
        return network::ip::address_v4::from_string(str).value();
    }

    consteval network::ip::address_v6 operator""_ipv6(const char* str, std::size_t) noexcept {
        return network::ip::address_v6::from_string(str).value();
    }

} // namespace iouxx::literals::network_literals

// std::formatter specializations for IPv4 / IPv6 addresses
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
    //   - and special rule for embedded IPv4, only when it is IPv4-compatible or IPv4-mapped
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
    //   - default, but can combine with 'r' or 'R' to disable the special rule
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
            bool is_ipv4_compatible = true;
            bool is_ipv4_mapped = true;
            // First 5 parts must be zero
            for (std::size_t i = 0; i < 5; ++i) {
                if (local[i] != 0) {
                    is_ipv4_compatible = false;
                    is_ipv4_mapped = false;
                    break;
                }
            }
            if (is_ipv4_compatible) {
                if (local[5] != 0) {
                    is_ipv4_compatible = false;
                } else if (local[6] == 0) {
                    // All but last 16 bits are zeros, not considered as mixed
                    // Looks like '::abcd'
                    return false;
                }
            }
            if (is_ipv4_mapped) {
                if (local[5] != 0xffff) {
                    is_ipv4_mapped = false;
                }
            }
            return is_ipv4_compatible || is_ipv4_mapped;
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

} // namespace std

[[nodiscard]]
inline constexpr std::string iouxx::network::ip::address_v4::to_string() const {
    std::string result;
    result.resize_and_overwrite(16, [this](char* data, std::size_t) {
        std::formatter<address_v4> formatter;
        struct fake_context {
            char* out_ptr;
            char* out() const noexcept { return out_ptr; }
        } ctx(data);
        return formatter.format(*this, ctx) - data;
    });
    return result;
}

// Use RFC 5952 recommended form for IPv6 to_string()
[[nodiscard]]
inline constexpr std::string iouxx::network::ip::address_v6::to_string() const {
    std::string result;
    result.resize_and_overwrite(64, [this](char* data, std::size_t) {
        std::formatter<address_v6> formatter;
        formatter.seen_r = true; // recommended form
        struct fake_context {
            char* out_ptr;
            char* out() const noexcept { return out_ptr; }
        } ctx(data);
        return formatter.format(*this, ctx) - data;
    });
    return result;
}

#endif // IOUXX_OPERATION_NETWORK_IP_H
