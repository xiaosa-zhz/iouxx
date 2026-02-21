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

IOUXX_EXPORT
namespace iouxx::inline iouops::fileops {

    enum class rw_flag {
        none    = 0,
        hipri   = RWF_HIPRI,
        dsync   = RWF_DSYNC,
        sync    = RWF_SYNC,
        nowait  = RWF_NOWAIT,
        append  = RWF_APPEND,
    };

    constexpr rw_flag operator|(rw_flag lhs, rw_flag rhs) noexcept {
        return static_cast<rw_flag>(
            std::to_underlying(lhs) | std::to_underlying(rhs)
        );
    }

    constexpr rw_flag& operator|=(rw_flag& lhs, rw_flag rhs) noexcept {
        lhs = lhs | rhs;
        return lhs;
    }

} // namespace iouxx::iouops::fileops

namespace iouxx::details {

    class file_read_buffer_operation_base
    {
    public:
        template<typename Self, utility::buffer_like Buffer>
        Self& buffer(this Self& self, Buffer&& buf) noexcept {
            auto span = utility::to_buffer(std::forward<Buffer>(buf));
            self.buf = span.data();
            self.len = span.size();
            return self;
        }

    protected:
        void* buf = nullptr;
        std::size_t len = 0;
    };

    class file_write_buffer_operation_base
    {
    public:
        template<typename Self, utility::readonly_buffer_like Buffer>
        Self& buffer(this Self& self, Buffer&& buf) noexcept {
            auto span = utility::to_readonly_buffer(std::forward<Buffer>(buf));
            self.buf = span.data();
            self.len = span.size();
            return self;
        }

    protected:
        const void* buf = nullptr;
        std::size_t len = 0;
    };

    class file_read_write_operation_base
    {
    public:
        template<typename Self>
        Self& file(this Self& self, fileops::file f) noexcept {
            self.fd = f.native_handle();
            self.is_fixed = false;
            return self;
        }

        template<typename Self>
        Self& file(this Self& self, fileops::fixed_file f) noexcept {
            self.fd = f.index();
            self.is_fixed = true;
            return self;
        }

        template<typename Self>
        Self& offset(this Self& self, std::size_t offset) noexcept {
            self.off = offset;
            return self;
        }

    protected:
        bool is_fixed = false;
        int fd = -1;
        std::size_t off = 0;
    };

    class rw_flag_base
    {
    public:
        template<typename Self>
        Self& options(this Self& self, fileops::rw_flag flags) noexcept {
            self.flags = flags;
            return self;
        }

    protected:
        fileops::rw_flag flags = fileops::rw_flag::none;
    };

    template<std::size_t MaxIOvecs>
    class vectored_read_base
    {
        static constexpr std::size_t max_iovecs = MaxIOvecs;
    public:
        template<typename Self, utility::buffer_like Buffer>
        Self& buffer(this Self& self, std::size_t off, Buffer&& buf) noexcept {
            auto span = utility::to_buffer(std::forward<Buffer>(buf));
            self.iovecs[off].iov_base = span.data();
            self.iovecs[off].iov_len = span.size();
            return self;
        }

    protected:
        std::array<::iovec, max_iovecs> iovecs = {};
    };

    template<std::size_t MaxIOvecs>
    class vectored_write_base
    {
        static constexpr std::size_t max_iovecs = MaxIOvecs;
    public:
        template<typename Self, utility::readonly_buffer_like Buffer>
        Self& buffer(this Self& self, std::size_t off, Buffer&& buf) noexcept {
            auto span = utility::to_readonly_buffer(std::forward<Buffer>(buf));
            self.iovecs[off].iov_base = const_cast<void*>(
                static_cast<const void*>(span.data())
            );
            self.iovecs[off].iov_len = span.size();
            return self;
        }

    protected:
        std::array<::iovec, max_iovecs> iovecs = {};
    };

} // namespace iouxx::details

IOUXX_EXPORT
namespace iouxx::inline iouops::fileops {

