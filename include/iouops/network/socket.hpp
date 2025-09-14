#pragma once
#include "cxxmodule_helper.hpp"
#ifndef IOUXX_OPERATION_NETWORK_SOCKET_H
#define IOUXX_OPERATION_NETWORK_SOCKET_H 1

#ifndef IOUXX_USE_CXX_MODULE

#include "sys/socket.h"
#include "netdb.h"

#include <utility>
#include <string_view>
#include <flat_map>
#include <vector>
#include <stdexcept>
#include <cstddef>
#include <iterator>
#include <ranges>

#include "util/utility.hpp"
#include "iouops/file/file.hpp"

#endif // IOUXX_USE_CXX_MODULE

IOUXX_EXPORT
namespace iouxx::inline iouops::network {

    // Warning:
    // This class is NOT a RAII wrapper of socket fd.
    class socket : public file::file
    {
        using base = file;
    public:
        enum class domain
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
            max = AF_MAX
        };

        enum class type
        {
            stream = SOCK_STREAM,
            datagram = SOCK_DGRAM,
            raw = SOCK_RAW,
            rdm = SOCK_RDM,
            seqpacket = SOCK_SEQPACKET,
            dccp = SOCK_DCCP,
            packet = SOCK_PACKET,

            nonblock = SOCK_NONBLOCK,
            cloexec = SOCK_CLOEXEC,
        };

        // Note: only nonblock and cloexec are supported as bitflag
        friend constexpr type operator|(type lhs, type rhs) noexcept {
            return static_cast<type>(
                std::to_underlying(lhs) | std::to_underlying(rhs)
            );
        }

        static constexpr int UNKNOWN_PROTOCOL_NO = -1;
        static constexpr int PROTOCOL_NO_LIMIT = 256;

        enum class protocol
        {
            unknown = UNKNOWN_PROTOCOL_NO,
            // No entries here
            // Use helper function to map protocol name to number
            max_protocol_no = PROTOCOL_NO_LIMIT,
        };

        explicit socket(int fd, domain d, type t, protocol p) noexcept
            : base(fd), d(d), t(t), p(p)
        {}

        [[nodiscard]]
        domain socket_domain() const noexcept { return d; }
        [[nodiscard]]
        type socket_type() const noexcept { return t; }
        [[nodiscard]]
        protocol socket_protocol() const noexcept { return p; }
        using base::native_handle;

        socket() = default;
        socket(const socket&) = default;
        socket& operator=(const socket&) = default;

