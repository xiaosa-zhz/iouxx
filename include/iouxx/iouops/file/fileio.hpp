#pragma once
#ifndef IOUXX_OPERATION_FILEIO_H
#define IOUXX_OPERATION_FILEIO_H 1

#ifndef IOUXX_USE_CXX_MODULE

#include <cstddef>
#include <utility>
#include <type_traits>

#include "iouxx/macro_config.hpp"
#include "iouxx/util/utility.hpp"
#include "iouxx/iouringxx.hpp"
#include "file.hpp"
#include "openclose.hpp" // IWYU pragma: export

#endif // IOUXX_USE_CXX_MODULE

namespace iouxx::details {

    class file_read_write_operation_base
    {
    public:
        template<typename Self>
        Self& file(this Self& self, iouops::file::file f) noexcept {
            self.fd = f.native_handle();
            self.is_fixed = false;
            return self;
        }

        template<typename Self>
        Self& file(this Self& self, iouops::file::fixed_file f) noexcept {
            self.fd = f.index();
            self.is_fixed = true;
            return self;
        }

        template<typename Self, utility::buffer_like Buffer>
        Self& buffer(this Self& self, Buffer&& buf) noexcept {
            auto span = utility::to_buffer(std::forward<Buffer>(buf));
            self.buf = span.data();
            self.len = span.size();
            return self;
        }

        template<typename Self>
        Self& offset(this Self& self, std::size_t offset) noexcept {
            self.off = offset;
            return self;
        }

    protected:
        int fd = -1;
        bool is_fixed = false;
        void* buf = nullptr;
        std::size_t len = 0;
        std::size_t off = 0;
    };

} // namespace iouxx::details

IOUXX_EXPORT
namespace iouxx::inline iouops::file {

    template<utility::eligible_callback<std::ptrdiff_t> Callback>
    class file_read_operation
        : public operation_base, public details::file_read_write_operation_base
    {
    public:
        template<utility::not_tag F>
        explicit file_read_operation(iouxx::ring& ring, F&& f)
            noexcept(utility::nothrow_constructible_callback<F>) :
            operation_base(iouxx::op_tag<file_read_operation>, ring),
            callback(std::forward<F>(f))
        {}

        template<typename F, typename... Args>
        explicit file_read_operation(iouxx::ring& ring, std::in_place_type_t<F>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<F, Args...>) :
            operation_base(iouxx::op_tag<file_read_operation>, ring),
            callback(std::forward<Args>(args)...)
        {}

        using callback_type = Callback;
        using result_type = void;

        static constexpr std::uint8_t opcode = IORING_OP_READ;

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_read(sqe, fd, buf, len, off);
            if (is_fixed) {
                sqe->flags |= IOSQE_FIXED_FILE;
            }
        }

        void do_callback(int ev, std::uint32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
            utility::eligible_nothrow_callback<callback_type, result_type>) {
            if (ev >= 0) {
                std::invoke(callback, static_cast<std::ptrdiff_t>(ev));
            } else {
                std::invoke(callback, utility::fail(-ev));
            }
        }

