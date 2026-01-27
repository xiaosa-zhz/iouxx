#ifdef IOUXX_CONFIG_USE_CXX_MODULE

import std;
import iouxx;

#else // !IOUXX_CONFIG_USE_CXX_MODULE

#include <cstdint>
#include <print>
#include <system_error>
#include <thread>
#include <atomic>
#include <cstdlib>

#include "iouxx/iouringxx.hpp"
#include "iouxx/iouops/futex.hpp"

#endif // IOUXX_CONFIG_USE_CXX_MODULE

#include <linux/futex.h> // test for FUTEX2_SIZE_U32

#ifndef FUTEX2_SIZE_U32

// Skip this test on machines without futex2 support

int main() {
    std::println("Skipping futex test: FUTEX2_SIZE_U32 not defined");
    return 0;
}

#else // FUTEX2_SIZE_U32

int main() {
    using namespace std::literals;
    std::uint32_t fake_futex = 0;
    iouxx::ring ring(64);
    
    std::jthread waiter([&fake_futex]() {
        iouxx::ring ring(64);
        iouxx::futex_wait_operation wait_op = ring.make_sync<iouxx::futex_wait_operation>();
        wait_op.futex_word(fake_futex)
            .expected_value(0);
        if (auto res = wait_op.submit_and_wait()) {
            std::println("Futex wait returned: {}", fake_futex);
        } else {
            std::println("Futex wait failed: {}", res.error().message());
            std::exit(1);
        }
    });

    std::this_thread::sleep_for(100ms);
    std::atomic_ref(fake_futex).store(1);
    iouxx::futex_wake_operation wake_op = ring.make_sync<iouxx::futex_wake_operation>();
    wake_op.futex_word(fake_futex);
    if (auto res = wake_op.submit_and_wait()) {
        std::println("Futex wake returned: {} woken", *res);
    } else {
        std::println("Futex wake failed: {}", res.error().message());
        std::exit(1);
    }
}

#endif // FUTEX2_SIZE_U32
