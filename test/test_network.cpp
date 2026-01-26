#ifdef IOUXX_CONFIG_USE_CXX_MODULE

import std;
import iouxx.ring;
import iouxx.util;
import iouxx.ops.network.socketio;

#else // !IOUXX_CONFIG_USE_CXX_MODULE

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <span>
#include <thread>
#include <atomic>
#include <string_view>
#include <print>
#include <system_error>

// This tests may be skiped if certain features are not supported.
// Always enable feature tests in this file
#ifndef IOUXX_CONFIG_ENABLE_FEATURE_TESTS
#define IOUXX_CONFIG_ENABLE_FEATURE_TESTS
#endif // IOUXX_CONFIG_ENABLE_FEATURE_TESTS

#include "iouxx/iouringxx.hpp"
#include "iouxx/iouops/network/ip.hpp"
#include "iouxx/iouops/network/socket.hpp"
#include "iouxx/iouops/network/socketio.hpp"

#endif // IOUXX_CONFIG_USE_CXX_MODULE

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cerrno>

using namespace iouxx;
using namespace std::literals;
using namespace iouxx::literals;
inline constexpr std::string_view magic_word = "exit"sv;
inline constexpr std::string_view client_msg = "Hello io_uring!"sv;
inline constexpr std::size_t client_msg_cnt = 10;
// Use different port among tests to avoid conflict
#if defined(__clang__)
#if defined(IOUXX_CONFIG_USE_CXX_MODULE)
inline constexpr network::ip::socket_v4_info server_addr = "127.0.0.1:38080"_sockv4;
inline constexpr network::ip::socket_v4_info client_addr = "127.0.0.1:38081"_sockv4;
#else
inline constexpr network::ip::socket_v4_info server_addr = "127.0.0.1:38082"_sockv4;
inline constexpr network::ip::socket_v4_info client_addr = "127.0.0.1:38083"_sockv4;
#endif
#elif defined(__GNUC__)
#if defined(IOUXX_CONFIG_USE_CXX_MODULE)
inline constexpr network::ip::socket_v4_info server_addr = "127.0.0.1:38084"_sockv4;
inline constexpr network::ip::socket_v4_info client_addr = "127.0.0.1:38085"_sockv4;
#else
inline constexpr network::ip::socket_v4_info server_addr = "127.0.0.1:38086"_sockv4;
inline constexpr network::ip::socket_v4_info client_addr = "127.0.0.1:38087"_sockv4;
#endif
#endif
static std::atomic<bool> server_started = false; // publish after listen is ready

// If encountered unsupported operation, skip this test
inline void exit_if_function_not_supported(const std::error_code& ec) noexcept {
    if (ec) {
        if (ec == std::errc::function_not_supported   // ENOSYS
            || ec == std::errc::operation_not_supported) { // ENOTSUP/EOPNOTSUPP
            std::println("Encountered unsupported io_uring opcode, treat as success");
            std::exit(0);
        }
    }
}

template<typename T, typename ByteType>
[[nodiscard]] std::span<T> start_lifetime_as(std::span<ByteType> byte_span) noexcept {
    void* p = byte_span.data();
    std::size_t n = byte_span.size();
    void* washed = std::memmove(p, p, n);
    return std::span<T>(std::launder(static_cast<T*>(washed)), n / sizeof(T));
}