    private:
        domain d = domain::unspec;
        type t = type::stream;
        protocol p = protocol::unknown;
    };

    struct unspecified_socket_info
    {
        static constexpr socket::domain domain = socket::domain::unspec;

        constexpr ::sockaddr to_system_sockaddr() const noexcept {
            return ::sockaddr{};
        }
    };

    // Warning:
    // This class is NOT a RAII wrapper of connection fd.
    class connection : public socket
    {
    public:
        connection() = default;
        connection(const connection&) = default;
        connection& operator=(const connection&) = default;

        explicit connection(const socket& sock, int fd) :
            socket(sock), conn_fd(fd)
        {}

        // Note: if socket fd is wanted, use socket::native_handle()
        [[nodiscard]]
        int native_handle() const noexcept { return conn_fd; }

    private:
        int conn_fd = -1;
    };

    class protocol_database : public std::ranges::view_interface<protocol_database>
    {
    public:
        protocol_database(const protocol_database&) = delete;
        protocol_database& operator=(const protocol_database&) = delete;

        using protocol = socket::protocol;

        // Warning: need exclusive access to netdb.h functions
        [[nodiscard]]
        static const protocol_database& instance() {
            static protocol_database db;
            return db;
        }

        static constexpr int UNKNOWN_PROTOCOL_NO = socket::UNKNOWN_PROTOCOL_NO;
        static constexpr int PROTOCOL_NO_LIMIT = socket::PROTOCOL_NO_LIMIT;

        struct [[nodiscard]] entry {
            std::string name = "unknown";
            std::vector<std::string> alias = { "Unknown", "UNKNOWN" };
            protocol no = protocol::unknown;

            constexpr explicit operator bool() const noexcept {
                return no != protocol::unknown;
            }

            friend constexpr bool operator==(const entry& lhs, const entry& rhs) noexcept {
                return lhs.no == rhs.no;
            }
        };

        inline static const entry unknown_protocol = {
            .name = "unknown",
            .alias = { "Unknown", "UNKNOWN" },
            .no = protocol::unknown
        };

        const entry& get(std::string_view name) const noexcept {
            auto it = name_index.find(name);
            if (it != name_index.end()) {
                return db[it->second];
            } else {
                return unknown_protocol;
            }
        }

        const entry& get(protocol p) const noexcept {
            const int no = std::to_underlying(p);
            if (no >= 0 && no < PROTOCOL_NO_LIMIT) {
                return db[no];
            } else {
                return unknown_protocol;
            }
        }

        [[nodiscard]]
        bool contains(std::string_view name) const noexcept {
            return name_index.contains(name);
        }

        [[nodiscard]]
        bool contains(protocol p) const noexcept {
            const int no = std::to_underlying(p);
            return no >= 0 && no < PROTOCOL_NO_LIMIT
                && db[no].no != protocol::unknown;
        }

        std::size_t size() const noexcept { return total; }

        struct iterator
        {
        public:
            using value_type = const entry;
            using difference_type = std::ptrdiff_t;
            using pointer = const entry*;
            using reference = const entry&;
            using iterator_category = std::forward_iterator_tag;

            iterator() = default;
            iterator(const iterator&) = default;
            iterator& operator=(const iterator&) = default;

            reference operator*() const noexcept { return *iter; }
            pointer operator->() const noexcept { return &(*iter); }

            iterator& operator++() noexcept {
                do { ++iter; }
                while (iter != end && *iter == unknown_protocol);
                return *this;
            }

            iterator operator++(int) noexcept {
                iterator tmp = *this;
                ++iter;
                return tmp;
            }

            friend constexpr bool operator==(const iterator& lhs, const iterator& rhs) noexcept {
                return lhs.iter == rhs.iter;
            }

            friend constexpr bool operator==(const iterator& iter, std::default_sentinel_t) noexcept {
                return iter.iter == iter.end;
            }

        private:
            using base_iter = std::vector<entry>::const_iterator;
            friend protocol_database;
            explicit iterator(base_iter iter, base_iter end) noexcept
                : iter(iter), end(end)
            {}

            base_iter iter;
            base_iter end;
        };

        [[nodiscard]]
        iterator begin() const noexcept {
            auto iter = db.begin();
            auto end = db.end();
            while (iter != end && *iter == unknown_protocol) {
                ++iter;
            }
            return iterator(iter, db.end());
        }

        [[nodiscard]]
        constexpr std::default_sentinel_t end() const noexcept {
            return std::default_sentinel;
        }

    private:
        protocol_database() {
            // TODO: flat_* lacks reserve() method for now
            // name_index.reserve(PROTOCOL_NO_LIMIT);

            // Load protocol database
            ::protoent* raw_entry = nullptr;
            ::setprotoent(1);
            utility::defer _([] { ::endprotoent(); });
            while ((raw_entry = ::getprotoent()) != nullptr) {
                int no = raw_entry->p_proto;
                if (no >= PROTOCOL_NO_LIMIT) {
                    throw std::runtime_error("Protocol number too large");
                }
                entry& db_entry = db[no];
                if (db_entry.no != protocol::unknown) {
                    // Duplicate entry
                    continue;
                }
                ++total;
                db_entry.no = static_cast<protocol>(no);
                db_entry.name.clear();
                db_entry.alias.clear();
                if (char* name = raw_entry->p_name) {
                    std::string_view name_view(name);
                    if (!name_view.empty()) {
                        db_entry.name = raw_entry->p_name;
                    }
                }
                if (char** aliases = raw_entry->p_aliases) {
                    for (char** alias = aliases; *alias; ++alias) {
                        std::string_view alias_view(*alias);
                        if (!alias_view.empty()) {
                            db_entry.alias.emplace_back(alias_view);
                        }
                    }
                    // entry.alias will not be modified after this
                }
                // Create name index
                if (!db_entry.name.empty()) {
                    name_index.try_emplace(db_entry.name, no);
                }
                for (std::string_view alias : db_entry.alias) {
                    name_index.try_emplace(alias, no);
                }
            }
        }

        std::vector<entry> db = std::vector<entry>(PROTOCOL_NO_LIMIT);
        std::flat_map<std::string_view, std::size_t> name_index{};
        std::size_t total = 0;
    };

    static_assert(std::ranges::forward_range<protocol_database>);

    inline socket::protocol to_protocol(std::string_view name) noexcept {
        return protocol_database::instance().get(name).no;
    }

    inline std::string_view get_protocol_name(socket::protocol p) noexcept {
        return protocol_database::instance().get(p).name;
    }

} // namespace iouxx::iouops::network

#endif // IOUXX_OPERATION_NETWORK_SOCKET_H