    template<utility::eligible_callback<std::ptrdiff_t> Callback>
    class file_read_operation final : public operation_base,
        public details::file_read_write_operation_base,
        public details::file_read_buffer_operation_base
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
        using result_type = std::ptrdiff_t;

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
                std::invoke_r<void>(callback, static_cast<std::ptrdiff_t>(ev));
            } else {
                std::invoke_r<void>(callback, utility::fail(-ev));
            }
        }

        [[no_unique_address]] callback_type callback;
    };

    template<utility::not_tag F>
    file_read_operation(iouxx::ring&, F) -> file_read_operation<std::decay_t<F>>;

    template<typename F, typename... Args>
    file_read_operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...) -> file_read_operation<F>;

    template<utility::eligible_callback<std::ptrdiff_t> Callback>
    class file_read_fixed_operation final : public operation_base,
        public details::file_read_write_operation_base,
        public details::file_read_buffer_operation_base
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
        using result_type = std::ptrdiff_t;

        static constexpr std::uint8_t opcode = IORING_OP_READ_FIXED;

        file_read_fixed_operation& index(int index) & noexcept {
            this->buf_index = index;
            return *this;
        }

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
                std::invoke_r<void>(callback, static_cast<std::ptrdiff_t>(ev));
            } else {
                std::invoke_r<void>(callback, utility::fail(-ev));
            }
        }

        int buf_index = alloc_index;
        [[no_unique_address]] callback_type callback;
    };

    template<utility::not_tag F>
    file_read_fixed_operation(iouxx::ring&, F) -> file_read_fixed_operation<std::decay_t<F>>;

    template<typename F, typename... Args>
    file_read_fixed_operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...)
        -> file_read_fixed_operation<F>;

    template<std::size_t MaxIOvecs>
    class file_readv
    {
        static constexpr std::size_t max_iovecs = MaxIOvecs;
    public:
        template<utility::eligible_callback<std::ptrdiff_t> Callback>
        class operation final : public operation_base,
            public details::file_read_write_operation_base,
            public details::rw_flag_base,
            public details::vectored_read_base<max_iovecs>
        {
        public:
            template<utility::not_tag F>
            explicit operation(iouxx::ring& ring, F&& f)
                noexcept(utility::nothrow_constructible_callback<F>) :
                operation_base(iouxx::op_tag<operation>, ring),
                callback(std::forward<F>(f))
            {}

            template<typename F, typename... Args>
            explicit operation(iouxx::ring& ring, std::in_place_type_t<F>, Args&&... args)
                noexcept(std::is_nothrow_constructible_v<F, Args...>) :
                operation_base(iouxx::op_tag<operation>, ring),
                callback(std::forward<Args>(args)...)
            {}

            using callback_type = Callback;
            using result_type = std::ptrdiff_t;

            static constexpr std::uint8_t opcode = IORING_OP_READV;

        private:
            friend operation_base;
            void build(::io_uring_sqe* sqe) & noexcept {
                ::io_uring_prep_readv2(sqe, fd,
                    this->iovecs.data(), this->iovecs.size(),
                    off, std::to_underlying(flags));
                if (is_fixed) {
                    sqe->flags |= IOSQE_FIXED_FILE;
                }
            }

            void do_callback(int ev, std::uint32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
                utility::eligible_nothrow_callback<callback_type, result_type>) {
                if (ev >= 0) {
                    std::invoke_r<void>(callback, static_cast<std::ptrdiff_t>(ev));
                } else {
                    std::invoke_r<void>(callback, utility::fail(-ev));
                }
            }

            [[no_unique_address]] callback_type callback;
        };

        template<utility::eligible_callback<std::ptrdiff_t> F>
        operation(iouxx::ring&, F) -> operation<std::decay_t<F>>;

        template<typename F, typename... Args>
        operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...) -> operation<F>;
    };

    template<std::size_t MaxIOvecs>
    class file_readv_fixed
    {
        static constexpr std::size_t max_iovecs = MaxIOvecs;
    public:
        template<utility::eligible_callback<std::ptrdiff_t> Callback>
        class operation final : public operation_base,
            public details::file_read_write_operation_base,
            public details::rw_flag_base,
            public details::vectored_read_base<max_iovecs>
        {
        public:
            template<utility::not_tag F>
            explicit operation(iouxx::ring& ring, F&& f)
                noexcept(utility::nothrow_constructible_callback<F>) :
                operation_base(iouxx::op_tag<operation>, ring),
                callback(std::forward<F>(f))
            {}

            template<typename F, typename... Args>
            explicit operation(iouxx::ring& ring, std::in_place_type_t<F>, Args&&... args)
                noexcept(std::is_nothrow_constructible_v<F, Args...>) :
                operation_base(iouxx::op_tag<operation>, ring),
                callback(std::forward<Args>(args)...)
            {}

            using callback_type = Callback;
            using result_type = std::ptrdiff_t;

            static constexpr std::uint8_t opcode = IORING_OP_READV_FIXED;

            operation& index(int index) & noexcept {
                this->buf_index = index;
                return *this;
            }

        private:
            friend operation_base;
            void build(::io_uring_sqe* sqe) & noexcept {
                ::io_uring_prep_readv_fixed(sqe, fd,
                    this->iovecs.data(), this->iovecs.size(),
                    off, std::to_underlying(flags),
                    buf_index);
                if (is_fixed) {
                    sqe->flags |= IOSQE_FIXED_FILE;
                }
            }

            void do_callback(int ev, std::uint32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
                utility::eligible_nothrow_callback<callback_type, result_type>) {
                if (ev >= 0) {
                    std::invoke_r<void>(callback, static_cast<std::ptrdiff_t>(ev));
                } else {
                    std::invoke_r<void>(callback, utility::fail(-ev));
                }
            }

            int buf_index = alloc_index;
            [[no_unique_address]] callback_type callback;
        };

        template<utility::eligible_callback<std::ptrdiff_t> F>
        operation(iouxx::ring&, F) -> operation<std::decay_t<F>>;

        template<typename F, typename... Args>
        operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...) -> operation<F>;
    };

    template<utility::eligible_callback<std::ptrdiff_t> Callback>
    class file_write_operation final : public operation_base,
        public details::file_read_write_operation_base,
        public details::file_write_buffer_operation_base
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
        using result_type = std::ptrdiff_t;

        static constexpr std::uint8_t opcode = IORING_OP_WRITE;

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_write(sqe, fd, buf, len, off);
            if (is_fixed) {
                sqe->flags |= IOSQE_FIXED_FILE;
            }
        }

        void do_callback(int ev, std::uint32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
            utility::eligible_nothrow_callback<callback_type, result_type>) {
            if (ev >= 0) {
                std::invoke_r<void>(callback, static_cast<std::ptrdiff_t>(ev));
            } else {
                std::invoke_r<void>(callback, utility::fail(-ev));
            }
        }

        [[no_unique_address]] callback_type callback;
    };

    template<utility::not_tag F>
    file_write_operation(iouxx::ring&, F) -> file_write_operation<std::decay_t<F>>;

    template<typename F, typename... Args>
    file_write_operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...) -> file_write_operation<F>;

    template<utility::eligible_callback<std::ptrdiff_t> Callback>
    class file_write_fixed_operation final : public operation_base,
        public details::file_read_write_operation_base,
        public details::file_write_buffer_operation_base
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
        using result_type = std::ptrdiff_t;

        static constexpr std::uint8_t opcode = IORING_OP_WRITE_FIXED;

        file_write_fixed_operation& index(int index) & noexcept {
            this->buf_index = index;
            return *this;
        }

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
                std::invoke_r<void>(callback, static_cast<std::ptrdiff_t>(ev));
            } else {
                std::invoke_r<void>(callback, utility::fail(-ev));
            }
        }

        int buf_index = alloc_index;
        [[no_unique_address]] callback_type callback;
    };

    template<utility::not_tag F>
    file_write_fixed_operation(iouxx::ring&, F) -> file_write_fixed_operation<std::decay_t<F>>;

    template<typename F, typename... Args>
    file_write_fixed_operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...)
        -> file_write_fixed_operation<F>;

    template<std::size_t MaxIOvecs>
    class file_writev
    {
        static constexpr std::size_t max_iovecs = MaxIOvecs;
    public:
        template<utility::eligible_callback<std::ptrdiff_t> Callback>
        class operation final : public operation_base,
            public details::file_read_write_operation_base,
            public details::rw_flag_base,
            public details::vectored_write_base<max_iovecs>
        {
        public:
            template<utility::not_tag F>
            explicit operation(iouxx::ring& ring, F&& f)
                noexcept(utility::nothrow_constructible_callback<F>) :
                operation_base(iouxx::op_tag<operation>, ring),
                callback(std::forward<F>(f))
            {}

            template<typename F, typename... Args>
            explicit operation(iouxx::ring& ring, std::in_place_type_t<F>, Args&&... args)
                noexcept(std::is_nothrow_constructible_v<F, Args...>) :
                operation_base(iouxx::op_tag<operation>, ring),
                callback(std::forward<Args>(args)...)
            {}

            using callback_type = Callback;
            using result_type = std::ptrdiff_t;

            static constexpr std::uint8_t opcode = IORING_OP_WRITEV;

        private:
            friend operation_base;
            void build(::io_uring_sqe* sqe) & noexcept {
                ::io_uring_prep_writev2(sqe, fd,
                    this->iovecs.data(), this->iovecs.size(),
                    off, std::to_underlying(flags));
                if (is_fixed) {
                    sqe->flags |= IOSQE_FIXED_FILE;
                }
            }

            void do_callback(int ev, std::uint32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
                utility::eligible_nothrow_callback<callback_type, result_type>) {
                if (ev >= 0) {
                    std::invoke_r<void>(callback, static_cast<std::ptrdiff_t>(ev));
                } else {
                    std::invoke_r<void>(callback, utility::fail(-ev));
                }
            }

            [[no_unique_address]] callback_type callback;
        };

        template<utility::eligible_callback<std::ptrdiff_t> F>
        operation(iouxx::ring&, F) -> operation<std::decay_t<F>>;

        template<typename F, typename... Args>
        operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...) -> operation<F>;
    };

    template<std::size_t MaxIOvecs>
    class file_writev_fixed
    {
        static constexpr std::size_t max_iovecs = MaxIOvecs;
    public:
        template<utility::eligible_callback<std::ptrdiff_t> Callback>
        class operation final : public operation_base,
            public details::file_read_write_operation_base,
            public details::rw_flag_base,
            public details::vectored_write_base<max_iovecs>
        {
        public:
            template<utility::not_tag F>
            explicit operation(iouxx::ring& ring, F&& f)
                noexcept(utility::nothrow_constructible_callback<F>) :
                operation_base(iouxx::op_tag<operation>, ring),
                callback(std::forward<F>(f))
            {}

            template<typename F, typename... Args>
            explicit operation(iouxx::ring& ring, std::in_place_type_t<F>, Args&&... args)
                noexcept(std::is_nothrow_constructible_v<F, Args...>) :
                operation_base(iouxx::op_tag<operation>, ring),
                callback(std::forward<Args>(args)...)
            {}

            using callback_type = Callback;
            using result_type = std::ptrdiff_t;

            static constexpr std::uint8_t opcode = IORING_OP_WRITEV_FIXED;

            operation& index(int index) & noexcept {
                this->buf_index = index;
                return *this;
            }

        private:
            friend operation_base;
            void build(::io_uring_sqe* sqe) & noexcept {
                ::io_uring_prep_writev_fixed(sqe, fd,
                    this->iovecs.data(), this->iovecs.size(),
                    off, std::to_underlying(flags),
                    buf_index);
                if (is_fixed) {
                    sqe->flags |= IOSQE_FIXED_FILE;
                }
            }

            void do_callback(int ev, std::uint32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
                utility::eligible_nothrow_callback<callback_type, result_type>) {
                if (ev >= 0) {
                    std::invoke_r<void>(callback, static_cast<std::ptrdiff_t>(ev));
                } else {
                    std::invoke_r<void>(callback, utility::fail(-ev));
                }
            }

            int buf_index = alloc_index;
            [[no_unique_address]] callback_type callback;
        };

        template<utility::eligible_callback<std::ptrdiff_t> F>
        operation(iouxx::ring&, F) -> operation<std::decay_t<F>>;

        template<typename F, typename... Args>
        operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...) -> operation<F>;
    };

} // namespace iouxx::iouops::fileops

#endif // IOUXX_OPERATION_FILEIO_H
