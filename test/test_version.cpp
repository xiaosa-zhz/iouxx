#ifdef IOUXX_CONFIG_USE_CXX_MODULE

import std;
import iouxx;

#else // !IOUXX_CONFIG_USE_CXX_MODULE

#include <print>

#include "iouxx/iouringxx.hpp"

#endif // IOUXX_CONFIG_USE_CXX_MODULE

int main() {
    static_assert(iouxx::ring::version_compat_with(IOUXX_LIBURING_MIN_VERSION));
    auto version = iouxx::ring::version();
    if (!iouxx::ring::version_compat_with(IOUXX_LIBURING_MIN_VERSION)) {
        std::println("Error: liburing version '{}' is required, but current version is '{}'.",
            IOUXX_LIBURING_MIN_VERSION, version);
        return -1;
    }
    std::println("liburing version '{}' is supported.", iouxx::ring::version());
}
