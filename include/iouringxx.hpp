#pragma once
#include <cstdint>
#ifndef IOUXX_LIBURING_CXX_WRAPER_H
#define IOUXX_LIBURING_CXX_WRAPER_H 1

/*
    * iouxx is a C++ wrapper for the liburing library.
*/

#include <liburing.h>

#include <concepts>
#include <type_traits>
#include <cstddef>
#include <utility>
#include <system_error>
#include <expected>
#include <span>
#include <ranges>
#include <vector>

#if defined(__cpp_contracts) && __cpp_contracts >= 202502L
#define assert(...) contract_assert(__VA_ARGS__)
#else // ! __cpp_contracts
#include <cassert>
#endif

namespace iouxx {

    // Forward declaration
    class io_uring_xx;

    namespace details {

        // Pre: ev >= 0
        inline std::error_code make_system_error_code(int ev) noexcept {
            if (ev != 0) {
                return std::error_code(ev, std::system_category());
            }
            return std::error_code();
        }

    } // namespace iouxx::details

    inline namespace iouops {

        // Forward declaration
        class operation_base;

        class operation_identifier
        {
        public:
            operation_identifier() = default;
            operation_identifier(const operation_identifier&) = default;
            operation_identifier& operator=(const operation_identifier&) = default;

            friend bool operator==(
                const operation_identifier& lhs,
                const operation_identifier& rhs) = default;

            friend auto operator<=>(
                const operation_identifier&,
                const operation_identifier&) = default;

            void* user_data() const noexcept {
                return static_cast<void*>(raw);
            }

        private:
            friend operation_base;
            explicit operation_identifier(operation_base* raw) noexcept
                : raw(raw)
            {}

            operation_base* raw = nullptr;
        };

        template<typename Operation>
        struct operation_t {
            using type = Operation;
        };

        // Tag for operation_base callback erasure
        template<typename Operation>
        inline constexpr operation_t<Operation> op_tag{};

        // Base class for operations.
        // Derived class must implement:
        //   void build() & noexcept;
        //   void do_callback(int ev, std::int32_t cqe_flags);
        // Warning:
        // User of operations should create and store operation objects
        // in their own context, make sure the operation object outlives
        // the whole execution of io_uring task.
        class operation_base
        {
        public:
            operation_base() = delete;
            operation_base(const operation_base&) = delete;
            operation_base& operator=(const operation_base&) = delete;
            operation_base(operation_base&&) = delete;
            operation_base& operator=(operation_base&&) = delete;

            template<typename Self>
            ::io_uring_sqe* to_sqe(this Self& self) noexcept {
                ::io_uring_sqe* sqe = ::io_uring_get_sqe(self.ring);
                if (!sqe) return nullptr;
                self.build(sqe); // Provided by derived class
                ::io_uring_sqe_set_data(sqe, static_cast<operation_base*>(&self));
                return sqe;
            }

            template<typename Self>
            std::error_code submit(this Self& self) noexcept {
                if (::io_uring_sqe* sqe = self.to_sqe()) {
                    int ev = ::io_uring_submit(self.ring);
                    if (ev < 0) {
                        return details::make_system_error_code(-ev);
                    }
                    return std::error_code();
                }
                return details::make_system_error_code(EAGAIN);
            }

            void callback(int ev, std::int32_t cqe_flags) {
                do_callback_ptr(this, ev, cqe_flags);
            }

            operation_identifier identifier() noexcept {
                return operation_identifier(this);
            }

        protected:
            template<typename Derived>
            static void callback_wrapper(operation_base* base, int ev, std::int32_t cqe_flags) {
                // Provided by derived class
                static_cast<Derived*>(base)->do_callback(ev, cqe_flags);
            }

            template<typename Derived>
            explicit operation_base(operation_t<Derived>, ::io_uring* ring) noexcept
                : do_callback_ptr(&callback_wrapper<Derived>), ring(ring)
            {}

            // Note:
            // Override method will receive raw error code from kernel, because:
            // 1. The positive value may be meaningful result,
            //    and it is operation rather than user-defined callback
            //    that knows what result means.
            // 2. Some error codes are not real error depends on context,
            //    e.g., -ETIME for pure timeout operation.
            //    The operation itself should decide how to handle it.
            using callback_type = void (*)(operation_base*, int, std::int32_t);

            callback_type do_callback_ptr = nullptr;
            ::io_uring* ring = nullptr;
        };

    } // namespace iouxx::iouops

    template<typename T>
    concept byte_unit = std::same_as<T, std::byte> || std::same_as<T, unsigned char>;

    template<typename R>
    concept buffer_like = std::constructible_from<std::span<std::byte>, std::remove_cvref_t<R>>
        || std::constructible_from<std::span<unsigned char>, std::remove_cvref_t<R>>;

    template<typename R>
    concept buffer_range = std::ranges::input_range<std::remove_cvref_t<R>>
        && buffer_like<std::ranges::range_value_t<std::remove_cvref_t<R>>>;

    namespace details {

        inline iouops::operation_base* from_user_data(void* data) noexcept {
            return static_cast<iouops::operation_base*>(data);
        }

        template<byte_unit ByteType, std::size_t N>
        inline ::iovec to_iovec(std::span<ByteType, N> buffer) noexcept {
            return ::iovec{
                .iov_base = buffer.data(),
                .iov_len = buffer.size()
            };
        }

