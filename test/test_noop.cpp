#include <system_error>
#include <print>

#include "iouringxx.hpp"
#include "iouops/noop.hpp"

#define TEST_EXPECT(...) do { \
    if (!(__VA_ARGS__)) { \
        std::println("Assertion failed: {}, {}:{}\n", #__VA_ARGS__, \
        __FILE__, __LINE__); \
        std::exit(-(__COUNTER__ + 1)); \
    } \
} while(0)

struct pinned_callback
{
    pinned_callback(const pinned_callback&) = delete;
    pinned_callback& operator=(const pinned_callback&) = delete;
    pinned_callback(pinned_callback&&) = delete;
    pinned_callback& operator=(pinned_callback&&) = delete;

    explicit pinned_callback(int a, int b, int c) noexcept
        : x(a), y(b), z(c)
    {}

    void operator()(std::error_code ec) noexcept {
        std::println("Pinned callback called with ec: {}", ec.message());
        if (!ec) {
            std::println("Pinned callback data: {}, {}, {}", x, y, z);
        }
    }

    int x, y, z;
};

void test_noop() {
    using namespace std::literals;
    using namespace iouxx;
    io_uring_xx ring(64);
    int n = 0;
    noop_operation noop(ring,
    [&n](std::error_code ec) {
        std::println("Noop completed!");
        n = 114514;
    });
    if (auto ec = noop.submit()) {
        std::println("Failed to submit noop task: {}", ec.message());
        TEST_EXPECT(false);
    }
    std::println("Noop task submitted, waiting for completion...");
    operation_result result = ring.wait_for_result().value();
    result();
    TEST_EXPECT(n == 114514);
    std::println("Noop completed with result: {}", result.result());
    auto sync_noop = ring.make_sync<noop_operation>();
    auto sync_result = sync_noop.submit_and_wait();
    TEST_EXPECT(sync_result);
    noop_operation noop2
        = ring.make<noop_operation>(std::in_place_type<pinned_callback>, 1, 2, 3);
    if (auto ec = noop2.submit()) {
        std::println("Failed to submit noop2 task: {}", ec.message());
        TEST_EXPECT(false);
    }
    std::println("Noop2 task submitted, waiting for completion...");
    result = ring.wait_for_result().value();
    result();
    auto callback = [] (std::expected<void, std::error_code> res) {
        if (res) {
            std::println("Awaited noop completed successfully");
        } else {
            std::println("Awaited noop failed: {}", res.error().message());
        }
    };
    noop_operation noop3
        = ring.make_in_place<noop_operation<decltype(callback)&>>(callback);
    if (auto ec = noop3.submit()) {
        std::println("Failed to submit noop3 task: {}", ec.message());
        TEST_EXPECT(false);
    }
    std::println("Noop3 task submitted, waiting for completion...");
    result = ring.wait_for_result().value();
    result();
}

int main() {
    TEST_EXPECT(true);
    test_noop();
}
