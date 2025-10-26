#include "iouxx/clock.hpp"
#ifdef IOUXX_CONFIG_USE_CXX_MODULE

import std;
import iouxx.ring;
import iouxx.ops.timeout;

#else // !IOUXX_CONFIG_USE_CXX_MODULE

#include <chrono>
#include <cstdlib>
#include <print>

#include "iouxx/iouringxx.hpp"
#include "iouxx/iouops/timeout.hpp"

#endif // IOUXX_CONFIG_USE_CXX_MODULE

#define TEST_EXPECT(...) do { \
    if (!(__VA_ARGS__)) { \
        std::println("Assertion failed: {}, {}:{}.", #__VA_ARGS__, \
        __FILE__, __LINE__); \
        std::exit(-(__COUNTER__ + 1)); \
    } \
} while(0)

void test_timeout() {
    using namespace std::literals;
    iouxx::ring ring(64);
    auto timer = ring.make_sync<iouxx::timeout_operation>();
    timer.wait_for(50ms, iouxx::boottime_clock{});
    auto start = std::chrono::steady_clock::now();
    if (auto res = timer.submit_and_wait()) {
        std::println("Timer expired!");
    } else {
        std::println("Failed to submit timer task: {}.", res.error().message());
        TEST_EXPECT(false);
    }
    auto end = std::chrono::steady_clock::now();
    auto duration = end - start;
    TEST_EXPECT(duration < 100ms);
    std::println("Timer completed after {}.",
        std::chrono::duration_cast<std::chrono::milliseconds>(duration));
}

void test_multishot_timeout() {
    using namespace std::literals;
    iouxx::ring ring(64);
    bool if_more = true;
    int counter = 0;
    iouxx::multishot_timeout_operation timer(ring,
        [&if_more, &counter](std::expected<bool, std::error_code> res) {
        ++counter;
        TEST_EXPECT(res);
        bool more = *res;
        if (more) {
            std::println("Timer expired, and more!");
        } else {
            std::println("Timer expired, no more.");
            if_more = false;
        }
    });
    timer.wait_for(10ms);
    timer.repeat(5);
    auto start = iouxx::boottime_clock::now();
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
    auto end = iouxx::boottime_clock::now();
    auto duration = end - start;
    TEST_EXPECT(duration < 100ms);
    std::println("Timer completed after {}.",
        std::chrono::duration_cast<std::chrono::milliseconds>(duration));
}

void test_ring_stop() {
    using namespace std::literals;
    iouxx::ring ring(64);
    auto dummy_callback = [](std::error_code) static noexcept {};
    auto timer1 = ring.make<iouxx::timeout_operation>(dummy_callback);
    timer1.wait_for(1s);
    TEST_EXPECT(!timer1.submit());
    auto timer2 = ring.make<iouxx::timeout_operation>(dummy_callback);
    timer2.wait_for(2s);
    TEST_EXPECT(!timer2.submit());
    auto res = ring.stop();
    TEST_EXPECT(res != std::make_error_code(std::errc::invalid_argument));
    std::println("Ring stopped");
}

int main() {
    TEST_EXPECT(true);
    test_timeout();
    test_multishot_timeout();
    test_ring_stop();
}
