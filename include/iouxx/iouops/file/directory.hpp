#pragma once
#ifndef IOUXX_OPERATION_FILE_DIRECTORY_H
#define IOUXX_OPERATION_FILE_DIRECTORY_H 1

#ifndef IOUXX_USE_CXX_MODULE

#include <string>
#include <type_traits>
#include <functional>
#include <utility>

#include "iouxx/macro_config.hpp"
#include "iouxx/util/utility.hpp"
#include "iouxx/iouringxx.hpp"
#include "file.hpp"
#include "openclose.hpp"

#endif // IOUXX_USE_CXX_MODULE

IOUXX_EXPORT
namespace iouxx::inline iouops::fileops {

    enum class unlink_flag : int {
        none = 0,
        removedir = AT_REMOVEDIR,
    };

    constexpr unlink_flag operator|(unlink_flag lhs, unlink_flag rhs) noexcept {
        return static_cast<unlink_flag>(
            std::to_underlying(lhs) | std::to_underlying(rhs)
        );
    }

    constexpr unlink_flag& operator|=(unlink_flag& lhs, unlink_flag rhs) noexcept {
        lhs = lhs | rhs;
        return lhs;
    }

    enum class rename_flag : unsigned int {
        none = 0,
        noreplace = RENAME_NOREPLACE,
        exchange = RENAME_EXCHANGE,
        whiteout = RENAME_WHITEOUT,
    };

    constexpr rename_flag operator|(rename_flag lhs, rename_flag rhs) noexcept {
        return static_cast<rename_flag>(
            std::to_underlying(lhs) | std::to_underlying(rhs)
        );
    }

    constexpr rename_flag& operator|=(rename_flag& lhs, rename_flag rhs) noexcept {
        lhs = lhs | rhs;
        return lhs;
    }

    enum class link_flag : int {
        none = 0,
        empty_path = AT_EMPTY_PATH,
        symlink_follow = AT_SYMLINK_FOLLOW,
    };

    constexpr link_flag operator|(link_flag lhs, link_flag rhs) noexcept {
        return static_cast<link_flag>(
            std::to_underlying(lhs) | std::to_underlying(rhs)
        );
    }

    constexpr link_flag& operator|=(link_flag& lhs, link_flag rhs) noexcept {
        lhs = lhs | rhs;
        return lhs;
    }

} // namespace iouxx::iouops::fileops

namespace iouxx::details {

    class single_path_directory_operation_base
    {
    public:
        template<typename Self, typename Path>
        Self& path(this Self& self, Path&& path) {
            self.pathstr = std::forward<Path>(path);
            return self;
        }

        template<typename Self>
        Self& directory(this Self& self, fileops::directory dir) noexcept {
            self.dirfd = dir.native_handle();
            return self;
        }

    protected:
        std::string pathstr;
        int dirfd = fileops::current_directory.native_handle();
    };

    class dual_path_directory_operation_base
    {
    public:
        template<typename Self, typename Path>
        Self& old_path(this Self& self, Path&& path) {
            self.oldpathstr = std::forward<Path>(path);
            return self;
        }

        template<typename Self>
        Self& old_directory(this Self& self, fileops::directory dir) noexcept {
            self.olddirfd = dir.native_handle();
            return self;
        }

        template<typename Self, typename Path>
        Self& new_path(this Self& self, Path&& path) {
            self.newpathstr = std::forward<Path>(path);
            return self;
        }

        template<typename Self>
        Self& new_directory(this Self& self, fileops::directory dir) noexcept {
            self.newdirfd = dir.native_handle();
            return self;
        }

    protected:
        std::string oldpathstr;
        int olddirfd = fileops::current_directory.native_handle();
        std::string newpathstr;
        int newdirfd = fileops::current_directory.native_handle();
    };

} // namespace iouxx::details

IOUXX_EXPORT
namespace iouxx::inline iouops::fileops {

