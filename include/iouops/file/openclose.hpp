#pragma once
#ifndef IOUXX_OPERATION_FILE_CLOSE_H
#define IOUXX_OPERATION_FILE_CLOSE_H 1

#include <functional>
#include <string>

#include "macro_config.hpp"
#include "util/utility.hpp"
#include "iouringxx.hpp"
#include "file.hpp"

namespace iouxx::inline iouops::file {

    enum class open_flag {
        unspec = 0,
        append = O_APPEND,
        cloexec = O_CLOEXEC,
        create = O_CREAT,
        direct = O_DIRECT,
        directory = O_DIRECTORY,
        nonblock = O_NONBLOCK,
        temporary_file = O_TMPFILE,
        truncate = O_TRUNC,
    };

    inline constexpr open_flag operator|(open_flag lhs, open_flag rhs) noexcept {
        return static_cast<open_flag>(
            std::to_underlying(lhs) | std::to_underlying(rhs)
        );
    }

    enum class open_mode {
        uread = S_IRUSR,
        uwrite = S_IWUSR,
        uexec = S_IXUSR,
        gread = S_IRGRP,
        gwrite = S_IWGRP,
        gexec = S_IXGRP,
        oread = S_IROTH,
        owrite = S_IWOTH,
        oexec = S_IXOTH,
    };

    inline constexpr open_mode operator|(open_mode lhs, open_mode rhs) noexcept {
        return static_cast<open_mode>(
            std::to_underlying(lhs) | std::to_underlying(rhs)
        );
    }

    inline constexpr int current_directory = AT_FDCWD;

    template<utility::eligible_callback<file> Callback>
    class file_open_operation : public operation_base
    {
    public:
        template<typename F>
        explicit file_open_operation(iouxx::io_uring_xx& ring, F&& f)
            noexcept(utility::nothrow_constructible_callback<F>) :
            operation_base(iouxx::op_tag<file_open_operation>, ring),
            callback(std::forward<F>(f))
        {}

        using callback_type = Callback;
        using result_type = int;

        static constexpr std::uint8_t opcode = IORING_OP_OPENAT;

        template<typename Path>
        file_open_operation& path(Path&& path) & noexcept {
            this->pathstr = std::forward<Path>(path);
            return *this;
        }

        file_open_operation& directory(directory dirfd) & noexcept {
            this->dirfd = dirfd.native_handle();
            return *this;
        }

        file_open_operation& options(open_flag flags) & noexcept {
            this->flags = flags;
            return *this;
        }

        file_open_operation& mode(open_mode mode) & noexcept {
            this->modes = mode;
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_openat(sqe, dirfd,
                pathstr.c_str(),
                std::to_underlying(flags),
                std::to_underlying(modes)
            );
        }

        void do_callback(int ev, std::int32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
            utility::eligible_nothrow_callback<callback_type, result_type>) {
            if (ev >= 0) {
                std::invoke(callback, file(ev));
            } else {
                std::invoke(callback, utility::fail(-ev));
            }
        }

        std::string pathstr;
        int dirfd = current_directory;
        open_flag flags = open_flag::unspec;
        open_mode modes = open_mode::uread | open_mode::uwrite;
        callback_type callback;
    };

    template<typename F>
    file_open_operation(iouxx::io_uring_xx&, F) -> file_open_operation<std::decay_t<F>>;

    template<utility::eligible_callback<void> Callback>
    class file_close_operation : public operation_base
    {
    public:
        template<typename F>
        explicit file_close_operation(iouxx::io_uring_xx& ring, F&& f)
            noexcept(utility::nothrow_constructible_callback<F>) :
            operation_base(iouxx::op_tag<file_close_operation>, ring),
            callback(std::forward<F>(f))
        {}

        using callback_type = Callback;
        using result_type = void;

        static constexpr std::uint8_t opcode = IORING_OP_CLOSE;

        file_close_operation& file(int fd) & noexcept {
            this->fd = fd;
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_close(sqe, fd);
        }

        void do_callback(int ev, std::int32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
            utility::eligible_nothrow_callback<callback_type, result_type>) {
            if constexpr (utility::callback<callback_type, void>) {
                if (ev == 0) {
                    std::invoke(callback, utility::void_success());
                } else {
                    std::invoke(callback, utility::fail(-ev));
                }
            } else if constexpr (utility::errorcode_callback<callback_type>) {
                std::invoke(callback, utility::make_system_error_code(-ev));
            } else {
                static_assert(utility::always_false<Callback>, "Unreachable");
            }
        }

        int fd = -1;
        [[no_unique_address]] callback_type callback;
    };

    template<typename F>
    file_close_operation(iouxx::io_uring_xx&, F) -> file_close_operation<std::decay_t<F>>;

} // namespace iouxx::inline iouops::file

#endif // IOUXX_OPERATION_FILE_CLOSE_H
