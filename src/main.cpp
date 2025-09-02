#include <cstddef>
#include <new>
#include <thread>
#include <atomic>
#include <print>

// #define SET_ALIGN
#define SET_ALIGN alignas(2 * std::hardware_destructive_interference_size)

SET_ALIGN std::atomic_int data = 0;
SET_ALIGN std::atomic_int flag = 0;
SET_ALIGN std::atomic_size_t total = 0;
SET_ALIGN std::atomic_size_t total0 = 0;
SET_ALIGN std::atomic_size_t total1 = 0;
SET_ALIGN std::size_t fc = 0;
SET_ALIGN std::size_t sc = 0;

inline constexpr std::size_t end = 10000000;

void p0() noexcept {
    std::size_t local = 0;
    while (local < end) {
        while (total.load(std::memory_order_acquire) <= local);
        data.store(42, std::memory_order_relaxed);
        flag.store(1, std::memory_order_relaxed);
        ++local;
        total0.fetch_add(1, std::memory_order_release);
    }
}

void p1() noexcept {
    std::size_t local = 0;
    while (local < end) {
        while (total.load(std::memory_order_acquire) <= local);
        while (flag.load(std::memory_order_relaxed) == 0);
        if (data.load(std::memory_order_relaxed) == 0) {
            ++fc;
        } else {
            ++sc;
        }
        ++local;
        total1.fetch_add(1, std::memory_order_release);
    }
}

int main() {
    {
        std::jthread t0(p0);
        std::jthread t1(p1);
        for (auto i = 0uz; i < end; ++i) {
            while (total0.load(std::memory_order_acquire) < i);
            while (total1.load(std::memory_order_acquire) < i);
            data.store(0, std::memory_order_relaxed);
            flag.store(0, std::memory_order_relaxed);
            total.fetch_add(1, std::memory_order_release);
        }
    }
    std::println("{} {}", fc, sc);
}
