#ifdef IOUXX_CONFIG_USE_CXX_MODULE

import std;
import iouxx.ring;

#else // !IOUXX_CONFIG_USE_CXX_MODULE

#include <thread>
#include <array>
#include <string_view>
#include <print>
#include <cstring>
#include <cstddef>

#include "iouxx/iouringxx.hpp"

#endif // IOUXX_CONFIG_USE_CXX_MODULE

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>

// This file is to make clangd work

void echo_server() {
    int sockfd = ::socket(AF_INET, SOCK_STREAM, 6);
    if (sockfd < 0) {
        std::println("Failed to create socket: {}", std::strerror(errno));
        return;
    }
    ::sockaddr_in addr{
        .sin_family = AF_INET,
        .sin_port = htons(8080),
        .sin_addr = { htonl(INADDR_LOOPBACK) },
    };
    int optval = 1;
    int resopt = ::setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
        &optval, sizeof(optval));
    if (resopt < 0) {
        std::println("Failed to set socket options: {}", std::strerror(errno));
        ::close(sockfd);
        return;
    }
    int res0 = ::bind(sockfd, (::sockaddr*) &addr, sizeof(addr));
    if (res0 < 0) {
    std::println("Failed to bind socket: {}", std::strerror(errno));
        return;
    }
    int res1 = ::listen(sockfd, 128);
    if (res1 < 0) {
    std::println("Failed to listen on socket: {}", std::strerror(errno));
        ::close(sockfd);
        return;
    }
    std::array<unsigned char, sizeof(::sockaddr_in)> addr_buffer;
    ::socklen_t len = 28;
    int fd = ::accept4(sockfd,
        (::sockaddr*) addr_buffer.data(), &len, SOCK_CLOEXEC);
    if (fd < 0) {
        std::println("Failed to accept connection: {}", std::strerror(errno));
        ::close(sockfd);
        return;
    }
    std::println("Peer: {} {::x}", len, addr_buffer);
    std::array<char, 1024> buffer{};
    while (true) {
        int size = ::recv(fd, buffer.data(), buffer.size() - 1, 0);
        if (size <= 0) {
            break;
        }
        std::string_view msg(buffer.data(), size);
        std::println("Received message from client: {}", msg);
        if (msg == "exit") {
            break;
        }
        int res = ::send(fd, buffer.data(), size, 0);
        if (res < 0) {
            std::println("Failed to send echo: {}", std::strerror(errno));
            break;
        }
    }
    int res2 = ::shutdown(fd, SHUT_RDWR);
    if (res2 < 0) {
        std::println("Failed to shutdown connection: {}", std::strerror(errno));
    }
    int res3 = ::close(fd);
    if (res3 < 0) {
        std::println("Failed to close connection: {}", std::strerror(errno));
    }
}

void echo_client() {
    int sockfd = ::socket(AF_INET, SOCK_STREAM, 6);
    ::sockaddr_in addr{
        .sin_family = AF_INET,
        .sin_port = htons(8081),
        .sin_addr = { htonl(INADDR_LOOPBACK) },
    };
    int optval = 1;
    int resopt = ::setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
        &optval, sizeof(optval));
    if (resopt < 0) {
        std::println("Failed to set socket options: {}", std::strerror(errno));
        ::close(sockfd);
        return;
    }
    int res0 = ::bind(sockfd, (::sockaddr*) &addr, sizeof(addr));
    if (res0 < 0) {
    std::println("Failed to bind client socket: {}", std::strerror(errno));
        return;
    }
    ::sockaddr_in server_addr{
        .sin_family = AF_INET,
        .sin_port = htons(8080),
        .sin_addr = { htonl(INADDR_LOOPBACK) },
    };
    int res1 = ::connect(sockfd, (::sockaddr*) &server_addr, sizeof(server_addr));
    if (res1 < 0) {
    std::println("Failed to connect to server: {}", std::strerror(errno));
        ::close(sockfd);
        return;
    }
    std::string_view echo_msg = "Hello io_uring!";
    std::array<char, 1024> buffer{};
    for (int i = 0; i < 10; ++i) {
        int _ = ::send(sockfd, echo_msg.data(), echo_msg.size(), 0);
        int size = ::recv(sockfd, buffer.data(), buffer.size() - 1, 0);
        std::string_view msg(buffer.data(), size);
        std::println("Received echo: {}", msg);
    }
    int res2 = ::send(sockfd, "exit", 4, 0);
    if (res2 < 0) {
    std::println("Failed to send exit message: {}", std::strerror(errno));
    }
    int res3 = ::shutdown(sockfd, SHUT_RDWR);
    if (res3 < 0) {
    std::println("Failed to shutdown socket: {}", std::strerror(errno));
    }
    int res4 = ::close(sockfd);
    if (res4 < 0) {
    std::println("Failed to close socket: {}", std::strerror(errno));
    }
}

static_assert(iouxx::ring::check_version("2.10"));
constexpr auto version = iouxx::ring::version();
static_assert(version.major == 2 && version.minor == 9);
static_assert(iouxx::ring::check_version("asodasd"));

int main() {
    std::println("liburing version: {}", version);
    auto ver = iouxx::ring::version_info::current();
    std::println("liburing runtime version: {:#}", ver);
    using namespace std::literals;
    std::jthread server_thread(&echo_server);
    std::this_thread::sleep_for(1s); // wait for server to start
    echo_client();
}