void echo_server() {
    ring ring(256);
    network::socket sock = [&ring] {
        auto open = ring.make_sync<network::socket_open_operation>();
        open.domain(network::socket_config::domain::ipv4)
            .type(network::socket_config::type::stream)
            .protocol(network::to_protocol("tcp"));
        if (auto res = open.submit_and_wait()) {
            std::println("Server socket created: {}", res->native_handle());
            return std::move(*res);
        } else {
            exit_if_function_not_supported(res.error());
            std::println("Failed to create server socket: {}", res.error().message());
            std::abort();
        }
    }();

    bool sockcmd_exists = true;
    [&ring, &sock, &sockcmd_exists] {
        using op = network::socket_setoption<network::sockopts::general::reuseaddr>;
        auto setsockopt = ring.make_sync<op::operation>();
        setsockopt.socket(sock)
            .option(true);
        if (auto res = setsockopt.submit_and_wait()) {
            std::println("Socket option SO_REUSEADDR set");
        } else {
            if (res.error() == std::errc::function_not_supported
                || res.error() == std::errc::operation_not_supported) {
                std::println("Socket option operation not supported, fallback to system call");
                sockcmd_exists = false;
                return;
            }
            std::println("Failed to set socket option SO_REUSEADDR: {}", res.error().message());
            std::abort();
        }
    }();

    if (!sockcmd_exists) {
        int optval = 1;
        int resopt = ::setsockopt(sock.native_handle(), SOL_SOCKET, SO_REUSEADDR,
            &optval, sizeof(optval));
        if (resopt < 0) {
            std::println("Failed to set socket options: {}", std::strerror(errno));
            ::close(sock.native_handle());
            std::abort();
        }
    }

    // Not until kernel 6.11 do io_uring have op bind and op listen
    bool bind_op_exists = true;

    [&ring, &sock, &bind_op_exists] {
        auto bind = ring.make_sync<network::socket_bind_operation>();
        bind.socket(sock)
            .socket_info(server_addr);
        if (auto res = bind.submit_and_wait()) {
            std::println("Socket bound successfully");
        } else {
            if (res.error() == std::errc::function_not_supported
                || res.error() == std::errc::operation_not_supported) {
                std::println("Socket bind operation not supported, fallback to system call");
                bind_op_exists = false;
                return;
            }
            std::println("Failed to bind socket: {}", res.error().message());
            std::abort();
        }
    }();

    if (bind_op_exists) {
        [&ring, &sock] {
            auto listen = ring.make_sync<network::socket_listen_operation>();
            listen.socket(sock)
                .backlog(128);
            if (auto res = listen.submit_and_wait()) {
                std::println("Socket is listening");
            } else {
                exit_if_function_not_supported(res.error());
                std::println("Failed to listen on socket: {}", res.error().message());
                std::abort();
            }
            server_started.store(true, std::memory_order_release);
        }();
    } else {
        ::sockaddr_in addr = server_addr.to_system_sockaddr();
        if (::bind(sock.native_handle(),
                reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            std::println("Failed to bind socket: {}", std::strerror(errno));
            std::abort();
        }
        if (::listen(sock.native_handle(), 128) < 0) {
            std::println("Failed to listen on socket: {}", std::strerror(errno));
            std::abort();
        }
        std::println("Socket is listening");
        server_started.store(true, std::memory_order_release);
    }

    network::connection connection = [&ring, &sock] {
        auto accept = ring.make_sync<network::socket_accept_operation>();
        accept.socket(sock);
        if (auto res = accept.submit_and_wait()) {
            auto&& [conn, peer] = *res;
            std::println("Accepted connection: {}", conn.native_handle());
            auto& info = std::get<network::ip::socket_v4_info>(peer);
            std::println("Peer address: {}", info);
            if (info != client_addr) {
                std::println("Peer address does not match expected client address");
                std::abort();
            }
            return std::move(conn);
        } else {
            exit_if_function_not_supported(res.error());
            std::println("Failed to accept connection: {}", res.error().message());
            std::abort();
        }
    }();
    std::vector<std::byte> buffer(4096);
    while (true) {
        std::println("Waiting for data...");
        std::string_view message;
        auto recv = ring.make_sync<network::socket_recv_operation>();
        recv.socket(connection)
            .buffer(buffer);
        if (auto res = recv.submit_and_wait()) {
            message = std::string_view(start_lifetime_as<char>(*res));
            if (message == magic_word) {
                std::println("Magic word received, exiting...");
                break;
            } else {
                std::println("Received {} bytes: '{}'", message.size(), message);
            }
        } else {
            exit_if_function_not_supported(res.error());
            std::println("Failed to receive data: {}", res.error().message());
            std::abort();
        }

        std::println("Echoing back...");
        // auto send = ring.make_sync<network::socket_send_operation>();
        // send.socket(connection)
        //     .buffer(std::span(buffer.data(), received));
        // if (auto res = send.submit_and_wait()) {
        //     std::println("Echoed back {} bytes", *res);
        // } else {
        //     exit_if_function_not_supported(res.error());
        //     std::println("Failed to send data: {}", res.error().message());
        //     std::abort();
        // }
        bool is_more = true;
        auto send = ring.make<network::socket_send_zc_operation>(
            [&](std::expected<network::send_zc_result, std::error_code> res) {
            if (res) {
                std::visit(iouxx::utility::overloaded {
                    [&](const network::buffer_free_notification&) {
                        std::println("Buffer is free to reuse");
                        is_more = false;
                    },
                    [&](const network::send_result_more& r) {
                        std::println("Sent {} bytes, wait for buffer free", r.bytes_sent);
                    },
                    [&](const network::send_result_nomore& r) {
                        std::println("Sent {} bytes and buffer is free", r.bytes_sent);
                        is_more = false;
                    }
                }, *res);
            } else {
                std::println("Failed to send data: {}", res.error().message());
                std::abort();
            }
        });
        send.socket(connection)
            .buffer(std::span(buffer.data(), message.size()));
        if (auto res = send.submit()) {
            exit_if_function_not_supported(res);
            std::println("Failed to submit send operation: {}", res.message());
            std::abort();
        } else {
            std::println("Send operation submitted");
        }
        while (is_more) {
            if (auto res = ring.wait_for_result()) {
                res->callback();
            } else {
                std::println("Failed to get send result: {}", res.error().message());
                std::abort();
            }
        }
    }

    [&ring, &connection] {
        auto shutdown = ring.make_sync<network::socket_shutdown_operation>();
        shutdown.socket(connection)
            .options(network::shutdown_option::rdwr);
        if (auto res = shutdown.submit_and_wait()) {
            std::println("Connection shutdown successfully");
        } else {
            exit_if_function_not_supported(res.error());
            std::println("Failed to shutdown connection: {}", res.error().message());
            std::abort();
        }
    }();

    [&ring, &sock] {
        auto close = ring.make_sync<network::socket_close_operation>();
        close.socket(sock);
        if (auto res = close.submit_and_wait()) {
            std::println("Socket closed successfully");
        } else {
            exit_if_function_not_supported(res.error());
            std::println("Failed to close socket: {}", res.error().message());
            std::abort();
        }
    }();
}

void echo_client() {
    ring ring(256);
    network::socket sock = [&ring] {
        auto open = ring.make_sync<network::socket_open_operation>();
        open.domain(network::socket_config::domain::ipv4)
            .type(network::socket_config::type::stream)
            .protocol(network::to_protocol("tcp"));
        if (auto res = open.submit_and_wait()) {
            std::println("Client socket created: {}", res->native_handle());
            return std::move(*res);
        } else {
            exit_if_function_not_supported(res.error());
            std::println("Failed to create client socket: {}", res.error().message());
            std::abort();
        }
    }();

    // For some reason setsockopt op may not work
    bool sockcmd_exists = true;

    [&ring, &sock, &sockcmd_exists] {
        using op = network::socket_setoption<network::sockopts::general::reuseaddr>;
        auto setsockopt = ring.make_sync<op::operation>();
        setsockopt.socket(sock)
            .option(true);
        if (auto res = setsockopt.submit_and_wait()) {
            std::println("Socket option SO_REUSEADDR set");
        } else {
            if (res.error() == std::errc::function_not_supported
                || res.error() == std::errc::operation_not_supported) {
                std::println("Socket option operation not supported, fallback to system call");
                sockcmd_exists = false;
                return;
            }
            std::println("Failed to set socket option SO_REUSEADDR: {}", res.error().message());
            std::abort();
        }
    }();

    if (!sockcmd_exists) {
        int optval = 1;
        int resopt = ::setsockopt(sock.native_handle(), SOL_SOCKET, SO_REUSEADDR,
            &optval, sizeof(optval));
        if (resopt < 0) {
            std::println("Failed to set socket options: {}", std::strerror(errno));
            ::close(sock.native_handle());
            return;
        }
    }
    
    // Not until kernel 6.11 do io_uring have op bind and op listen
    bool bind_op_exists = true;

    // optional bind client local address (useful to show symmetry)
    [&ring, &sock, &bind_op_exists] {
        auto bind = ring.make_sync<network::socket_bind_operation>();
        bind.socket(sock)
            .socket_info(client_addr);
        if (auto res = bind.submit_and_wait()) {
            std::println("Client socket bound successfully");
        } else {
            if (res.error() == std::errc::function_not_supported
                || res.error() == std::errc::operation_not_supported) {
                std::println("Socket bind operation not supported, fallback to system call");
                bind_op_exists = false;
                return;
            }
            std::println("Failed to bind client socket: {}", res.error().message());
            std::abort();
        }
    }();

    if (!bind_op_exists) {
        ::sockaddr_in addr = client_addr.to_system_sockaddr();
        if (::bind(sock.native_handle(),
                reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            std::println("Failed to bind client socket: {}", std::strerror(errno));
            std::abort();
        }
        std::println("Client socket bound successfully");
    }

    // connect
    [&ring, &sock] {
        auto connect = ring.make_sync<network::socket_connect_operation>();
        connect.socket(sock)
            .peer_socket_info(server_addr);
        if (auto res = connect.submit_and_wait()) {
            std::println("Connected to server");
        } else {
            exit_if_function_not_supported(res.error());
            std::println("Connect failed: {}", res.error().message());
            std::abort();
        }
    }();

    // send messages and receive echoes
    std::vector<std::byte> txbuf(client_msg.size());
    std::memcpy(txbuf.data(), client_msg.data(), client_msg.size());
    std::vector<std::byte> rxbuf(4096);
    for (std::size_t i = 0; i < client_msg_cnt; ++i) {
        // send
        [&ring, &sock, &txbuf, i] {
            auto send = ring.make_sync<network::socket_send_operation>();
            send.socket(sock)
                .buffer(txbuf);
            if (auto res = send.submit_and_wait()) {
                std::println("Sent {} bytes (msg #{})", *res, i + 1);
            } else {
                exit_if_function_not_supported(res.error());
                std::println("Send failed: {}", res.error().message());
                std::abort();
            }
        }();
        // recv
        [&ring, &sock, &rxbuf, i] {
            std::println("Waiting for echo (msg #{})...", i + 1);
            auto recv = ring.make_sync<network::socket_recv_operation>();
            recv.socket(sock)
                .buffer(rxbuf);
            if (auto res = recv.submit_and_wait()) {
                std::string_view message(start_lifetime_as<char>(*res));
                std::println("Received {} bytes (msg #{}): '{}'",
                    message.size(), i + 1, message);
            } else {
                exit_if_function_not_supported(res.error());
                std::println("Receive failed: {}", res.error().message());
                std::abort();
            }
        }();
    }

    // send magic word to ask server to exit
    [&] {
        std::vector<std::byte> mw(magic_word.size());
        std::memcpy(mw.data(), magic_word.data(), magic_word.size());
        auto send = ring.make_sync<network::socket_send_operation>();
        send.socket(sock)
            .buffer(std::span(mw));
        if (auto res = send.submit_and_wait()) {
            std::println("Sent magic word, {} bytes", *res);
        } else {
            exit_if_function_not_supported(res.error());
            std::println("Send magic word failed: {}", res.error().message());
            std::abort();
        }
    }();

    // shutdown + close
    [&ring, &sock] {
        auto shutdown = ring.make_sync<network::socket_shutdown_operation>();
        shutdown.socket(sock)
            .options(network::shutdown_option::rdwr);
        if (auto res = shutdown.submit_and_wait()) {
            std::println("Client shutdown successfully");
        } else {
            exit_if_function_not_supported(res.error());
            std::println("Client shutdown failed: {}", res.error().message());
            std::abort();
        }
    }();

    [&ring, &sock] {
        auto close = ring.make_sync<network::socket_close_operation>();
        close.socket(sock);
        if (auto res = close.submit_and_wait()) {
            std::println("Socket closed successfully");
        } else {
            exit_if_function_not_supported(res.error());
            std::println("Failed to close socket: {}", res.error().message());
            std::abort();
        }
    }();
}

int main() {
    if (network::to_protocol("tcp") == network::socket_config::protocol::unknown) {
        std::println("Protocol database not initialized, aborting");
        std::abort();
    }
    std::jthread srv(&echo_server);
    // wait until server has called listen and set the flag
    while (!server_started.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(10ms);
    }
    echo_client();
}
