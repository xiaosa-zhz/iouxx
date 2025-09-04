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

// IOUXX_CONFIG_DISABLE_ASSERTION
// Define this macro will disable all assertions in the library.
#ifdef IOUXX_CONFIG_DISABLE_ASSERTION
#undef NDEBUG
#endif // IOUXX_CONFIG_DISABLE_ASSERTION

// IOUXX_CONFIG_ENABLE_FEATURE_TESTS
// Define this macro will make each operation check whether it is supported
// on the current system. If not, std::errc::function_not_supported (ENOSYS)
// will be returned when submitting the operation.
#ifdef IOUXX_CONFIG_ENABLE_FEATURE_TESTS
#define IOUXX_IORING_FEATURE_TESTS_ENABLED 1
#else // !IOUXX_CONFIG_ENABLE_FEATURE_TESTS
#define IOUXX_IORING_FEATURE_TESTS_ENABLED 0
#endif // IOUXX_CONFIG_ENABLE_FEATURE_TESTS

#endif // IOUXX_MACRO_CONFIGURATION_H
