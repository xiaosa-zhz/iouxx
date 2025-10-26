#ifdef IOUXX_CONFIG_USE_CXX_MODULE

import std;
import iouxx.util;

#else // !IOUXX_CONFIG_USE_CXX_MODULE

#include <system_error>
#include <expected>

#include "iouxx/util/utility.hpp"

#endif // IOUXX_CONFIG_USE_CXX_MODULE

using namespace iouxx::utility;

constexpr void not_cb() {}
static_assert(!eligible_callback<decltype(not_cb), void>);
constexpr void cb1(std::expected<int, std::error_code>) {}
static_assert(eligible_callback<decltype(cb1), int>);
static_assert(!eligible_nothrow_callback<decltype(cb1), int>);
constexpr void cb2(std::expected<void, std::error_code>) {}
static_assert(eligible_callback<decltype(cb2), void>);
static_assert(!eligible_nothrow_callback<decltype(cb2), void>);
constexpr void cb3(std::error_code) noexcept {}
static_assert(eligible_callback<decltype(cb3), void>);
static_assert(errorcode_callback<decltype(cb3)>);
static_assert(eligible_nothrow_callback<decltype(cb3), void>);
static_assert(!eligible_callback<decltype(cb3), int>);
static_assert(!errorcode_callback<decltype(cb1)>);
static_assert(!eligible_callback<decltype(cb1), void>);
constexpr void cb4(std::expected<int, std::error_code>) noexcept {}
static_assert(eligible_callback<decltype(cb4), int>);
static_assert(eligible_nothrow_callback<decltype(cb4), int>);

// Compile only test
int main() {}
