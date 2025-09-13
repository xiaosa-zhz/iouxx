#pragma once
#ifndef IOUXX_OPERATION_FILE_FILE_INSTANCE_H
#define IOUXX_OPERATION_FILE_FILE_INSTANCE_H 1

#ifndef IOUXX_USE_CXX_MODULE

#include "macro_config.hpp" // IWYU pragma: export
#include "cxxmodule_helper.hpp" // IWYU pragma: export

#endif // IOUXX_USE_CXX_MODULE

IOUXX_EXPORT
namespace iouxx::inline iouops::file {

    // Warning:
    // This class is NOT a RAII wrapper of fd.
    class file
    {
    public:
        file() = default;
        file(const file&) = default;
        file& operator=(const file&) = default;

        explicit file(int fd) noexcept : fd(fd) {}

        [[nodiscard]]
        int native_handle() const noexcept { return fd; }

    private:
        int fd = -1;
    };

    inline constexpr file invalid_file = {};

    // Warning:
    // This class is NOT a RAII wrapper of directory fd.
    class directory : public file
    {
    public:
        using file::file; // inherit constructors
    };

} // namespace iouxx::iouops::file

#endif // IOUXX_OPERATION_FILE_FILE_INSTANCE_H
