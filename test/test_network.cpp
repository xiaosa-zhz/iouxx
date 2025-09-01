#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <vector>
#include <span>
#include <array>
#include <thread>
#include <atomic>
#include <string_view>
#include <print>
#include <system_error>

// This tests may be skiped if certain features are not supported
#ifndef IOUXX_CONFIG_ENABLE_FEATURE_TESTS
#define IOUXX_CONFIG_ENABLE_FEATURE_TESTS
#endif // IOUXX_CONFIG_ENABLE_FEATURE_TESTS

#include "iouringxx.hpp"
#include "iouops/network/ip.hpp"
#include "iouops/network/socket.hpp"
#include "iouops/network/socketio.hpp"

using namespace iouxx;
using namespace std::literals;
using namespace iouxx::literals;
inline constexpr std::string_view magic_word = "exit"sv;
inline constexpr std::string_view client_msg = "Hello io_uring!"sv;
inline constexpr std::size_t client_msg_cnt = 10;
inline constexpr network::ip::socket_v4_info server_addr = "127.0.0.1:16381"_sockv4;
inline constexpr network::ip::socket_v4_info client_addr = "127.0.0.1:16382"_sockv4;
static std::atomic<bool> server_started = false; // publish after listen is ready

inline void exit_if_function_not_supported(const std::error_code& ec) noexcept {
    if (ec && ec == std::errc::function_not_supported) {
        std::println("Encountered unsupported io_uring opcode, treat as success");
        std::exit(0);
    }
}

template<class T>
[[nodiscard]] T* start_lifetime_as_array(void* p, std::size_t n) noexcept {
    std::array<std::byte, sizeof(T)> rep = {};
    std::byte* origin = static_cast<std::byte*>(p);
    T* ptr = static_cast<T*>(p);
    for (std::size_t i = 0; i < n; ++i) {
        std::memcpy(rep.data(), origin + i * sizeof(T), sizeof(T));
        new (ptr + i) T(std::bit_cast<T>(rep));
    }
    return std::launder(ptr);
}

