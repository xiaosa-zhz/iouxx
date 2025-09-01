#include <new>
#include <thread>
#include <atomic>
#include <print>

// #define SET_ALIGN
#define SET_ALIGN alignas(2 * std::hardware_destructive_interference_size)

SET_ALIGN std::atomic_int data = 0;
SET_ALIGN std::atomic_int flag = 0;
SET_ALIGN std::atomic_bool start = false;
SET_ALIGN std::size_t fc = 0;
SET_ALIGN std::size_t sc = 0;

void p0(std::stop_token st) {
    while (!st.stop_requested()) {
        while (!start.load(std::memory_order_acquire));
        data.store(42, std::memory_order_relaxed);
        flag.store(1, std::memory_order_relaxed);
    }
}

void p1(std::stop_token st) {
    while (!st.stop_requested()) {
        while (!start.load(std::memory_order_acquire));
        while (flag.load(std::memory_order_relaxed) == 0);
        if (data.load(std::memory_order_relaxed) == 0) {
            ++fc;
        } else {
            ++sc;
        }
        std::println("{} {}", fc, sc);
        start.store(false, std::memory_order_release);
    }
}

int main() {
    {
        std::jthread t0(p0);
        std::jthread t1(p1);
        for (auto i = 0uz; i < 200000; ++i) {
            while (start.load(std::memory_order_acquire));
            data.store(0, std::memory_order_relaxed);
            flag.store(0, std::memory_order_relaxed);
            start.store(true, std::memory_order_release);
        }
        t0.request_stop();
        t1.request_stop();
        start.store(true, std::memory_order_release);
    }
}
