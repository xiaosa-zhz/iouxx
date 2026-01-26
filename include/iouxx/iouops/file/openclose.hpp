#pragma once
#ifndef IOUXX_OPERATION_FILE_CLOSE_H
#define IOUXX_OPERATION_FILE_CLOSE_H 1

#ifndef IOUXX_USE_CXX_MODULE

#include <type_traits>
#include <functional>
#include <string>
#include <utility>

#include "iouxx/macro_config.hpp"
#include "iouxx/util/utility.hpp"
#include "iouxx/iouringxx.hpp"
#include "file.hpp"

#endif // IOUXX_USE_CXX_MODULE

/*
 * Note: some flags are not compitible with io_uring fixed file
*/

IOUXX_EXPORT
namespace iouxx::inline iouops::fileops {

    enum class open_flag : std::uint64_t {
        unspec = 0,
        append = O_APPEND,
        cloexec = O_CLOEXEC,
        create = O_CREAT,
        direct = O_DIRECT,
        // directory = O_DIRECTORY, // use directory_open_operation instead
        nonblock = O_NONBLOCK,
        temporary_file = O_TMPFILE,
        truncate = O_TRUNC,
        readonly = O_RDONLY,
        writeonly = O_WRONLY,
        readwrite = O_RDWR,
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

    enum class open_mode : std::uint64_t {
        none = 0,
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

    enum class open_resolve_flag : std::uint64_t {
        none = 0,
        no_xdev = RESOLVE_NO_XDEV,
        no_magiclinks = RESOLVE_NO_MAGICLINKS,
        no_symlinks = RESOLVE_NO_SYMLINKS,
        beneath = RESOLVE_BENEATH,
        in_root = RESOLVE_IN_ROOT,
        cached = RESOLVE_CACHED,
    };

    constexpr open_resolve_flag operator|(
        open_resolve_flag lhs, open_resolve_flag rhs) noexcept {
        return static_cast<open_resolve_flag>(
            std::to_underlying(lhs) | std::to_underlying(rhs)
        );
    }

    constexpr open_resolve_flag& operator|=(
        open_resolve_flag& lhs, open_resolve_flag rhs) noexcept {
        lhs = lhs | rhs;
        return lhs;
    }

    inline constexpr directory current_directory{ AT_FDCWD };

} // namespace iouxx::iouops::fileops

namespace iouxx::inline iouops::fileops {

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
            self.how.flags = std::to_underlying(flags);
            return self;
        }

        template<typename Self>
        Self& mode(this Self& self, open_mode mode) noexcept {
            self.how.mode = std::to_underlying(mode);
            return self;
        }

        template<typename Self>
        Self& resolve_flags(this Self& self, open_resolve_flag resolve) noexcept {
            self.how.resolve = std::to_underlying(resolve);
            return self;
        }

    protected:
        std::string pathstr;
        int dirfd = current_directory.native_handle();
        ::open_how how = { .flags = 0, .mode = 0, .resolve = 0 };
    };

} // namespace iouxx::iouops::fileops

IOUXX_EXPORT
namespace iouxx::inline iouops::fileops {

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
        using result_type = file;

