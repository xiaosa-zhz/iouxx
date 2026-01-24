#ifdef IOUXX_CONFIG_USE_CXX_MODULE

import std;
import iouxx.ring;
import iouxx.ops.cancel;
import iouxx.ops.timeout;
import iouxx.ops.futex;

#else // !IOUXX_CONFIG_USE_CXX_MODULE

#include <cstdint>
#include <chrono>
#include <print>
#include <system_error>
#include <thread>
#include <atomic>
#include <bit>

#include "iouxx/iouringxx.hpp"
#include "iouxx/iouops/timeout.hpp"
#include "iouxx/iouops/cancel.hpp"
#include "iouxx/iouops/futex.hpp"

#endif // IOUXX_CONFIG_USE_CXX_MODULE

// TODO: test on machine supporting futex operations

int main() {
    using namespace std::literals;
    std::uint32_t fake_futex = 0;
    iouxx::ring ring(64);
    
    std::jthread waiter([&ring, &fake_futex]() {
        iouxx::futex_wait_operation wait_op = ring.make_sync<iouxx::futex_wait_operation>();
        wait_op.futex_word(fake_futex)
            .expected_value(0);
        if (auto res = wait_op.submit_and_wait()) {
            std::println("Futex wait returned: {}", fake_futex);
        } else {
            std::println("Futex wait failed: {}", res.error().message());
        }
    });

    std::this_thread::sleep_for(100ms);
    fake_futex = 1;
    iouxx::futex_wake_operation wake_op = ring.make_sync<iouxx::futex_wake_operation>();
    wake_op.futex_word(fake_futex);
    if (auto res = wake_op.submit_and_wait()) {
        std::println("Futex wake returned: {} woken", *res);
    } else {
        std::println("Futex wake failed: {}", res.error().message());
    }
}
