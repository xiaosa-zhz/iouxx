#include <source_location>
#include <string_view>
#include <print>

#include <arpa/inet.h>

#include "iouops/network/ip.hpp"

#define TEST_EXPECT(...) do { \
    if (!(__VA_ARGS__)) { \
        std::println("Assertion failed: {}, {}:{}\n", #__VA_ARGS__, \
        loc.file_name(), loc.line()); \
        std::exit(-(__COUNTER__ + 1)); \
    } \
} while(0)

void ip_parse_test() {
    using namespace iouxx::iouops::network::ip;

    // Test ipv4 parsing
    std::println("Starting ipv4 tests...");
    // special addresses
    auto special = [](std::string_view s, address_v4 expected,
        std::source_location loc = std::source_location::current()) {
        auto r = address_v4::from_string(s);
        TEST_EXPECT(r.has_value());
        TEST_EXPECT(r == expected);
        auto ru = address_v4::from_string_uncheck(s);
        TEST_EXPECT(ru == expected);
        TEST_EXPECT(r->to_string() == s);
    };
    // loopback 127.0.0.1
    special("127.0.0.1", address_v4::loopback());
    // any 0.0.0.0
    special("0.0.0.0", address_v4::any());
    // broadcast 255.255.255.255
    special("255.255.255.255", address_v4::broadcast());

    // roundtrip tests
    auto roundtrip = [](std::string_view s,
        std::source_location loc = std::source_location::current()) {
        auto r = address_v4::from_string(s);
        TEST_EXPECT(r.has_value());
        std::string back = r->to_string();
        auto r2 = address_v4::from_string_uncheck(back);
        TEST_EXPECT(r == r2);
    };
    roundtrip("1.2.3.4");
    roundtrip("192.168.0.1");
    roundtrip("10.0.0.42");

    // invalid cases
    auto invalid = [](std::string_view s,
        std::source_location loc = std::source_location::current()) {
        auto r = address_v4::from_string(s);
        TEST_EXPECT(!r.has_value());
    };
    invalid("");
    invalid(" ");
    invalid("1");
    invalid("1.2.3");
    invalid("1.2.3.4.5");
    invalid("256.0.0.1");
    invalid("+1.0.0.0");
    invalid("127.0.0.+1");
    invalid("-1.0.0.0");
    invalid("127.0.0.-1");
    invalid("127.0.1-.1");
    invalid("127.0.1+.1");
    invalid("127.0.1e2.1");
    invalid(".1.2.3");
    invalid("1..2.3");
    invalid("1.2.3.");
    invalid("abc.def.ghi.jkl");
    invalid("01a.2.3.4");
    invalid("1.2.3.4 ");
    invalid(" 1.2.3.4");
    invalid("1.2. 3 .4");

    // ipv4 literals
    {
        std::source_location loc = std::source_location::current();
        using namespace iouxx::literals::network_literals;
        constexpr address_v4 a1 = "127.0.0.1"_ipv4;
        TEST_EXPECT(a1 == address_v4::loopback());
    }

    std::println("All ipv4 tests passed.");

    // Test ipv6 parsing
    std::println("Starting ipv6 tests...");
    // special addresses
    auto special6 = [](std::string_view s, address_v6 expected,
        std::source_location loc = std::source_location::current()) {
        auto r = address_v6::from_string(s);
        TEST_EXPECT(r.has_value());
        TEST_EXPECT(r == expected);
        auto back = r->to_string();
        auto ru = address_v6::from_string_uncheck(back);
        TEST_EXPECT(ru == expected);
    };
    // loopback ::1
    special6("::1", address_v6::loopback());
    special6("0:0:0:0:0:0:0:1", address_v6::loopback());
    // any ::
    special6("::", address_v6::any());
    special6("0:0:0:0:0:0:0:0", address_v6::any());

    // roundtrip tests for ipv6
    auto roundtrip6 = [](std::string_view s,
        std::source_location loc = std::source_location::current()) {
        auto r0 = address_v6::from_string(s);
        TEST_EXPECT(r0.has_value());
        // Single format
        std::string back1 = std::format("{}", *r0); // same as {:r}
        auto r1 = address_v6::from_string_uncheck(back1);
        TEST_EXPECT(r0 == r1);
        std::string back2 = std::format("{:r}", *r0);
        auto r2 = address_v6::from_string_uncheck(back2);
        TEST_EXPECT(r0 == r2);
        std::string back3 = std::format("{:f}", *r0);
        auto r3 = address_v6::from_string_uncheck(back3);
        TEST_EXPECT(r0 == r3);
        std::string back4 = std::format("{:z}", *r0);
        auto r4 = address_v6::from_string_uncheck(back4);
        TEST_EXPECT(r0 == r4);
        std::string back5 = std::format("{:m}", *r0);
        auto r5 = address_v6::from_string_uncheck(back5);
        TEST_EXPECT(r0 == r5);
        std::string back6 = std::format("{:n}", *r0);
        auto r6 = address_v6::from_string_uncheck(back6);
        TEST_EXPECT(r0 == r6);
        std::string back7 = std::format("{:u}", *r0);
        auto r7 = address_v6::from_string_uncheck(back7);
        TEST_EXPECT(r0 == r7);
        // Combined formats of 'r'
        std::string back8 = std::format("{:rn}", *r0);
        auto r8 = address_v6::from_string_uncheck(back8);
        TEST_EXPECT(r0 == r8);
        std::string back9 = std::format("{:ru}", *r0);
        auto r9 = address_v6::from_string_uncheck(back9);
        TEST_EXPECT(r0 == r9);
        std::string back10 = std::format("{:rnu}", *r0);
        auto r10 = address_v6::from_string_uncheck(back10);
        TEST_EXPECT(r0 == r10);
        // Other combined formats (of 'f', 'z', 'm', 'u')
        // fz fm fu zm zu mu
        // fzm fzu fmu zmu
        // fzmu
        std::string back11 = std::format("{:fz}", *r0);
        auto r11 = address_v6::from_string_uncheck(back11);
        TEST_EXPECT(r0 == r11);
        std::string back12 = std::format("{:fm}", *r0);
        auto r12 = address_v6::from_string_uncheck(back12);
        TEST_EXPECT(r0 == r12);
        std::string back13 = std::format("{:fu}", *r0);
        auto r13 = address_v6::from_string_uncheck(back13);
        TEST_EXPECT(r0 == r13);
        std::string back14 = std::format("{:zm}", *r0);
        auto r14 = address_v6::from_string_uncheck(back14);
        TEST_EXPECT(r0 == r14);
        std::string back15 = std::format("{:zu}", *r0);
        auto r15 = address_v6::from_string_uncheck(back15);
        TEST_EXPECT(r0 == r15);
        std::string back16 = std::format("{:mu}", *r0);
        auto r16 = address_v6::from_string_uncheck(back16);
        TEST_EXPECT(r0 == r16);
        std::string back17 = std::format("{:fzm}", *r0);
        auto r17 = address_v6::from_string_uncheck(back17);
        TEST_EXPECT(r0 == r17);
        std::string back18 = std::format("{:fzu}", *r0);
        auto r18 = address_v6::from_string_uncheck(back18);
        TEST_EXPECT(r0 == r18);
        std::string back19 = std::format("{:fmu}", *r0);
        auto r19 = address_v6::from_string_uncheck(back19);
        TEST_EXPECT(r0 == r19);
        std::string back20 = std::format("{:zmu}", *r0);
        auto r20 = address_v6::from_string_uncheck(back20);
        TEST_EXPECT(r0 == r20);
        std::string back21 = std::format("{:fzmu}", *r0);
        auto r21 = address_v6::from_string_uncheck(back21);
        TEST_EXPECT(r0 == r21);
    };
    roundtrip6("1:2:3:4:5:6:7:8");
    roundtrip6("2001:db8::1");
    roundtrip6("fe80::1234:5678:9abc:def0");
    roundtrip6("fe80::1234:5678:0:def0");
    roundtrip6("::ffff:192.168.0.1");
    roundtrip6("aaaa::ffff:192.168.0.1");
    roundtrip6("2001:db8:85a3::8a2e:370:7334");
    roundtrip6("2001:db8:85a3:ffff:8a2e:370:7334:eeee");
    roundtrip6("2001:db8:85a3:ffff:8a2e:370:192.168.0.1");
    // cases that not recommended by RFC 5952 but still valid
    roundtrip6("2001:0db8:85a3:ffff:8a2e:07:7334:eeee"); // not full length leading zeros
    roundtrip6("2001:0db8:85a3:ffff:8a2e:007:7334:eeee"); // not full length leading zeros
    roundtrip6("2001:0db8:85a3:ffff:8a2e::7334:eeee"); // only compress one zero group
    roundtrip6("2001:db8:0:0::0:eeee"); // not compress all possible zeros
    roundtrip6("2001:dB8:85A3:fFFf:8a2E:370:7334:EeEe"); // not lowercase
    roundtrip6("1:0:0:0:5::8"); // compress zeros but not the longest zero group

    // standard inet_pton inet_ntop tests
    auto standard = [](std::string s, std::source_location loc = std::source_location::current()) {
        auto addr = address_v6::from_string(s);
        TEST_EXPECT(addr.has_value());
        std::string back = addr->to_string();
        v6raw raw = addr->raw();
        ::in6_addr addr6 = std::bit_cast<::in6_addr>(raw);
        char buf[INET6_ADDRSTRLEN] = {};
        const char* r = ::inet_ntop(AF_INET6, &addr6, buf, sizeof(buf));
        TEST_EXPECT(r != nullptr);
        TEST_EXPECT(back == buf);
        int r2 = ::inet_pton(AF_INET6, s.c_str(), &addr6);
        TEST_EXPECT(r2 == 1);
        TEST_EXPECT(std::bit_cast<v6raw>(addr6) == raw);
    };

    standard("::");
    standard("::1");
    standard("::abcd");
    standard("::abcd:0");
    standard("1:2:3:4:5:6:7:8");
    standard("2001:db8::1");
    standard("fe80::1234:5678:9abc:def0");
    standard("::ffff:1");
    standard("2001:db8:85a3:ffff:8a2e:370:7334:eeee");
    standard("2001:db8:85a3:ffff:8a2e:370:192.168.0.1");
    standard("::ffff:192.168.0.1");
    standard("::192.168.0.1");

    // formatter tests
    {
#define FORMATTER_EXPECT(...) do { \
    std::source_location loc = std::source_location::current(); \
    TEST_EXPECT((__VA_ARGS__)); \
} while(0)
        // default and {:r}
        {
            constexpr std::string_view tests[] = {
                "::", "::1", "::abcd",
                "::192.168.0.1", "::ffff:192.168.0.1",
                "::ffff:0.0.0.1",
                "2001:db8:85a3:ffff:8a2e:370:7334:eeee",
                "2001:db8::ffff:c0a8:1",
                "2001:db8::c0a8:1",
                "2001:db8:85a3:0:8a2e::",
            };
            for (std::string_view s : tests) {
                auto addr = address_v6::from_string(s);
                FORMATTER_EXPECT(addr.has_value());
                std::string back1 = std::format("{}", *addr);
                FORMATTER_EXPECT(back1 == s);
                std::string back2 = std::format("{:r}", *addr);
                FORMATTER_EXPECT(back2 == s);
            }
        }
        // {:f}
        {
            constexpr std::string_view tests[] = {
                "0:0:0:0:0:0:0:0", "0:0:0:0:0:0:0:1",
                "0:0:0:0:0:0:c0a8:1", "0:0:0:0:0:ffff:c0a8:1",
                "2001:db8:85a3:ffff:8a2e:370:7334:eeee",
                "2001:db8:0:0:0:0:ffff:1",
            };
            for (std::string_view s : tests) {
                auto addr = address_v6::from_string(s);
                FORMATTER_EXPECT(addr.has_value());
                std::string back = std::format("{:f}", *addr);
                FORMATTER_EXPECT(back == s);
            }
        }
        // {:z}
        {
            constexpr std::string_view tests[] = {
                "::c0a8:0001", "::ffff:c0a8:0001",
                "2001:0db8:85a3:000f:8a2e:0370:7334:eeee",
                "2001:0008:85a3::7334:0eee",
                "0000:0000:05a3::0eee",
                "2001::000f:8a2e:0000:0000:eeee",
            };
            for (std::string_view s : tests) {
                auto addr = address_v6::from_string(s);
                FORMATTER_EXPECT(addr.has_value());
                std::string back = std::format("{:z}", *addr);
                std::println("{} {}", s, back);
                FORMATTER_EXPECT(back == s);
            }
        }
        // {:m}
        {
            constexpr std::string_view tests[] = {
                "::192.168.0.1", "::ffff:192.168.0.1",
                "2001:db8:85a3:ffff:8a2e:370:192.168.0.1",
                "2001::8a2e:370:192.168.0.1",
                "::ffff:8a2e:370:192.168.0.1",
                "2001:db8:85a3::192.168.0.1",
                "::85a3:ffff:0:0:192.168.0.1",
            };
            for (std::string_view s : tests) {
                auto addr = address_v6::from_string(s);
                FORMATTER_EXPECT(addr.has_value());
                std::string back = std::format("{:m}", *addr);
                FORMATTER_EXPECT(back == s);
            }
        }
        // {:n}
        {
            constexpr std::string_view tests[] = {
                "::", "::1", "::abcd",
                "::c0a8:1", "::ffff:c0a8:1",
                "2001:db8:85a3:ffff:8a2e:370:7334:eeee",
                "2001:0:0:ffff::eeee",
                "2001:db8:85a3:0:8a2e::",
                "2001::ffff:8a2e:0:0:eeee",
            };
            for (std::string_view s : tests) {
                auto addr = address_v6::from_string(s);
                FORMATTER_EXPECT(addr.has_value());
                std::string back = std::format("{:n}", *addr);
                FORMATTER_EXPECT(back == s);
            }
        }
        // {:u}
        {
            constexpr std::string_view tests[] = {
                "::", "::1", "::ABCD", "::FFFF:0",
                "::C0A8:1", "::FFFF:C0A8:1",
                "2001:DB8:85A3:FFFF:8A2E:370:7334:EEEE",
                "2001:DB8:85A3::7334:EEEE",
                "2001:0:0:FFFF::EEEE",
                "2001:DB8:85A3:0:8A2E::",
            };
            for (std::string_view s : tests) {
                auto addr = address_v6::from_string(s);
                FORMATTER_EXPECT(addr.has_value());
                std::string back = std::format("{:u}", *addr);
                FORMATTER_EXPECT(back == s);
            }
        }

        // Combined formats of 'r'
        // rn ru rnu

        // {:rn}
        {
            constexpr std::string_view tests[] = {
                "::", "::1", "::abcd",
                "::c0a8:1", "::ffff:c0a8:1",
                "::ffff:0:1",
                "2001:db8:85a3:ffff:8a2e:370:7334:eeee",
                "2001:0:0:ffff::eeee",
                "2001:db8:85a3:0:8a2e::",
            };
            for (std::string_view s : tests) {
                auto addr = address_v6::from_string(s);
                FORMATTER_EXPECT(addr.has_value());
                std::string back = std::format("{:rn}", *addr);
                FORMATTER_EXPECT(back == s);
            }
        }
        // {:ru}
        {
            constexpr std::string_view tests[] = {
                "::", "::1", "::ABCD",
                "::192.168.0.1", "::FFFF:192.168.0.1",
                "::FFFF:0.0.0.1",
                "2001:DB8:85A3:FFFF:8A2E:370:7334:EEEE",
                "2001:0:0:FFFF::EEEE",
                "2001:DB8:85A3:0:8A2E::",
            };
            for (std::string_view s : tests) {
                auto addr = address_v6::from_string(s);
                FORMATTER_EXPECT(addr.has_value());
                std::string back = std::format("{:ru}", *addr);
                FORMATTER_EXPECT(back == s);
            }
        }
        // {:rnu}
        {
            constexpr std::string_view tests[] = {
                "::", "::1", "::ABCD",
                "::C0A8:1", "::FFFF:C0A8:1",
                "::FFFF:0:1",
                "2001:DB8:85A3:FFFF:8A2E:370:7334:EEEE",
                "2001:0:0:FFFF::EEEE",
                "2001:DB8:85A3:0:8A2E::",
            };
            for (std::string_view s : tests) {
                auto addr = address_v6::from_string(s);
                FORMATTER_EXPECT(addr.has_value());
                std::string back = std::format("{:rnu}", *addr);
                FORMATTER_EXPECT(back == s);
            }
        }

        // Other combined formats (of 'f', 'z', 'm', 'u')
        // fz fm fu zm zu mu
        // fzm fzu fmu zmu
        // fzmu

        // {:fz}
        {
            constexpr std::string_view tests[] = {
                "0000:0000:0000:0000:0000:0000:0000:0000",
                "0000:0000:0000:0000:0000:0000:0000:0001",
                "0000:0000:0000:0000:0000:0000:c0a8:0001",
                "0000:0000:0000:0000:0000:ffff:c0a8:0001",
                "2001:0db8:85a3:ffff:8a2e:0370:7334:eeee",
                "2001:0db8:0000:0000:0000:0000:ffff:0001",
                "0000:0000:05a3:0000:0000:0000:0000:0eee",
            };
            for (std::string_view s : tests) {
                auto addr = address_v6::from_string(s);
                FORMATTER_EXPECT(addr.has_value());
                std::string back = std::format("{:fz}", *addr);
                FORMATTER_EXPECT(back == s);
            }
        }
        // {:fm}
        {
            constexpr std::string_view tests[] = {
                "0:0:0:0:0:0:192.168.0.1", "0:0:0:0:0:ffff:192.168.0.1",
                "2001:db8:85a3:ffff:8a2e:370:192.168.0.1",
                "2001:0:0:0:8a2e:370:192.168.0.1",
                "0:0:0:ffff:8a2e:370:192.168.0.1",
                "2001:db8:85a3:0:0:0:192.168.0.1",
            };
            for (std::string_view s : tests) {
                auto addr = address_v6::from_string(s);
                FORMATTER_EXPECT(addr.has_value());
                std::string back = std::format("{:fm}", *addr);
                FORMATTER_EXPECT(back == s);
            }
        }
        // {:fu}
        {
            constexpr std::string_view tests[] = {
                "0:0:0:0:0:0:0:0", "0:0:0:0:0:0:0:1",
                "0:0:0:0:0:0:0:ABCD", "0:0:0:0:0:0:FFFF:0",
                "0:0:0:0:0:0:C0A8:1", "0:0:0:0:0:FFFF:C0A8:1",
                "2001:DB8:85A3:FFFF:8A2E:370:7334:EEEE",
                "2001:DB8:85A3:0:0:0:7334:EEEE",
                "2001:0:0:FFFF:0:0:0:EEEE",
                "2001:DB8:85A3:0:8A2E:0:0:0",
            };
            for (std::string_view s : tests) {
                auto addr = address_v6::from_string(s);
                FORMATTER_EXPECT(addr.has_value());
                std::string back = std::format("{:fu}", *addr);
                FORMATTER_EXPECT(back == s);
            }
        }
        // {:zm}
        {
            constexpr std::string_view tests[] = {
                "::192.168.0.1", "::ffff:192.168.0.1",
                "2001:0db8:85a3:ffff:8a2e:0370:192.168.0.1",
                "2001::8a2e:0370:192.168.0.1",
                "::ffff:8a2e:0370:192.168.0.1",
                "2001:0db8:85a3::192.168.0.1",
                "2001:0db8:0000:ffff::192.168.0.1",
                "2001::ffff:0000:0000:192.168.0.1",
            };
            for (std::string_view s : tests) {
                auto addr = address_v6::from_string(s);
                FORMATTER_EXPECT(addr.has_value());
                std::string back = std::format("{:zm}", *addr);
                FORMATTER_EXPECT(back == s);
            }
        }
        // {:zu}
        {
            constexpr std::string_view tests[] = {
                "::C0A8:0001", "::FFFF:C0A8:0001",
                "2001:0DB8:85A3:000F:8A2E:0370:7334:EEEE",
                "2001:0008:85A3::7334:0EEE",
                "0000:0000:05A3::0EEE",
                "2001::000F:8A2E:0000:0000:EEEE",
            };
            for (std::string_view s : tests) {
                auto addr = address_v6::from_string(s);
                FORMATTER_EXPECT(addr.has_value());
                std::string back = std::format("{:zu}", *addr);
                FORMATTER_EXPECT(back == s);
            }
        }
        // {:mu}
        {
            constexpr std::string_view tests[] = {
                "::192.168.0.1", "::FFFF:192.168.0.1",
                "2001:DB8:85A3:FFFF:8A2E:370:192.168.0.1",
                "2001::8A2E:370:192.168.0.1",
                "::FFFF:8A2E:370:192.168.0.1",
                "2001:DB8:85A3::192.168.0.1",
                "::85A3:FFFF:0:0:192.168.0.1",
            };
            for (std::string_view s : tests) {
                auto addr = address_v6::from_string(s);
                FORMATTER_EXPECT(addr.has_value());
                std::string back = std::format("{:mu}", *addr);
                FORMATTER_EXPECT(back == s);
            }
        }
        // {:fzm}
        {
            constexpr std::string_view tests[] = {
                "0000:0000:0000:0000:0000:0000:192.168.0.1",
                "0000:0000:0000:0000:0000:ffff:192.168.0.1",
                "2001:0db8:85a3:ffff:8a2e:0370:192.168.0.1",
                "2001:0000:0000:0000:8a2e:0370:192.168.0.1",
                "0000:0000:0000:ffff:8a2e:0370:192.168.0.1",
                "2001:0db8:85a3:0000:0000:0000:192.168.0.1",
                "2001:0000:0000:ffff:0000:0000:192.168.0.1",
            };
            for (std::string_view s : tests) {
                auto addr = address_v6::from_string(s);
                FORMATTER_EXPECT(addr.has_value());
                std::string back = std::format("{:fzm}", *addr);
                FORMATTER_EXPECT(back == s);
            }
        }
        // {:fzu}
        {
            constexpr std::string_view tests[] = {
                "0000:0000:0000:0000:0000:0000:0000:0000",
                "0000:0000:0000:0000:0000:0000:0000:0001",
                "0000:0000:0000:0000:0000:0000:0000:ABCD",
                "0000:0000:0000:0000:0000:0000:FFFF:0000",
                "0000:0000:0000:0000:0000:0000:C0A8:0001",
                "2001:0DB8:85A3:FFFF:8A2E:0370:7334:EEEE",
                "2001:0DB8:85A3:0000:0000:0000:7334:EEEE",
                "2001:0DB8:0000:0000:FFFF:0000:0000:EEEE",
                "2001:0DB8:85A3:000F:8A2E:0000:C0A8:0001",
            };
            for (std::string_view s : tests) {
                auto addr = address_v6::from_string(s);
                FORMATTER_EXPECT(addr.has_value());
                std::string back = std::format("{:fzu}", *addr);
                FORMATTER_EXPECT(back == s);
            }
        }
        // {:fmu}
        {
            constexpr std::string_view tests[] = {
                "0:0:0:0:0:0:192.168.0.1",
                "0:0:0:0:0:CF:192.168.0.1",
                "2001:DB8:85A3:FFFF:8A2E:370:192.168.0.1",
                "2001:0:0:0:8A2E:370:192.168.0.1",
                "0:0:0:FFFF:8A2E:370:192.168.0.1",
                "2001:DB8:85A3:0:0:0:192.168.0.1",
            };
            for (std::string_view s : tests) {
                auto addr = address_v6::from_string(s);
                FORMATTER_EXPECT(addr.has_value());
                std::string back = std::format("{:fmu}", *addr);
                FORMATTER_EXPECT(back == s);
            }
        }
        // {:zmu}
        {
            constexpr std::string_view tests[] = {
                "::192.168.0.1", "::FFFF:192.168.0.1",
                "2001:0DB8:85A3:FFFF:8A2E:0370:192.168.0.1",
                "2001::8A2E:0370:192.168.0.1",
                "::FFFF:8A2E:0370:192.168.0.1",
                "2001:0DB8:85A3::192.168.0.1",
                "2001:0DB8:0000:00CF::192.168.0.1",
                "2001::FFFF:0000:0000:192.168.0.1",
            };
            for (std::string_view s : tests) {
                auto addr = address_v6::from_string(s);
                FORMATTER_EXPECT(addr.has_value());
                std::string back = std::format("{:zmu}", *addr);
                FORMATTER_EXPECT(back == s);
            }
        }
        // {:fzmu}
        {
            constexpr std::string_view tests[] = {
                "0000:0000:0000:0000:0000:0000:192.168.0.1",
                "0000:0000:0000:0000:0000:FFFF:192.168.0.1",
                "2001:0DB8:85A3:FFFF:8A2E:0370:192.168.0.1",
                "2001:0000:0000:0000:8A2E:0370:192.168.0.1",
                "0000:0000:0000:FFFF:8A2E:0370:192.168.0.1",
                "2001:0DB8:85A3:0000:0000:0000:192.168.0.1",
                "2001:0000:0000:FFFF:0000:0000:192.168.0.1",
            };
            for (std::string_view s : tests) {
                auto addr = address_v6::from_string(s);
                FORMATTER_EXPECT(addr.has_value());
                std::string back = std::format("{:fzmu}", *addr);
                FORMATTER_EXPECT(back == s);
            }
        }
#undef FORMATTER_EXPECT
    }

    // invalid cases for ipv6
    auto invalid6 = [](std::string_view s,
        std::source_location loc = std::source_location::current()) {
        auto r = address_v6::from_string(s);
        TEST_EXPECT(!r.has_value());
    };
    invalid6("");
    invalid6(" ");
    invalid6(":");
    invalid6(":::");
    invalid6("1:2:3:4:5:6:7:8:9");
    invalid6("2001:db8:85a3:ffff: 8a2e:370:7334:eeee");
    invalid6("2001:db8: 85a3 :ffff:8a2e:370:7334:eeee");
    invalid6("2001:db8:85a3:ffff:8a2e:370:7334 :eeee");
    invalid6("2001:db8:85a3:ff1ff:8a2e:370:7334:eeee");
    invalid6("2001:db8:85a3:ffff:8a2e:370:7334:10000");
    invalid6("1:00000::1");
    invalid6("gggg::1");
    invalid6("1:gggg::1");
    invalid6("1:1::gggg");
    invalid6("1:2:3:4:5:6:7:8:");
    invalid6(":1:2:3:4:5:6:7:8");
    invalid6("1::2::3");
    invalid6("::1 ");
    invalid6(" ::1");
    invalid6(":: 1");
    invalid6(":::1");
    invalid6("1::1::");
    invalid6("::1::1");
    invalid6("1::1:1:1::1");
    invalid6("1:2:3:4:5:6:7");
    invalid6("1111:2222:::3333:4444");
    invalid6("1:2:3:4:5::6:7:8");
    invalid6("192.168.0.1");
    invalid6("192.168.0.1::");
    invalid6("192.168.0.1::1");
    invalid6("ffff:192.168.0.1:aaaa::");
    invalid6("1:192.168.0.1::1:192.168.0.1");
    invalid6("2001:db8:85a3:ffff:192.168.0.1:8a2e:370");
    invalid6("2001:db8:85a3:ffff:8a2e:370:192.168.0.256");
    invalid6("[001:db8:85a3:ffff:8a2e:370:7334:eeee");
    invalid6("2001:db8:8[a3:ffff:8a2e:370:7334:eeee");
    invalid6("2001:db8:85a3:ffff:8a2[:370:7334:eeee");
    invalid6("2001:db8:85a3:ffff:8a2e:370:7334:eee[");

    // ipv6 literals
    {
        std::source_location loc = std::source_location::current();
        using namespace iouxx::literals::network_literals;
        constexpr address_v6 a1 = "::1"_ipv6;
        TEST_EXPECT(a1 == address_v6::loopback());
    }

    std::println("All ipv6 tests passed.");
}

int main() {
    std::source_location loc = std::source_location::current();
    TEST_EXPECT(true);
    ip_parse_test();
}
