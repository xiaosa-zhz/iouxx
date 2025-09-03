#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <thread>
#include <array>
#include <string_view>
#include <print>

void echo_server() {
    int sockfd = ::socket(AF_INET, SOCK_STREAM, 6);
    if (sockfd < 0) {
        std::println("Failed to create socket");
        return;
    }
    ::sockaddr_in addr{
        .sin_family = AF_INET,
        .sin_port = htons(8080),
        .sin_addr = { htonl(INADDR_LOOPBACK) },
    };
    int res0 = ::bind(sockfd, (::sockaddr*) &addr, sizeof(addr));
    if (res0 < 0) {
        std::println("Failed to bind socket");
        return;
    }
    int res1 = ::listen(sockfd, 128);
    if (res1 < 0) {
        std::println("Failed to listen on socket");
        ::close(sockfd);
        return;
    }
    int fd = ::accept4(sockfd, nullptr, nullptr, SOCK_CLOEXEC);
    if (fd < 0) {
        std::println("Failed to accept connection");
        ::close(sockfd);
        return;
    }
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
            std::println("Failed to send echo");
            break;
        }
    }
    int _ = ::shutdown(fd, SHUT_RDWR);
    int _ = ::close(fd);
}

void echo_client() {
    int sockfd = ::socket(AF_INET, SOCK_STREAM, 6);
    ::sockaddr_in addr{
        .sin_family = AF_INET,
        .sin_port = htons(8081),
        .sin_addr = { htonl(INADDR_LOOPBACK) },
    };
    int _ = ::bind(sockfd, (::sockaddr*) &addr, sizeof(addr));
    ::sockaddr_in server_addr{
        .sin_family = AF_INET,
        .sin_port = htons(8080),
        .sin_addr = { htonl(INADDR_LOOPBACK) },
    };
    int _ = ::connect(sockfd, (::sockaddr*) &server_addr, sizeof(server_addr));
    std::string_view echo_msg = "Hello io_uring!";
    std::array<char, 1024> buffer{};
    for (int i = 0; i < 10; ++i) {
        int _ = ::send(sockfd, echo_msg.data(), echo_msg.size(), 0);
        int size = ::recv(sockfd, buffer.data(), buffer.size() - 1, 0);
        std::string_view msg(buffer.data(), size);
        std::println("Received echo: {}", msg);
    }
    int _ = ::send(sockfd, "exit", 4, 0);
    int _ = ::shutdown(sockfd, SHUT_RDWR);
    int _ = ::close(sockfd);
}

int main() {
    using namespace std::literals;
    std::jthread server_thread(&echo_server);
    std::this_thread::sleep_for(1s); // wait for server to start
    echo_client();
}
