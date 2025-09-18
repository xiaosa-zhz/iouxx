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
        constexpr file() = default;
        constexpr file(const file&) = default;
        constexpr file& operator=(const file&) = default;

        constexpr explicit file(int fd) noexcept : fd(fd) {}

        [[nodiscard]]
        constexpr int native_handle() const noexcept { return fd; }

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

    // io_uring fixed file
    class fixed_file
    {
    public:
        constexpr fixed_file() = default;
        constexpr fixed_file(const fixed_file&) = default;
        constexpr fixed_file& operator=(const fixed_file&) = default;

        constexpr explicit fixed_file(int index) noexcept
            : fd_index(index)
        {}

        [[nodiscard]]
        constexpr int index() const noexcept { return fd_index; }

    private:
        int fd_index = -1;
    };

} // namespace iouxx::iouops::file

#endif // IOUXX_OPERATION_FILE_FILE_INSTANCE_H