void echo_server() {
    io_uring_xx ring(256);
    network::socket sock = [&ring] {
        network::socket sock;
        auto open = ring.make<network::socket_open_operation>(
            [&sock](std::expected<network::socket, std::error_code> res) {
            if (res) {
                std::println("Server socket created: {}", res->native_handle());
                sock = *res;
            } else {
                std::println("Failed to create server socket: {}", res.error().message());
                std::abort();
            }
        });
        open.domain(network::socket::domain::ipv4)
            .type(network::socket::type::stream)
            .protocol(network::to_protocol("tcp"));
        auto ec = open.submit();
        exit_if_function_not_supported(ec);
        if (ec) {
            std::println("Failed to submit open operation: {}", ec.message());
            std::abort();
        }
        if (auto res = ring.wait_for_result(100ms)) {
            res->callback();
        } else {
            std::println("Failed to wait for result: {}", res.error().message());
            std::abort();
        }
        return sock;
    }();
    // Test manual, blocking bind by io_uring
    // {
    //     struct system_addrsock_info {
    //         ::sockaddr* addr;
    //         std::size_t addrlen;
    //     };
    //     alignas(std::max_align_t) unsigned char sockaddr_buf[128];
    //     auto [addr, addrlen] = system_addrsock_info{
    //         .addr = reinterpret_cast<::sockaddr*>(
    //             new (&sockaddr_buf) auto(server_addr.to_system_sockaddr())
    //         ),
    //         .addrlen = sizeof(server_addr.to_system_sockaddr())
    //     };
    //     // if (bind(sock.native_handle(), addr, addrlen) < 0) {
    //     //     std::println("Bind operation failed: {}", std::strerror(errno));
    //     //     std::abort();
    //     // } else {
    //     //     std::println("Bind operation successful");
    //     //     std::exit(0);
    //     // }
    //     ::io_uring_sqe* sqe = ::io_uring_get_sqe(&ring.native());
    //     if (!sqe) {
    //         std::println("Failed to get SQE for bind operation");
    //         std::abort();
    //     }
    //     ::io_uring_prep_bind(sqe, sock.native_handle(), addr, addrlen);
    //     int ev = ::io_uring_submit(&ring.native());
    //     if (ev < 0) {
    //         std::println("Failed to submit bind operation: {}", std::strerror(-ev));
    //         std::abort();
    //     }
    //     ::io_uring_cqe* cqe = nullptr;
    //     ev = ::io_uring_wait_cqe(&ring.native(), &cqe);
    //     if (ev < 0) {
    //         std::println("Failed to wait for bind result: {}", std::strerror(-ev));
    //         std::abort();
    //     } else {
    //         if (cqe->res < 0) {
    //             std::println("Bind operation failed: {}", std::strerror(-cqe->res));
    //             std::abort();
    //         } else {
    //             std::println("Socket bound successfully");
    //         }
    //         ::io_uring_cqe_seen(&ring.native(), cqe);
    //     }
    // }
    [&ring, &sock] {
        auto bind = ring.make<network::socket_bind_operation>(
            [](std::error_code ec) {
            if (ec) {
                std::println("Failed to bind socket: {}", ec.message());
                std::abort();
            } else {
                std::println("Socket bound successfully");
            }
        });
        bind.socket(sock)
            .socket_info(server_addr);
        auto ec = bind.submit();
        exit_if_function_not_supported(ec);
        if (ec) {
            std::println("Failed to submit bind operation: {}", ec.message());
            std::abort();
        }
        if (auto res = ring.wait_for_result(100ms)) {
            res->callback();
        } else {
            std::println("Failed to wait for result: {}", res.error().message());
            std::abort();
        }
    }();
    [&ring, &sock] {
        auto listen = ring.make<network::socket_listen_operation>(
            [](std::error_code ec) {
            if (ec) {
                std::println("Failed to listen on socket: {}", ec.message());
                std::abort();
            } else {
                std::println("Socket is now listening");
            }
        });
        listen.socket(sock).backlog(128);
        auto ec = listen.submit();
        exit_if_function_not_supported(ec);
        if (ec) {
            std::println("Failed to submit listen operation: {}", ec.message());
            std::abort();
        }
        if (auto res = ring.wait_for_result(100ms)) {
            res->callback();
        } else {
            std::println("Failed to wait for result: {}", res.error().message());
            std::abort();
        }
        server_started.store(true, std::memory_order_release);
    }();
    network::connection connection = [&ring, &sock] {
        network::connection connection;
        auto accept = ring.make<network::socket_accept_operation>(
            [&connection](std::expected<network::connection, std::error_code> res) {
            if (res) {
                std::println("Accepted connection: {}", res->native_handle());
                connection = *res;
            } else {
                std::println("Failed to accept connection: {}", res.error().message());
                std::abort();
            }
        });
        auto ec = accept.submit();
        exit_if_function_not_supported(ec);
        if (ec) {
            std::println("Failed to submit accept operation: {}", ec.message());
            std::abort();
        }
        if (auto res = ring.wait_for_result(100ms)) {
            res->callback();
        } else {
            std::println("Failed to wait for result: {}", res.error().message());
            std::abort();
        }
        return connection;
    }();
    std::vector<std::byte> buffer(4096);
    while (true) {
        std::size_t received = 0;
        bool should_exit = false;
        auto recv = ring.make<network::socket_recv_operation>(
            [&buffer, &received, &should_exit]
            (std::expected<std::size_t, std::error_code> res) {
            if (res) {
                received = *res;
                std::string_view str(
                    start_lifetime_as_array<char>(buffer.data(), received),
                    received);
                std::println("Received {} bytes: '{}'", received, str);
                if (str == magic_word) {
                    should_exit = true;
                }
            } else {
                std::println("Failed to receive data: {}", res.error().message());
            }
        });
        recv.socket(connection)
            .buffer(std::span(buffer).subspan(0, 4095));
        auto ec = recv.submit();
        exit_if_function_not_supported(ec);
        if (ec) {
            std::println("Failed to submit recv operation: {}", ec.message());
            std::abort();
        }
        if (auto res = ring.wait_for_result(100ms)) {
            res->callback();
        } else {
            std::println("Failed to wait for result: {}", res.error().message());
            std::abort();
        }

        if (should_exit) {
            std::println("Magic word received, exiting...");
            break;
        }

        auto send = ring.make<network::socket_send_operation>(
            [&buffer](std::expected<std::size_t, std::error_code> res) {
            if (res) {
                std::println("Sent {} bytes", *res);
            } else {
                std::println("Failed to send data: {}", res.error().message());
            }
        });
        send.socket(connection)
            .buffer(std::span(buffer).subspan(0, received));
        ec = send.submit();
        exit_if_function_not_supported(ec);
        if (ec) {
            std::println("Failed to submit send operation: {}", ec.message());
            std::abort();
        }
        if (auto res = ring.wait_for_result(100ms)) {
            res->callback();
        } else {
            std::println("Failed to wait for result: {}", res.error().message());
            std::abort();
        }
    }

    [&ring, &connection] {
        auto shutdown = ring.make<network::socket_shutdown_operation>(
            [](std::error_code ec) {
            if (ec) {
                std::println("Failed to shutdown socket: {}", ec.message());
            } else {
                std::println("Socket shutdown successfully");
            }
        });
        shutdown.connection(connection)
            .options(network::shutdown_option::rdwr);
        auto ec = shutdown.submit();
        exit_if_function_not_supported(ec);
        if (ec) {
            std::println("Failed to submit shutdown operation: {}", ec.message());
            std::abort();
        }
        if (auto res = ring.wait_for_result(100ms)) {
            res->callback();
        } else {
            std::println("Failed to wait for result: {}", res.error().message());
            std::abort();
        }
    }();

    [&ring, &sock] {
        auto close = ring.make<network::socket_close_operation>(
            [](std::error_code ec) {
            if (ec) {
                std::println("Failed to close socket: {}", ec.message());
            } else {
                std::println("Socket closed successfully");
            }
        });
        close.socket(sock);
        auto ec = close.submit();
        exit_if_function_not_supported(ec);
        if (ec) {
            std::println("Failed to submit close operation: {}", ec.message());
            std::abort();
        }
        if (auto res = ring.wait_for_result(100ms)) {
            res->callback();
        } else {
            std::println("Failed to wait for result: {}", res.error().message());
            std::abort();
        }
    }();
}