    template<utility::eligible_callback<void> Callback>
    class unlink_operation final : public operation_base,
        public details::single_path_directory_operation_base
    {
    public:
        template<utility::not_tag F>
        explicit unlink_operation(iouxx::ring& ring, F&& f)
            noexcept(utility::nothrow_constructible_callback<F>) :
            operation_base(iouxx::op_tag<unlink_operation>, ring),
            callback(std::forward<F>(f))
        {}

        template<typename F, typename... Args>
        explicit unlink_operation(iouxx::ring& ring, std::in_place_type_t<F>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<F, Args...>) :
            operation_base(iouxx::op_tag<unlink_operation>, ring),
            callback(std::forward<Args>(args)...)
        {}

        using callback_type = Callback;
        using result_type = void;

        static constexpr std::uint8_t opcode = IORING_OP_UNLINKAT;

        unlink_operation& flags(unlink_flag flags) & noexcept {
            this->unlink_flags = std::to_underlying(flags);
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_unlinkat(sqe, dirfd, pathstr.c_str(), unlink_flags);
        }

        void do_callback(int ev, std::uint32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
            utility::eligible_nothrow_callback<callback_type, result_type>) {
            if constexpr (utility::stdexpected_callback<callback_type, void>) {
                if (ev == 0) {
                    std::invoke_r<void>(callback, utility::void_success());
                } else {
                    std::invoke_r<void>(callback, utility::fail(-ev));
                }
            } else if constexpr (utility::errorcode_callback<callback_type>) {
                std::invoke_r<void>(callback, utility::make_system_error_code(-ev));
            } else {
                static_assert(false, "Unreachable");
            }
        }

        int unlink_flags = 0;
        [[no_unique_address]] callback_type callback;
    };

    template<utility::not_tag F>
    unlink_operation(iouxx::ring&, F) -> unlink_operation<std::decay_t<F>>;

