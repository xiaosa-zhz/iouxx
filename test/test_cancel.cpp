#include <chrono>
#include <print>
#include <system_error>
#include <thread>

#include "iouringxx.hpp"
#include "iouops/timeout.hpp"
#include "iouops/cancel.hpp"

#define TEST_EXPECT(...) do { \
    if (!(__VA_ARGS__)) { \
        std::println("Assertion failed: {}, {}:{}\n", #__VA_ARGS__, \
        __FILE__, __LINE__); \
        std::exit(-(__COUNTER__ + 1)); \
    } \
} while(0)

void test_cancel() {
    using namespace std::literals;
    iouxx::io_uring_xx ring(64);
    bool timeout_result = false;
    iouxx::timeout_operation timer(ring,
        [&timeout_result](std::error_code ec) {
        if (ec == std::errc::operation_canceled) {
            std::println("Timer cancelled: {}", ec.message());
            timeout_result = true;
        } else {
            std::println("Timer expired!");
        }
    });
    timer.wait_for(100ms);
    auto ec = timer.submit();
    TEST_EXPECT(!ec);
    std::this_thread::sleep_for(10ms);
    bool cancel_result = false;
    iouxx::cancel_operation cancel(ring,
        [&cancel_result](std::error_code ec) {
        if (ec) {
            std::println("Cancel failed: {}", ec.message());
        } else {
            std::println("Cancel succeeded");
            cancel_result = true;
        }
    });
    cancel.target(timer.identifier());
    ec = cancel.submit();
    TEST_EXPECT(!ec);
    ring.wait_for_result().value()();
    ring.wait_for_result().value()();
    TEST_EXPECT(timeout_result && cancel_result);
}

int main() {
    test_cancel();
}