        static constexpr std::uint8_t opcode = IORING_OP_OPENAT2;

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            how.flags |= O_NONBLOCK;
            ::io_uring_prep_openat2(sqe, dirfd,
                pathstr.c_str(), &how
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

        static constexpr std::uint8_t opcode = IORING_OP_OPENAT2;

        fixed_file_open_operation& index(int index = alloc_index) & noexcept {
            this->file_index = index;
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            how.flags |= O_NONBLOCK;
            ::io_uring_prep_openat2_direct(sqe, dirfd,
                pathstr.c_str(), &how, file_index
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

    template<utility::eligible_callback<directory> Callback>
    class directory_open_operation : public operation_base, public file_open_operation_base
    {
    public:
        template<utility::not_tag F>
        explicit directory_open_operation(iouxx::ring& ring, F&& f)
            noexcept(utility::nothrow_constructible_callback<F>) :
            operation_base(iouxx::op_tag<directory_open_operation>, ring),
            callback(std::forward<F>(f))
        {}

        template<typename F, typename... Args>
        explicit directory_open_operation(iouxx::ring& ring, std::in_place_type_t<F>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<F, Args...>) :
            operation_base(iouxx::op_tag<directory_open_operation>, ring),
            callback(std::forward<Args>(args)...)
        {}

        using callback_type = Callback;
        using result_type = fileops::directory;

        static constexpr std::uint8_t opcode = IORING_OP_OPENAT2;

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            how.flags |= O_DIRECTORY | O_NONBLOCK;
            ::io_uring_prep_openat2(sqe, dirfd,
                pathstr.c_str(), &how
            );
        }

        void do_callback(int ev, std::uint32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
            utility::eligible_nothrow_callback<callback_type, result_type>) {
            if (ev >= 0) {
                std::invoke(callback, fileops::directory(ev));
            } else {
                std::invoke(callback, utility::fail(-ev));
            }
        }

        [[no_unique_address]] callback_type callback;
    };

    template<utility::not_tag F>
    directory_open_operation(iouxx::ring&, F) -> directory_open_operation<std::decay_t<F>>;

    template<typename F, typename... Args>
    directory_open_operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...)
        -> directory_open_operation<F>;

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
            if constexpr (utility::stdexpected_callback<callback_type, void>) {
                if (ev == 0) {
                    std::invoke(callback, utility::void_success());
                } else {
                    std::invoke(callback, utility::fail(-ev));
                }
            } else if constexpr (utility::errorcode_callback<callback_type>) {
                std::invoke(callback, utility::make_system_error_code(-ev));
            } else {
                static_assert(false, "Unreachable");
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
    
    template<utility::eligible_callback<fixed_file> Callback>
    class fixed_file_register_operation : public operation_base
    {
    public:
        template<utility::not_tag F>
        explicit fixed_file_register_operation(iouxx::ring& ring, F&& f)
            noexcept(utility::nothrow_constructible_callback<F>) :
            operation_base(iouxx::op_tag<fixed_file_register_operation>, ring),
            callback(std::forward<F>(f))
        {}

        template<typename F, typename... Args>
        explicit fixed_file_register_operation(iouxx::ring& ring, std::in_place_type_t<F>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<F, Args...>) :
            operation_base(iouxx::op_tag<fixed_file_register_operation>, ring),
            callback(std::forward<Args>(args)...)
        {}

        using callback_type = Callback;
        using result_type = fixed_file;

        static constexpr std::uint8_t opcode = IORING_OP_FILES_UPDATE;

        fixed_file_register_operation& file(file fd) & noexcept {
            this->fd = fd.native_handle();
            return *this;
        }

        fixed_file_register_operation& offset(int offset) & noexcept {
            this->off = offset;
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_files_update(sqe, &fd, 1, off);
        }

        void do_callback(int ev, std::uint32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
            utility::eligible_nothrow_callback<callback_type, result_type>) {
            if (ev >= 0) {
                std::invoke(callback, fixed_file(
                    off == alloc_index ? fd : off
                ));
            } else {
                std::invoke(callback, utility::fail(-ev));
            }
        }

        int fd = -1;
        int off = alloc_index;
        [[no_unique_address]] callback_type callback;
    };

    template<utility::not_tag F>
    fixed_file_register_operation(iouxx::ring&, F)
        -> fixed_file_register_operation<std::decay_t<F>>;

    template<typename F, typename... Args>
    fixed_file_register_operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...)
        -> fixed_file_register_operation<F>;

    struct fixed_file_register_batch_result {
        std::size_t allocated;
        std::span<int> file_index;
    };

    template<utility::eligible_callback<fixed_file_register_batch_result> Callback>
    class fixed_file_register_batch_operation : public operation_base
    {
    public:
        template<utility::not_tag F>
        explicit fixed_file_register_batch_operation(iouxx::ring& ring, F&& f)
            noexcept(utility::nothrow_constructible_callback<F>) :
            operation_base(iouxx::op_tag<fixed_file_register_batch_operation>, ring),
            callback(std::forward<F>(f))
        {}

        template<typename F, typename... Args>
        explicit fixed_file_register_batch_operation(iouxx::ring& ring,
            std::in_place_type_t<F>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<F, Args...>) :
            operation_base(iouxx::op_tag<fixed_file_register_batch_operation>, ring),
            callback(std::forward<Args>(args)...)
        {}

        using callback_type = Callback;
        using result_type = fixed_file_register_batch_result;

        static constexpr std::uint8_t opcode = IORING_OP_FILES_UPDATE;

        // fds will be updated with indexes of fixed files
        fixed_file_register_batch_operation& files(std::span<int> fds) & noexcept {
            this->fds = fds;
            return *this;
        }

        fixed_file_register_batch_operation& offset(int offset) & noexcept {
            this->off = offset;
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_files_update(sqe, fds.data(), fds.size(), off);
        }

        void do_callback(int ev, std::uint32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
            utility::eligible_nothrow_callback<callback_type, result_type>) {
            if (ev >= 0) {
                if (off != alloc_index) {
                    for (int i = 0; i < fds.size(); ++i) {
                        fds[i] = off + i;
                    }
                }
                std::invoke(callback, fixed_file_register_batch_result{
                    .allocated = static_cast<std::size_t>(ev),
                    .file_index = fds
                });
            } else {
                std::invoke(callback, utility::fail(-ev));
            }
        }

        std::span<int> fds;
        int off = alloc_index;
        [[no_unique_address]] callback_type callback;
    };

    template<utility::not_tag F>
    fixed_file_register_batch_operation(iouxx::ring&, F)
        -> fixed_file_register_batch_operation<std::decay_t<F>>;

    template<typename F, typename... Args>
    fixed_file_register_batch_operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...)
        -> fixed_file_register_batch_operation<F>;

