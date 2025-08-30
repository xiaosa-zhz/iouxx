#include <chrono>
#include <cstdlib>
#include <print>

#include "iouringxx.hpp"
#include "iouops/timeout.hpp"

#define TEST_EXPECT(...) do { \
    if (!(__VA_ARGS__)) { \
        std::println("Assertion failed: {}, {}:{}\n", #__VA_ARGS__, \
        __FILE__, __LINE__); \
        std::exit(-(__COUNTER__ + 1)); \
    } \
} while(0)

void test_timeout() {
    using namespace std::literals;
    iouxx::io_uring_xx ring(64);
    int n = 0;
    iouxx::timeout_operation timer(ring,
        [&n](std::error_code ec) {
        std::println("Timer expired!");
        n = 114514;
    });
    timer.wait_for(50ms);
    auto start = std::chrono::steady_clock::now();
    if (auto ec = timer.submit()) {
        std::println("Failed to submit timer task: {}", ec.message());
        TEST_EXPECT(false);
    }
    std::println("Timer task submitted, waiting for completion...");
    iouxx::operation_result result = ring.wait_for_result().value();
    result();
    TEST_EXPECT(n == 114514);
    auto end = std::chrono::steady_clock::now();
    auto duration = end - start;
    TEST_EXPECT(duration < 100ms);
    std::println("Timer completed after {}",
        std::chrono::duration_cast<std::chrono::milliseconds>(duration));
}

void test_multishot_timeout() {
    using namespace std::literals;
    iouxx::io_uring_xx ring(64);
    bool if_more = true;
    int counter = 0;
    iouxx::multishot_timeout_operation timer(ring,
        [&if_more, &counter](std::error_code ec, bool more) {
        ++counter;
        if (more) {
            std::println("Timer expired, and more!");
        } else {
            std::println("Timer expired, no more.");
            if_more = false;
        }
    });
    timer.wait_for(10ms);
    timer.repeat(5);
    auto start = std::chrono::steady_clock::now();
    if (auto ec = timer.submit()) {
        std::println("Failed to submit timer task: {}", ec.message());
        TEST_EXPECT(false);
    }
    std::println("Timer task submitted, waiting for completion...");
    while (if_more) {
        iouxx::operation_result result = ring.wait_for_result().value();
        result();
    }
    TEST_EXPECT(counter == 5);
    auto end = std::chrono::steady_clock::now();
    auto duration = end - start;
    TEST_EXPECT(duration < 100ms);
    std::println("Timer completed after {}",
        std::chrono::duration_cast<std::chrono::milliseconds>(duration));
}

int main() {
    TEST_EXPECT(true);
    test_timeout();
    test_multishot_timeout();
}
