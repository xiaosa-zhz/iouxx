#pragma once
#ifndef IOUXX_OPERATION_FILEIO_H
#define IOUXX_OPERATION_FILEIO_H 1

#include <cstddef>
#include <utility>
#include <type_traits>

#include "macro_config.hpp"
#include "util/utility.hpp"
#include "iouringxx.hpp"
#include "file.hpp"
#include "openclose.hpp" // IWYU pragma: export

namespace iouxx::inline iouops {

    template<utility::eligible_callback<std::ptrdiff_t> Callback>
    class file_read_operation : public operation_base
    {
    public:
        template<typename F>
        explicit file_read_operation(iouxx::io_uring_xx& ring, F&& f)
            noexcept(utility::nothrow_constructible_callback<F>) :
            operation_base(iouxx::op_tag<file_read_operation>, ring),
            callback(std::forward<F>(f))
        {}

        template<typename F, typename... Args>
        explicit file_read_operation(iouxx::io_uring_xx& ring, std::in_place_type_t<F>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<F, Args...>) :
            operation_base(iouxx::op_tag<file_read_operation>, ring),
            callback(std::forward<Args>(args)...)
        {}

        using callback_type = Callback;
        using result_type = void;

        static constexpr std::uint8_t opcode = IORING_OP_READ;

        file_read_operation& file(file::file f) & noexcept {
            this->fd = f.native_handle();
            return *this;
        }

        template<utility::buffer_like Buffer>
        file_read_operation& buffer(Buffer&& buf) & noexcept {
            auto span = utility::to_buffer(std::forward<Buffer>(buf));
            this->buf = span.data();
            this->len = span.size();
            return *this;
        }

        file_read_operation& offset(std::size_t offset) & noexcept {
            this->off = offset;
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_read(sqe, fd, buf, len, off);
        }

        void do_callback(int ev, std::int32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
            utility::eligible_nothrow_callback<callback_type, result_type>) {
            if (ev >= 0) {
                std::invoke(callback, static_cast<std::ptrdiff_t>(ev));
            } else {
                std::invoke(callback, utility::fail(-ev));
            }
        }

        int fd = -1;
        void* buf = nullptr;
        std::size_t len = 0;
        std::size_t off = 0;
        callback_type callback;
    };

    template<typename F>
    file_read_operation(iouxx::io_uring_xx&, F) -> file_read_operation<std::decay_t<F>>;

    template<typename F, typename... Args>
    file_read_operation(iouxx::io_uring_xx&, std::in_place_type_t<F>, Args&&...) -> file_read_operation<F>;

    template<utility::eligible_callback<std::ptrdiff_t> Callback>
    class file_write_operation : public operation_base
    {
    public:
        template<typename F>
        explicit file_write_operation(iouxx::io_uring_xx& ring, F&& f)
            noexcept(utility::nothrow_constructible_callback<F>) :
            operation_base(iouxx::op_tag<file_write_operation>, ring),
            callback(std::forward<F>(f))
        {}

        template<typename F, typename... Args>
        explicit file_write_operation(iouxx::io_uring_xx& ring, std::in_place_type_t<F>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<F, Args...>) :
            operation_base(iouxx::op_tag<file_write_operation>, ring),
            callback(std::forward<Args>(args)...)
        {}

        using callback_type = Callback;
        using result_type = void;

        static constexpr std::uint8_t opcode = IORING_OP_WRITE;

        file_write_operation& file(file::file f) & noexcept {
            this->fd = f.native_handle();
            return *this;
        }

        template<utility::readonly_buffer_like Buffer>
        file_write_operation& buffer(Buffer&& buf) & noexcept {
            auto span = utility::to_readonly_buffer(std::forward<Buffer>(buf));
            this->buf = span.data();
            this->len = span.size();
            return *this;
        }

        file_write_operation& offset(std::size_t offset) & noexcept {
            this->off = offset;
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_write(sqe, fd, buf, len, off);
        }

        void do_callback(int ev, std::int32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
            utility::eligible_nothrow_callback<callback_type, result_type>) {
            if (ev >= 0) {
                std::invoke(callback, static_cast<std::ptrdiff_t>(ev));
            } else {
                std::invoke(callback, utility::fail(-ev));
            }
        }

        int fd = -1;
        const void* buf = nullptr;
        std::size_t len = 0;
        std::size_t off = 0;
        callback_type callback;
    };

    template<typename F>
    file_write_operation(iouxx::io_uring_xx&, F) -> file_write_operation<std::decay_t<F>>;
    
    template<typename F, typename... Args>
    file_write_operation(iouxx::io_uring_xx&, std::in_place_type_t<F>, Args&&...) -> file_write_operation<F>;

} // namespace iouxx::iouops

#endif // IOUXX_OPERATION_FILEIO_H