    // Reverse operation of fixed_file_register_operation
    // Create a normal fd from fixed file index
    template<utility::eligible_callback<file> Callback>
    class fixed_file_install_operation
    {
    public:
        template<utility::not_tag F>
        explicit fixed_file_install_operation(iouxx::ring& ring, F&& f)
            noexcept(utility::nothrow_constructible_callback<F>) :
            operation_base(iouxx::op_tag<fixed_file_install_operation>, ring),
            callback(std::forward<F>(f))
        {}

        template<typename F, typename... Args>
        explicit fixed_file_install_operation(iouxx::ring& ring, std::in_place_type_t<F>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<F, Args...>) :
            operation_base(iouxx::op_tag<fixed_file_install_operation>, ring),
            callback(std::forward<Args>(args)...)
        {}

        using callback_type = Callback;
        using result_type = fileops::file;

        static constexpr std::uint8_t opcode = IORING_OP_FILES_UPDATE;

        fixed_file_install_operation& file(fixed_file file) & noexcept {
            this->file_index = file.index();
            return *this;
        }

        fixed_file_install_operation& no_cloexec(bool set = true) & noexcept {
            this->no_cloexec_flag = set;
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_fixed_fd_install(sqe, file_index,
                no_cloexec_flag ? IORING_FIXED_FD_NO_CLOEXEC : 0);
        }

        void do_callback(int ev, std::uint32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
            utility::eligible_nothrow_callback<callback_type, result_type>) {
            if (ev >= 0) {
                std::invoke(callback, fileops::file(ev));
            } else {
                std::invoke(callback, utility::fail(-ev));
            }
        }

        int file_index = -1;
        bool no_cloexec_flag = false;
        [[no_unique_address]] callback_type callback;
    };

    template<utility::not_tag F>
    fixed_file_install_operation(iouxx::ring&, F)
        -> fixed_file_install_operation<std::decay_t<F>>;

    template<typename F, typename... Args>
    fixed_file_install_operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...)
        -> fixed_file_install_operation<F>;

} // namespace iouxx::inline iouops::fileops

#endif // IOUXX_OPERATION_FILE_CLOSE_H