        template<byte_unit ByteType>
        inline std::span<ByteType> from_iovec(const ::iovec& iov) noexcept {
            return std::span<ByteType>(
                static_cast<ByteType*>(iov.iov_base), iov.iov_len);
        }

    } // namespace iouxx::details

    class operation_result
    {
    public:
        operation_result() = delete;
        operation_result(const operation_result&) = default;
        operation_result& operator=(const operation_result&) = default;

        int result() const noexcept { return res; }
        int reset_result(int result = 0) noexcept {
            return std::exchange(res, result);
        }

        void operator()() const {
            cb->callback(res, cqe_flags);
        }

    private:
        friend io_uring_xx;
        explicit operation_result(io_uring_cqe* cqe) noexcept :
            cb(details::from_user_data(::io_uring_cqe_get_data(cqe))),
            res(cqe->res), cqe_flags(cqe->flags)
        {}

        iouops::operation_base* cb;
        std::int32_t res;
        std::uint32_t cqe_flags;
    };

    class io_uring_xx
    {
    public:
        io_uring_xx() = default;

        explicit io_uring_xx(std::size_t queue_depth) {
            std::error_code ec = do_init(queue_depth);
            if (ec) {
                throw std::system_error(ec, "Failed to initialize io_uring");
            }
        }

        io_uring_xx(const io_uring_xx&) = delete;
        io_uring_xx& operator=(const io_uring_xx&) = delete;

        io_uring_xx(io_uring_xx&& other) noexcept
            : ring(std::exchange(other.ring, invalid_ring()))
        {}

        io_uring_xx& operator=(io_uring_xx&& other) noexcept {
            if (this != &other) {
                exit();
                this->swap(other);
            }
            return *this;
        }

        void swap(io_uring_xx& other) noexcept {
            std::ranges::swap(ring, other.ring);
        }

        ~io_uring_xx() { exit(); }

        bool valid() const noexcept { return ring.ring_fd >= 0; }

        std::error_code reinit(std::size_t queue_depth) noexcept {
            exit();
            return do_init(queue_depth);
        }

        void exit() noexcept {
            if (valid()) {
                ::io_uring_queue_exit(&ring);
                ring = invalid_ring();
            }
        }

        std::error_code submit(::io_uring_sqe* sqe) noexcept {
            assert(valid());
            if (!sqe) {
                return details::make_system_error_code(EAGAIN);
            }
            int ev = ::io_uring_submit(&ring);
            if (ev < 0) {
                return details::make_system_error_code(-ev);
            }
            return std::error_code();
        }

        std::expected<operation_result, std::error_code> fetch_result() noexcept {
            assert(valid());
            ::io_uring_cqe* cqe = nullptr;
            int ev = ::io_uring_peek_cqe(&ring, &cqe);
            if (ev < 0) {
                return std::unexpected(details::make_system_error_code(-ev));
            }
            operation_result result(cqe);
            ::io_uring_cqe_seen(&ring, cqe);
            return result;
        }

        std::expected<operation_result, std::error_code> wait_for_result() noexcept {
            assert(valid());
            ::io_uring_cqe* cqe = nullptr;
            int ev = ::io_uring_wait_cqe(&ring, &cqe);
            if (ev < 0) {
                return std::unexpected(details::make_system_error_code(-ev));
            }
            operation_result result(cqe);
            ::io_uring_cqe_seen(&ring, cqe);
            return result;
        }

        template<buffer_range Buffers>
        std::error_code register_buffers(Buffers&& buffers) noexcept {
            assert(valid());
            std::vector<::iovec> iovecs = std::forward<Buffers>(buffers)
                | std::views::transform([]<typename Buffer>(Buffer&& buffer) {
                    using byte_type = std::ranges::range_value_t<std::remove_cvref_t<Buffer>>;
                    return details::to_iovec(
                        std::span<byte_type>(std::forward<Buffer>(buffer))
                    );
                })
                | std::ranges::to<std::vector<::iovec>>();
            int ev = ::io_uring_register_buffers(
                &ring, iovecs.data(), iovecs.size());
            return details::make_system_error_code(-ev);
        }

        // TODO
        std::error_code register_direct_descriptor_table(std::size_t size) noexcept {
            assert(valid());
            int ev = ::io_uring_register_files_sparse(&ring, size);
            return details::make_system_error_code(-ev);
        }

        ::io_uring& native() & noexcept {
            assert(valid());
            return ring;
        }

    private:
        static ::io_uring invalid_ring() noexcept {
            return { .ring_fd = -1, .enter_ring_fd = -1 };
        }

        std::error_code do_init(std::size_t queue_depth) noexcept {
            assert(!valid());
            int ev = ::io_uring_queue_init(queue_depth, &ring, 0);
            if (ev < 0) {
                ring = invalid_ring();
                return details::make_system_error_code(-ev);
            } else {
                return std::error_code();
            }
        }

        ::io_uring ring = invalid_ring(); // using ring_fd to detect if valid
    };

} // namespace iouxx

// Hash support for iouxx::iouops::operation_identifier
template<>
struct std::hash<iouxx::iouops::operation_identifier>
{
    std::size_t operator()(const iouxx::iouops::operation_identifier& id) const noexcept {
        return std::hash<void*>{}(id.user_data());
    }
};

#endif // IOUXX_LIBURING_CXX_WRAPER_H
