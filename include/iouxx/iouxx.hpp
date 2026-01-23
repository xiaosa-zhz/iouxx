#pragma once
#ifndef IOUXX_ALL_HEADERS_H
#define IOUXX_ALL_HEADERS_H 1

/*
 * This file is a convenience header that includes all headers of iouxx.
*/

#include "clock.hpp" // IWYU pragma: export

#include "iouringxx.hpp" // IWYU pragma: export

#include "iouops/noop.hpp" // IWYU pragma: export
#include "iouops/timeout.hpp" // IWYU pragma: export
#include "iouops/cancel.hpp" // IWYU pragma: export
#include "iouops/network/socketio.hpp" // IWYU pragma: export
#include "iouops/file/fileio.hpp" // IWYU pragma: export

namespace iouxx::details {

    // Make 'empty header warning' happy
    [[maybe_unused]] consteval void all_headers_anchor() noexcept {}

} // namespace iouxx::details

#endif // IOUXX_ALL_HEADERS_H