void echo_client() {
    io_uring_xx ring(256);
    network::socket sock = [&ring] {
        network::socket sock;
        auto open = ring.make<network::socket_open_operation>(
            [&sock](std::expected<network::socket, std::error_code> res) {
            if (res) {
                std::println("Client socket created: {}", res->native_handle());
                sock = *res;
            } else {
                std::println("Client socket create failed: {}", res.error().message());
                std::abort();
            }
        });
        open.domain(network::socket::domain::ipv4)
            .type(network::socket::type::stream)
            .protocol(network::to_protocol("tcp"));
        if (auto ec = open.submit()) {
            exit_if_function_not_supported(ec);
            std::println("Submit open failed: {}", ec.message());
            std::abort();
        }
        if (auto r = ring.wait_for_result(100ms)) {
            r->callback();
        } else {
            std::println("Wait open failed: {}", r.error().message());
            std::abort();
        }
        return sock;
    }();

    // optional bind client local address (useful to show symmetry)
    [&ring, &sock] {
        auto bind = ring.make<network::socket_bind_operation>(
            [](std::error_code ec) {
            if (ec) {
                std::println("Client bind failed: {}",
                    ec.message());
                    std::abort();
                }
            else {
                std::println("Client bound");
            }
        });
        bind.socket(sock)
            .socket_info(client_addr);
        if (auto ec = bind.submit()) {
            exit_if_function_not_supported(ec);
            std::println("Submit client bind failed: {}", ec.message());
            std::abort();
        }
        if (auto r = ring.wait_for_result(100ms)) {
            r->callback();
        } else {
            std::println("Wait client bind failed: {}", r.error().message());
            std::abort();
        }
    }();

    // connect
    [&ring, &sock] {
        auto connect = ring.make<network::socket_connect_operation>(
            [](std::error_code ec) {
                if (ec) {
                    std::println("Connect failed: {}", ec.message());
                    std::abort();
                }
                else {
                    std::println("Connected to server");
                }
            });
        connect.socket(sock)
            .socket_info(server_addr);
        if (auto ec = connect.submit()) {
            exit_if_function_not_supported(ec);
            std::println("Submit connect failed: {}", ec.message());
            std::abort();
        }
        if (auto r = ring.wait_for_result(1s)) {
            r->callback();
        } else {
            std::println("Wait connect failed: {}", r.error().message());
            std::abort();
        }
    }();

    // send messages and receive echoes
    std::vector<std::byte> txbuf(client_msg.size());
    std::memcpy(txbuf.data(), client_msg.data(), client_msg.size());
    std::vector<std::byte> rxbuf(4096);
    for (std::size_t i = 0; i < client_msg_cnt; ++i) {
        // send
        [&] {
            auto send = ring.make<network::socket_send_operation>(
            [](std::expected<std::size_t, std::error_code> res) {
                if (!res) {
                    std::println("Client send failed: {}", res.error().message());
                    std::abort();
                }
                else {
                    std::println("Client sent {} bytes", *res);
                }
            });
            send.socket(sock)
                .buffer(std::span(txbuf).subspan(0, client_msg.size()));
            if (auto ec = send.submit()) {
                exit_if_function_not_supported(ec);
                std::println("Submit client send failed: {}", ec.message());
                std::abort();
            }
            if (auto r = ring.wait_for_result(100ms)) {
                r->callback();
            } else {
                std::println("Wait client send failed: {}", r.error().message());
                std::abort();
            }
        }();
        // receive echo
        [&] {
            auto recv = ring.make<network::socket_recv_operation>(
            [&rxbuf](std::expected<std::size_t, std::error_code> res) {
                if (!res) {
                    std::println("Client recv failed: {}", res.error().message());
                    std::abort();
                }
                else {
                    std::size_t received = *res;
                    std::string_view str(
                        start_lifetime_as_array<char>(rxbuf.data(), received),
                        received);
                    std::println("Client received {} bytes: '{}'", received, str);
                }
            });
            recv.socket(sock)
                .buffer(std::span(rxbuf).subspan(0, 4095));
            if (auto ec = recv.submit()) {
                exit_if_function_not_supported(ec);
                std::println("Submit client recv failed: {}", ec.message());
                std::abort();
            }
            if (auto r = ring.wait_for_result(500ms)) {
                r->callback();
            } else {
                std::println("Wait client recv failed: {}", r.error().message());
                std::abort();
            }
        }();
    }

    // send magic word to ask server to exit
    [&] {
        std::vector<std::byte> mw(magic_word.size());
        std::memcpy(mw.data(), magic_word.data(), magic_word.size());
        auto send = ring.make<network::socket_send_operation>(
            [](std::expected<std::size_t, std::error_code> res) {
            if (!res) {
                std::println("Send magic word failed: {}", res.error().message());
                std::abort();
            }
            else {
                std::println("Magic word sent ({} bytes)", *res);
            }
        });
        send.socket(sock).buffer(std::span(mw));
        if (auto ec = send.submit()) {
            exit_if_function_not_supported(ec);
            std::println("Submit magic word send failed: {}", ec.message());
            std::abort();
        }
        if (auto r = ring.wait_for_result(100ms)) {
            r->callback();
        } else {
            std::println("Wait magic word send failed: {}", r.error().message());
            std::abort();
        }
    }();

    // shutdown + close
    [&ring, &sock] {
        auto shutdown = ring.make<network::socket_shutdown_operation>(
            [](std::error_code ec) {
            if (ec) {
                std::println("Client shutdown failed: {}", ec.message());
            } else {
                std::println("Client shutdown OK");
            }
        });
        shutdown.connection(network::connection(sock, sock.native_handle()))
            .options(network::shutdown_option::rdwr);
        if (auto ec = shutdown.submit()) {
            exit_if_function_not_supported(ec);
            std::println("Submit client shutdown failed: {}", ec.message());
        }
        if (auto r = ring.wait_for_result(100ms)) {
            r->callback();
        } else {
            std::println("Wait client shutdown failed: {}", r.error().message());
        }
    }();

    [&ring, &sock] {
        auto close = ring.make<network::socket_close_operation>(
            [](std::error_code ec) {
            if (ec) std::println("Client close failed: {}", ec.message());
            else std::println("Client closed");
        });
        close.socket(sock);
        if (auto ec = close.submit()) {
            exit_if_function_not_supported(ec);
            std::println("Submit client close failed: {}", ec.message());
        }
        if (auto r = ring.wait_for_result(100ms)) {
            r->callback();
        } else {
            std::println("Wait client close failed: {}", r.error().message());
        }
    }();
}

int main() {
    if (network::to_protocol("tcp") == network::socket::protocol::unknown) {
        std::println("Protocol database not initialized, aborting");
        std::abort();
    }
    std::thread srv(&echo_server);
    // wait until server has called listen and set the flag
    while (!server_started.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(10ms);
    }
    echo_client();
    srv.join();
}
