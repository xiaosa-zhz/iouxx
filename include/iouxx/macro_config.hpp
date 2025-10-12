#pragma once
#ifndef IOUXX_MACRO_CONFIGURATION_H
#define IOUXX_MACRO_CONFIGURATION_H 1

/*
    * This file responds for macro configuration.
    * User may define macros to enable/disable certain features.
*/

// IOUXX_CONFIG_CALLBACK_NOTHROW
// Define this macro will make all operation callbacks nothrow.
// If the user-provided callback may throw, std::terminate will be called.
#ifdef IOUXX_CONFIG_CALLBACK_NOTHROW
#define IOUXX_CALLBACK_NOEXCEPT noexcept
#define IOUXX_CALLBACK_NOEXCEPT_IF(...) noexcept
#else // !IOUXX_CONFIG_CALLBACK_NOTHROW
#define IOUXX_CALLBACK_NOEXCEPT
#define IOUXX_CALLBACK_NOEXCEPT_IF(...) noexcept(__VA_ARGS__)
#endif // IOUXX_CONFIG_CALLBACK_NOTHROW

// IOUXX_CONFIG_ENABLE_FEATURE_TESTS
// Define this macro will make each operation check whether it is supported
// on the current system. If not, std::errc::function_not_supported (ENOSYS)
// will be returned when submitting the operation.
#ifdef IOUXX_CONFIG_ENABLE_FEATURE_TESTS
#define IOUXX_IORING_FEATURE_TESTS_ENABLED 1
#else // !IOUXX_CONFIG_ENABLE_FEATURE_TESTS
#define IOUXX_IORING_FEATURE_TESTS_ENABLED 0
#endif // IOUXX_CONFIG_ENABLE_FEATURE_TESTS

// IOUXX_CONFIG_USE_CXX_MODULE
// Define this macro to enable C++ module support.
#ifdef IOUXX_CONFIG_USE_CXX_MODULE
#define IOUXX_USE_CXX_MODULE 1
#define IOURINGINLINE inline // Module cannot handle static inline
#endif // IOUXX_CONFIG_USE_CXX_MODULE

// IOUXX_CONFIG_DISABLE_ASSERTION
// Define this macro will disable all assertions in the library.
#ifndef IOUXX_CONFIG_DISABLE_ASSERTION
#define IOUXX_ENABLE_INTERNAL_ASSERTION 1
#endif // IOUXX_CONFIG_DISABLE_ASSERTION

// IOUXX_CONFIG_NOT_USE_CONTRACTS
// Define this macro to disable C++26 contract support.
// Internal assertion will use fallback implementation.
#ifndef IOUXX_CONFIG_NOT_USE_CONTRACTS
#if defined(__cpp_contracts) && __cpp_contracts >= 202502L
#define IOUXX_USE_CXX_CONTRACTS 1
#endif // __cpp_contracts
#endif // IOUXX_CONFIG_NOT_USE_CONTRACTS

#endif // IOUXX_MACRO_CONFIGURATION_H
