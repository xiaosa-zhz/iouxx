#ifndef IOUXX_ASSERTION_H
#define IOUXX_ASSERTION_H 1

#include "iouxx/macro_config.hpp" // IWYU pragma: keep

#ifdef IOUXX_ENABLE_INTERNAL_ASSERTION

#ifdef IOUXX_ENABLE_ASSERT_ASSUME
#define IOUXX_DO_ASSERT(...) [] static noexcept { std::unreachable(); }()
#else // !IOUXX_ENABLE_ASSERT_ASSUME
#define IOUXX_DO_ASSERT(...) iouxx::utility::assertion_failed(#__VA_ARGS__, __FILE__, __LINE__)
#endif // IOUXX_ENABLE_ASSERT_ASSUME

#ifdef IOUXX_USE_CXX_CONTRACTS
#define IOUXX_ASSERT(...) contract_assert(__VA_ARGS__)
#else // !IOUXX_USE_CXX_CONTRACTS
#define IOUXX_ASSERT(...) ((__VA_ARGS__) ? static_cast<void>(0) : IOUXX_DO_ASSERT(__VA_ARGS__))
#endif // IOUXX_USE_CXX_CONTRACTS

#else // !IOUXX_ENABLE_INTERNAL_ASSERTION
#define IOUXX_ASSERT(...) static_cast<void>(0)
#endif // IOUXX_ENABLE_INTERNAL_ASSERTION

#endif // IOUXX_ASSERTION_H
