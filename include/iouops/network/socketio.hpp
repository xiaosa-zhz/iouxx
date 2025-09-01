#pragma once
#ifndef IOUXX_OPERATION_SOCKETIO_H
#define IOUXX_OPERATION_SOCKETIO_H 1

/*
    * This file is a convenience header to include all socket related operations.
*/

#include "./sockprep.hpp" // IWYU pragma: export
#include "./connection.hpp" // IWYU pragma: export
#include "./sendrecv.hpp" // IWYU pragma: export

namespace iouxx::details {

    // Make 'empty header warning' happy
    [[maybe_unused]] inline void all_network_headers_anchor() noexcept {}

} // namespace iouxx::details

#endif // IOUXX_OPERATION_SOCKETIO_H