    template<typename F, typename... Args>
    unlink_operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...) -> unlink_operation<F>;

    template<utility::eligible_callback<void> Callback>
    class rename_operation final : public operation_base,
        public details::dual_path_directory_operation_base
    {
    public:
        template<utility::not_tag F>
        explicit rename_operation(iouxx::ring& ring, F&& f)
            noexcept(utility::nothrow_constructible_callback<F>) :
            operation_base(iouxx::op_tag<rename_operation>, ring),
            callback(std::forward<F>(f))
        {}

        template<typename F, typename... Args>
        explicit rename_operation(iouxx::ring& ring, std::in_place_type_t<F>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<F, Args...>) :
            operation_base(iouxx::op_tag<rename_operation>, ring),
            callback(std::forward<Args>(args)...)
        {}

        using callback_type = Callback;
        using result_type = void;

        static constexpr std::uint8_t opcode = IORING_OP_RENAMEAT;

        rename_operation& flags(rename_flag flags) & noexcept {
            this->rename_flags = std::to_underlying(flags);
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_renameat(sqe, olddirfd, oldpathstr.c_str(),
                newdirfd, newpathstr.c_str(), rename_flags);
        }

        void do_callback(int ev, std::uint32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
            utility::eligible_nothrow_callback<callback_type, result_type>) {
            if constexpr (utility::stdexpected_callback<callback_type, void>) {
                if (ev == 0) {
                    std::invoke_r<void>(callback, utility::void_success());
                } else {
                    std::invoke_r<void>(callback, utility::fail(-ev));
                }
            } else if constexpr (utility::errorcode_callback<callback_type>) {
                std::invoke_r<void>(callback, utility::make_system_error_code(-ev));
            } else {
                static_assert(false, "Unreachable");
            }
        }

        unsigned int rename_flags = 0;
        [[no_unique_address]] callback_type callback;
    };

    template<utility::not_tag F>
    rename_operation(iouxx::ring&, F) -> rename_operation<std::decay_t<F>>;

    template<typename F, typename... Args>
    rename_operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...) -> rename_operation<F>;

    template<utility::eligible_callback<void> Callback>
    class mkdir_operation final : public operation_base,
        public details::single_path_directory_operation_base
    {
    public:
        template<utility::not_tag F>
        explicit mkdir_operation(iouxx::ring& ring, F&& f)
            noexcept(utility::nothrow_constructible_callback<F>) :
            operation_base(iouxx::op_tag<mkdir_operation>, ring),
            callback(std::forward<F>(f))
        {}

        template<typename F, typename... Args>
        explicit mkdir_operation(iouxx::ring& ring, std::in_place_type_t<F>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<F, Args...>) :
            operation_base(iouxx::op_tag<mkdir_operation>, ring),
            callback(std::forward<Args>(args)...)
        {}

        using callback_type = Callback;
        using result_type = void;

        static constexpr std::uint8_t opcode = IORING_OP_MKDIRAT;

        mkdir_operation& mode(open_mode mode) & noexcept {
            this->dir_mode = std::to_underlying(mode);
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_mkdirat(sqe, dirfd, pathstr.c_str(), dir_mode);
        }

        void do_callback(int ev, std::uint32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
            utility::eligible_nothrow_callback<callback_type, result_type>) {
            if constexpr (utility::stdexpected_callback<callback_type, void>) {
                if (ev == 0) {
                    std::invoke_r<void>(callback, utility::void_success());
                } else {
                    std::invoke_r<void>(callback, utility::fail(-ev));
                }
            } else if constexpr (utility::errorcode_callback<callback_type>) {
                std::invoke_r<void>(callback, utility::make_system_error_code(-ev));
            } else {
                static_assert(false, "Unreachable");
            }
        }

        mode_t dir_mode = 0;
        [[no_unique_address]] callback_type callback;
    };

    template<utility::not_tag F>
    mkdir_operation(iouxx::ring&, F) -> mkdir_operation<std::decay_t<F>>;

    template<typename F, typename... Args>
    mkdir_operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...) -> mkdir_operation<F>;

    template<utility::eligible_callback<void> Callback>
    class symlink_operation final : public operation_base
    {
    public:
        template<utility::not_tag F>
        explicit symlink_operation(iouxx::ring& ring, F&& f)
            noexcept(utility::nothrow_constructible_callback<F>) :
            operation_base(iouxx::op_tag<symlink_operation>, ring),
            callback(std::forward<F>(f))
        {}

        template<typename F, typename... Args>
        explicit symlink_operation(iouxx::ring& ring, std::in_place_type_t<F>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<F, Args...>) :
            operation_base(iouxx::op_tag<symlink_operation>, ring),
            callback(std::forward<Args>(args)...)
        {}

        using callback_type = Callback;
        using result_type = void;

        static constexpr std::uint8_t opcode = IORING_OP_SYMLINKAT;

        template<typename Path>
        symlink_operation& target(Path&& path) & {
            this->targetstr = std::forward<Path>(path);
            return *this;
        }

        template<typename Path>
        symlink_operation& link_path(Path&& path) & {
            this->linkpathstr = std::forward<Path>(path);
            return *this;
        }

        symlink_operation& directory(fileops::directory dir) & noexcept {
            this->newdirfd = dir.native_handle();
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_symlinkat(sqe, targetstr.c_str(),
                newdirfd, linkpathstr.c_str());
        }

        void do_callback(int ev, std::uint32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
            utility::eligible_nothrow_callback<callback_type, result_type>) {
            if constexpr (utility::stdexpected_callback<callback_type, void>) {
                if (ev == 0) {
                    std::invoke_r<void>(callback, utility::void_success());
                } else {
                    std::invoke_r<void>(callback, utility::fail(-ev));
                }
            } else if constexpr (utility::errorcode_callback<callback_type>) {
                std::invoke_r<void>(callback, utility::make_system_error_code(-ev));
            } else {
                static_assert(false, "Unreachable");
            }
        }

        std::string targetstr;
        std::string linkpathstr;
        int newdirfd = current_directory.native_handle();
        [[no_unique_address]] callback_type callback;
    };

    template<utility::not_tag F>
    symlink_operation(iouxx::ring&, F) -> symlink_operation<std::decay_t<F>>;

    template<typename F, typename... Args>
    symlink_operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...) -> symlink_operation<F>;

    template<utility::eligible_callback<void> Callback>
    class link_operation final : public operation_base,
        public details::dual_path_directory_operation_base
    {
    public:
        template<utility::not_tag F>
        explicit link_operation(iouxx::ring& ring, F&& f)
            noexcept(utility::nothrow_constructible_callback<F>) :
            operation_base(iouxx::op_tag<link_operation>, ring),
            callback(std::forward<F>(f))
        {}

        template<typename F, typename... Args>
        explicit link_operation(iouxx::ring& ring, std::in_place_type_t<F>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<F, Args...>) :
            operation_base(iouxx::op_tag<link_operation>, ring),
            callback(std::forward<Args>(args)...)
        {}

        using callback_type = Callback;
        using result_type = void;

        static constexpr std::uint8_t opcode = IORING_OP_LINKAT;

        link_operation& flags(link_flag flags) & noexcept {
            this->link_flags = std::to_underlying(flags);
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_linkat(sqe, olddirfd, oldpathstr.c_str(),
                newdirfd, newpathstr.c_str(), link_flags);
        }

        void do_callback(int ev, std::uint32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
            utility::eligible_nothrow_callback<callback_type, result_type>) {
            if constexpr (utility::stdexpected_callback<callback_type, void>) {
                if (ev == 0) {
                    std::invoke_r<void>(callback, utility::void_success());
                } else {
                    std::invoke_r<void>(callback, utility::fail(-ev));
                }
            } else if constexpr (utility::errorcode_callback<callback_type>) {
                std::invoke_r<void>(callback, utility::make_system_error_code(-ev));
            } else {
                static_assert(false, "Unreachable");
            }
        }

        int link_flags = 0;
        [[no_unique_address]] callback_type callback;
    };

    template<utility::not_tag F>
    link_operation(iouxx::ring&, F) -> link_operation<std::decay_t<F>>;

    template<typename F, typename... Args>
    link_operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...) -> link_operation<F>;

} // namespace iouxx::inline iouops::fileops

#endif // IOUXX_OPERATION_FILE_DIRECTORY_H
