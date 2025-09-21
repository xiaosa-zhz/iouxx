#pragma once
#ifndef IOUXX_OPERATION_FILE_CLOSE_H
#define IOUXX_OPERATION_FILE_CLOSE_H 1

#ifndef IOUXX_USE_CXX_MODULE

#include <functional>
#include <string>
#include <utility>
#include <type_traits>

#include "iouxx/macro_config.hpp"
#include "iouxx/util/utility.hpp"
#include "iouxx/iouringxx.hpp"
#include "file.hpp"

#endif // IOUXX_USE_CXX_MODULE

IOUXX_EXPORT
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

    constexpr open_flag operator|(open_flag lhs, open_flag rhs) noexcept {
        return static_cast<open_flag>(
            std::to_underlying(lhs) | std::to_underlying(rhs)
        );
    }

    constexpr open_flag& operator|=(open_flag& lhs, open_flag rhs) noexcept {
        lhs = lhs | rhs;
        return lhs;
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

    constexpr open_mode operator|(open_mode lhs, open_mode rhs) noexcept {
        return static_cast<open_mode>(
            std::to_underlying(lhs) | std::to_underlying(rhs)
        );
    }

    constexpr open_mode& operator|=(open_mode& lhs, open_mode rhs) noexcept {
        lhs = lhs | rhs;
        return lhs;
    }

    inline constexpr directory current_directory{ AT_FDCWD };

} // namespace iouxx::iouops::file

namespace iouxx::inline iouops::file {

    class file_open_operation_base
    {
    public:
        template<typename Self, typename Path>
        Self& path(this Self& self, Path&& path) noexcept {
            self.pathstr = std::forward<Path>(path);
            return self;
        }

        template<typename Self>
        Self& directory(this Self& self, directory dir) noexcept {
            self.dirfd = dir.native_handle();
            return self;
        }

        template<typename Self>
        Self& options(this Self& self, open_flag flags) noexcept {
            self.flags = flags;
            return self;
        }

        template<typename Self>
        Self& mode(this Self& self, open_mode mode) noexcept {
            self.modes = mode;
            return self;
        }

    protected:
        std::string pathstr;
        int dirfd = current_directory.native_handle();
        open_flag flags = open_flag::unspec;
        open_mode modes = open_mode::uread | open_mode::uwrite;
    };

} // namespace iouxx::iouops::file

IOUXX_EXPORT
namespace iouxx::inline iouops::file {

    // TODO: change openat to openat2

    template<utility::eligible_callback<file> Callback>
    class file_open_operation : public operation_base, public file_open_operation_base
    {
    public:
        template<utility::not_tag F>
        explicit file_open_operation(iouxx::ring& ring, F&& f)
            noexcept(utility::nothrow_constructible_callback<F>) :
            operation_base(iouxx::op_tag<file_open_operation>, ring),
            callback(std::forward<F>(f))
        {}

        template<typename F, typename... Args>
        explicit file_open_operation(iouxx::ring& ring, std::in_place_type_t<F>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<F, Args...>) :
            operation_base(iouxx::op_tag<file_open_operation>, ring),
            callback(std::forward<Args>(args)...)
        {}

        using callback_type = Callback;
        using result_type = int;

        static constexpr std::uint8_t opcode = IORING_OP_OPENAT;

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_openat(sqe, dirfd,
                pathstr.c_str(),
                std::to_underlying(flags),
                std::to_underlying(modes)
            );
        }

        void do_callback(int ev, std::uint32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
            utility::eligible_nothrow_callback<callback_type, result_type>) {
            if (ev >= 0) {
                std::invoke(callback, file(ev));
            } else {
                std::invoke(callback, utility::fail(-ev));
            }
        }

        [[no_unique_address]] callback_type callback;
    };

    template<utility::not_tag F>
    file_open_operation(iouxx::ring&, F) -> file_open_operation<std::decay_t<F>>;

    template<typename F, typename... Args>
    file_open_operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...) -> file_open_operation<F>;

    inline constexpr int alloc_index = IORING_FILE_INDEX_ALLOC;

    template<utility::eligible_callback<fixed_file> Callback>
    class fixed_file_open_operation : public operation_base, public file_open_operation_base
    {
    public:
        template<utility::not_tag F>
        explicit fixed_file_open_operation(iouxx::ring& ring, F&& f)
            noexcept(utility::nothrow_constructible_callback<F>) :
            operation_base(iouxx::op_tag<fixed_file_open_operation>, ring),
            callback(std::forward<F>(f))
        {}

        template<typename F, typename... Args>
        explicit fixed_file_open_operation(iouxx::ring& ring, std::in_place_type_t<F>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<F, Args...>) :
            operation_base(iouxx::op_tag<fixed_file_open_operation>, ring),
            callback(std::forward<Args>(args)...)
        {}

        using callback_type = Callback;
        using result_type = fixed_file;

        static constexpr std::uint8_t opcode = IORING_OP_OPENAT;

        fixed_file_open_operation& index(int index = alloc_index) & noexcept {
            this->file_index = index;
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_openat_direct(sqe, dirfd,
                pathstr.c_str(),
                std::to_underlying(flags),
                std::to_underlying(modes),
                file_index
            );
        }

        void do_callback(int ev, std::uint32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
            utility::eligible_nothrow_callback<callback_type, result_type>) {
            if (ev >= 0) {
                std::invoke(callback, fixed_file(ev));
            } else {
                std::invoke(callback, utility::fail(-ev));
            }
        }

        int file_index = alloc_index;
        [[no_unique_address]] callback_type callback;
    };

    template<utility::not_tag F>
    fixed_file_open_operation(iouxx::ring&, F) -> fixed_file_open_operation<std::decay_t<F>>;

    template<typename F, typename... Args>
    fixed_file_open_operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...)
        -> fixed_file_open_operation<F>;

    template<utility::eligible_callback<void> Callback>
    class file_close_operation : public operation_base
    {
    public:
        template<utility::not_tag F>
        explicit file_close_operation(iouxx::ring& ring, F&& f)
            noexcept(utility::nothrow_constructible_callback<F>) :
            operation_base(iouxx::op_tag<file_close_operation>, ring),
            callback(std::forward<F>(f))
        {}

        template<typename F, typename... Args>
        explicit file_close_operation(iouxx::ring& ring, std::in_place_type_t<F>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<F, Args...>) :
            operation_base(iouxx::op_tag<file_close_operation>, ring),
            callback(std::forward<Args>(args)...)
        {}

        using callback_type = Callback;
        using result_type = void;

        static constexpr std::uint8_t opcode = IORING_OP_CLOSE;

        file_close_operation& file(const file& f) & noexcept {
            this->fd = f.native_handle();
            this->is_fixed = false;
            return *this;
        }

        file_close_operation& file(const fixed_file& f) & noexcept {
            this->fd = f.index();
            this->is_fixed = true;
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            if (is_fixed) {
                ::io_uring_prep_close_direct(sqe, fd);
            } else {
                ::io_uring_prep_close(sqe, fd);
            }
        }

        void do_callback(int ev, std::uint32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
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
        bool is_fixed = false;
        [[no_unique_address]] callback_type callback;
    };

    template<utility::not_tag F>
    file_close_operation(iouxx::ring&, F) -> file_close_operation<std::decay_t<F>>;

    template<typename F, typename... Args>
    file_close_operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...) -> file_close_operation<F>;

} // namespace iouxx::inline iouops::file

#endif // IOUXX_OPERATION_FILE_CLOSE_H