        [[no_unique_address]] callback_type callback;
    };

    template<utility::not_tag F>
    file_read_operation(iouxx::ring&, F) -> file_read_operation<std::decay_t<F>>;

    template<typename F, typename... Args>
    file_read_operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...) -> file_read_operation<F>;

    template<utility::eligible_callback<std::ptrdiff_t> Callback>
    class file_read_fixed_operation
        : public operation_base, public details::file_read_write_operation_base
    {
    public:
        template<utility::not_tag F>
        explicit file_read_fixed_operation(iouxx::ring& ring, F&& f)
            noexcept(utility::nothrow_constructible_callback<F>) :
            operation_base(iouxx::op_tag<file_read_fixed_operation>, ring),
            callback(std::forward<F>(f))
        {}

        template<typename F, typename... Args>
        explicit file_read_fixed_operation(iouxx::ring& ring, std::in_place_type_t<F>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<F, Args...>) :
            operation_base(iouxx::op_tag<file_read_fixed_operation>, ring),
            callback(std::forward<Args>(args)...)
        {}

        using callback_type = Callback;
        using result_type = void;

        static constexpr std::uint8_t opcode = IORING_OP_READ_FIXED;

        template<utility::buffer_like Buffer>
        file_read_fixed_operation& buffer(Buffer&& buf, int index) & noexcept {
            this->file_read_write_operation_base::buffer(std::forward<Buffer>(buf));
            this->buf_index = index;
            return *this;
        }

        // Shadow the file_read_write_operation_base::buffer method to avoid misuse
        template<utility::buffer_like Buffer>
        file_read_fixed_operation& buffer(Buffer&&) & noexcept = delete;

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_read_fixed(sqe, fd, buf, len, off, buf_index);
            if (is_fixed) {
                sqe->flags |= IOSQE_FIXED_FILE;
            }
        }

        void do_callback(int ev, std::uint32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
            utility::eligible_nothrow_callback<callback_type, result_type>) {
            if (ev >= 0) {
                std::invoke(callback, static_cast<std::ptrdiff_t>(ev));
            } else {
                std::invoke(callback, utility::fail(-ev));
            }
        }

        int buf_index = -1;
        [[no_unique_address]] callback_type callback;
    };

    template<utility::not_tag F>
    file_read_fixed_operation(iouxx::ring&, F) -> file_read_fixed_operation<std::decay_t<F>>;

    template<typename F, typename... Args>
    file_read_fixed_operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...)
        -> file_read_fixed_operation<F>;

    template<utility::eligible_callback<std::ptrdiff_t> Callback>
    class file_write_operation
        : public operation_base, public details::file_read_write_operation_base
    {
    public:
        template<utility::not_tag F>
        explicit file_write_operation(iouxx::ring& ring, F&& f)
            noexcept(utility::nothrow_constructible_callback<F>) :
            operation_base(iouxx::op_tag<file_write_operation>, ring),
            callback(std::forward<F>(f))
        {}

        template<typename F, typename... Args>
        explicit file_write_operation(iouxx::ring& ring, std::in_place_type_t<F>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<F, Args...>) :
            operation_base(iouxx::op_tag<file_write_operation>, ring),
            callback(std::forward<Args>(args)...)
        {}

        using callback_type = Callback;
        using result_type = void;

        static constexpr std::uint8_t opcode = IORING_OP_WRITE;

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_write(sqe, fd, buf, len, off);
        }

        void do_callback(int ev, std::uint32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
            utility::eligible_nothrow_callback<callback_type, result_type>) {
            if (ev >= 0) {
                std::invoke(callback, static_cast<std::ptrdiff_t>(ev));
            } else {
                std::invoke(callback, utility::fail(-ev));
            }
        }

        [[no_unique_address]] callback_type callback;
    };

    template<utility::not_tag F>
    file_write_operation(iouxx::ring&, F) -> file_write_operation<std::decay_t<F>>;

    template<typename F, typename... Args>
    file_write_operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...) -> file_write_operation<F>;

    template<utility::eligible_callback<std::ptrdiff_t> Callback>
    class file_write_fixed_operation
        : public operation_base, public details::file_read_write_operation_base
    {
    public:
        template<utility::not_tag F>
        explicit file_write_fixed_operation(iouxx::ring& ring, F&& f)
            noexcept(utility::nothrow_constructible_callback<F>) :
            operation_base(iouxx::op_tag<file_write_fixed_operation>, ring),
            callback(std::forward<F>(f))
        {}

        template<typename F, typename... Args>
        explicit file_write_fixed_operation(iouxx::ring& ring, std::in_place_type_t<F>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<F, Args...>) :
            operation_base(iouxx::op_tag<file_write_fixed_operation>, ring),
            callback(std::forward<Args>(args)...)
        {}

        using callback_type = Callback;
        using result_type = void;

        static constexpr std::uint8_t opcode = IORING_OP_WRITE_FIXED;

        template<utility::buffer_like Buffer>
        file_write_fixed_operation& buffer(Buffer&& buf, int index) & noexcept {
            this->file_read_write_operation_base::buffer(std::forward<Buffer>(buf));
            this->buf_index = index;
            return *this;
        }

        // Shadow the file_read_write_operation_base::buffer method to avoid misuse
        template<utility::buffer_like Buffer>
        file_write_fixed_operation& buffer(Buffer&&) & noexcept = delete;

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_write_fixed(sqe, fd, buf, len, off, buf_index);
            if (is_fixed) {
                sqe->flags |= IOSQE_FIXED_FILE;
            }
        }

        void do_callback(int ev, std::uint32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
            utility::eligible_nothrow_callback<callback_type, result_type>) {
            if (ev >= 0) {
                std::invoke(callback, static_cast<std::ptrdiff_t>(ev));
            } else {
                std::invoke(callback, utility::fail(-ev));
            }
        }

        int buf_index = -1;
        [[no_unique_address]] callback_type callback;
    };

} // namespace iouxx::iouops::file

#endif // IOUXX_OPERATION_FILEIO_H
