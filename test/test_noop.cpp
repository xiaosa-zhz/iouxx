#include <print>
#include <system_error>

#include "iouringxx.hpp"
#include "iouops/noop.hpp"

#define test_expect(...) do { \
    if (!(__VA_ARGS__)) { \
        std::println("Assertion failed: {}, {}:{}\n", #__VA_ARGS__, \
        __FILE__, __LINE__); \
        std::exit(-(__COUNTER__ + 1)); \
    } \
} while(0)

void test_noop() {
    using namespace std::literals;
    iouxx::io_uring_xx ring(64);
    int n = 0;
    iouxx::noop_operation noop(ring,
    [&n](std::error_code ec) {
        std::println("Noop completed!");
        n = 114514;
    });
    if (auto ec = noop.submit()) {
        std::println("Failed to submit noop task: {}", ec.message());
        test_expect(false);
    }
    std::println("Noop task submitted, waiting for completion...");
    iouxx::operation_result result = ring.wait_for_result().value();
    result();
    test_expect(n == 114514);
    std::println("Noop completed with result: {}", result.result());
}

int main() {
    test_expect(true);
    test_noop();
}
