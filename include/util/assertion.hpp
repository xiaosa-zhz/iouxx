#ifndef IOUXX_ASSERTION_H
#define IOUXX_ASSERTION_H 1

#include "macro_config.hpp" // IWYU pragma: keep

#ifndef IOUXX_CONFIG_DISABLE_ASSERTION

#if defined(__cpp_contracts) && __cpp_contracts >= 202502L
#define assert(...) contract_assert(__VA_ARGS__)
#else // ! __cpp_contracts
#include <cassert> // IWYU pragma: export
#endif

#else // !IOUXX_CONFIG_DISABLE_ASSERTION
#define assert(...) ((void)0)
#endif // IOUXX_CONFIG_DISABLE_ASSERTION

#endif // IOUXX_ASSERTION_H
