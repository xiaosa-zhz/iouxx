#pragma once
#ifndef IOUXX_ALL_OPERATION_HEADERS_H
#define IOUXX_ALL_OPERATION_HEADERS_H 1

/*
    * This file is a convenience header that includes all headers of iouops.
*/

#include "iouops/noop.hpp" // IWYU pragma: export
#include "iouops/timeout.hpp" // IWYU pragma: export
#include "iouops/cancel.hpp" // IWYU pragma: export
#include "iouops/fileio.hpp" // IWYU pragma: export
#include "iouops/network/ip.hpp" // IWYU pragma: export
#include "iouops/network/socketio.hpp" // IWYU pragma: export

namespace iouxx::details {

    // Make 'empty header warning' happy
    [[maybe_unused]] inline void all_iouops_headers_anchor() noexcept {}

} // namespace iouxx::details

#endif // IOUXX_ALL_OPERATION_HEADERS_H
