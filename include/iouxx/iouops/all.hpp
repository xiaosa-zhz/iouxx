#pragma once
#ifndef IOUXX_ALL_OPERATION_HEADERS_H
#define IOUXX_ALL_OPERATION_HEADERS_H 1

/*
 * This file is a convenience header that includes all headers of iouops.
*/

#include "noop.hpp" // IWYU pragma: export
#include "timeout.hpp" // IWYU pragma: export
#include "cancel.hpp" // IWYU pragma: export
#include "file/fileio.hpp" // IWYU pragma: export
#include "network/socketio.hpp" // IWYU pragma: export

namespace iouxx::details {

    // Make 'empty header warning' happy
    [[maybe_unused]] consteval void all_iouops_headers_anchor() noexcept {}

} // namespace iouxx::details

#endif // IOUXX_ALL_OPERATION_HEADERS_H
