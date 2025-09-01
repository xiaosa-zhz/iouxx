#pragma once
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
#include <chrono>
#include <memory> // IWYU pragma: keep

#include "macro_config.hpp"
#include "util/utility.hpp"
#include "util/assertion.hpp"

namespace iouxx {

    // Forward declaration
    class io_uring_xx;

    inline namespace iouops {

        // Forward declaration
        class operation_base;

        struct dummy_callback {
            constexpr void operator()(auto&&) const noexcept {}
        };

        template<typename Result>
        struct sync_wait_callback {
            using expected_type = std::expected<Result, std::error_code>;
            expected_type result;

            void operator()(expected_type res) noexcept {
                result = std::move(res);
            }
        };

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

            std::uint64_t user_data64() const noexcept {
                return static_cast<std::uint64_t>(
                    reinterpret_cast<std::uintptr_t>(raw)
                );
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
                ::io_uring_sqe* sqe = ::io_uring_get_sqe(self.ring->native());
                if (!sqe) return nullptr;
                self.build(sqe); // Provided by derived class
                ::io_uring_sqe_set_data(sqe, static_cast<operation_base*>(&self));
                return sqe;
            }

            template<typename Self>
                requires (!utility::is_specialization_of_v<
                    sync_wait_callback, typename Self::callback_type>)
            std::error_code submit(this Self& self) noexcept {
#if IOUXX_IORING_FEATURE_TESTS_ENABLED == 1
                if (!::io_uring_opcode_supported(self.ring->ring_probe(),
                    Self::IORING_OPCODE)) {
                    return std::make_error_code(std::errc::function_not_supported);
                }
#endif // IOUXX_IORING_FEATURE_TESTS_ENABLED
                if (::io_uring_sqe* sqe = self.to_sqe()) {
                    int ev = ::io_uring_submit(self.ring->native());
                    if (ev < 0) {
                        return utility::make_system_error_code(-ev);
                    }
                    return std::error_code();
                }
                return std::make_error_code(std::errc::resource_unavailable_try_again);
            }

            template<typename Self>
                requires (utility::is_specialization_of_v<
                    sync_wait_callback, typename Self::callback_type>)
            auto submit_and_wait(this Self& self)
                noexcept -> typename Self::callback_type::expected_type {
#if IOUXX_IORING_FEATURE_TESTS_ENABLED == 1
                if (!::io_uring_opcode_supported(self.ring->ring_probe(),
                    Self::IORING_OPCODE)) {
                    return std::unexpected(
                        std::make_error_code(std::errc::function_not_supported)
                    );
                }
#endif // IOUXX_IORING_FEATURE_TESTS_ENABLED
                if (::io_uring_sqe* sqe = self.to_sqe()) {
                    int ev = ::io_uring_submit(self.ring->native());
                    if (ev < 0) {
                        return std::unexpected(
                            utility::make_system_error_code(-ev)
                        );
                    }
                    // wait
                    if (auto cqe_result = self.ring->wait_for_result()) {
                        cqe_result->callback();
                        return std::move(self.callback.result);
                    } else {
                        return std::unexpected(cqe_result.error());
                    }
                    
                }
                return std::unexpected(
                    std::make_error_code(std::errc::resource_unavailable_try_again)
                );
            }

            void callback(int ev, std::int32_t cqe_flags) &
                IOUXX_CALLBACK_NOEXCEPT {
                do_callback_ptr(this, ev, cqe_flags);
            }

            operation_identifier identifier() & noexcept {
                return operation_identifier(this);
            }

        protected:
            template<typename Derived>
            static void callback_wrapper(operation_base* base, int ev, std::int32_t cqe_flags)
                IOUXX_CALLBACK_NOEXCEPT {
                // Provided by derived class
                static_cast<Derived*>(base)->do_callback(ev, cqe_flags);
            }

            template<typename Derived>
            explicit operation_base(operation_t<Derived>, io_uring_xx& ring) noexcept
                : do_callback_ptr(&callback_wrapper<Derived>), ring(&ring)
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
            io_uring_xx* ring = nullptr;
        };

    } // namespace iouxx::iouops

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

        void callback() const IOUXX_CALLBACK_NOEXCEPT {
            cb->callback(res, cqe_flags);
        }

        void operator()() const IOUXX_CALLBACK_NOEXCEPT {
            callback();
        }

    private:
        friend io_uring_xx;
        explicit operation_result(io_uring_cqe* cqe) noexcept :
            cb(from_user_data(::io_uring_cqe_get_data(cqe))),
            res(cqe->res), cqe_flags(cqe->flags)
        {}

        static iouops::operation_base* from_user_data(void* data) noexcept {
            return static_cast<iouops::operation_base*>(data);
        }

        iouops::operation_base* cb;
        std::int32_t res;
        std::uint32_t cqe_flags;
    };

    class io_uring_xx
    {
    public:
        explicit io_uring_xx() = default;

        explicit io_uring_xx(std::size_t queue_depth) {
            std::error_code ec = do_init(queue_depth);
            if (ec) {
                throw std::system_error(ec, "Failed to initialize io_uring");
            }
        }

        io_uring_xx(const io_uring_xx&) = delete;
        io_uring_xx& operator=(const io_uring_xx&) = delete;
        io_uring_xx(io_uring_xx&& other) = delete;
        io_uring_xx& operator=(io_uring_xx&& other) = delete;

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
#if IOUXX_IORING_FEATURE_TESTS_ENABLED == 1
                probe.reset();
#endif // IOUXX_IORING_FEATURE_TESTS_ENABLED
                ::io_uring_queue_exit(&ring);
                ring = invalid_ring();
            }
        }

        // Explicitly specify operation type to create
        template<template<typename...> class Operation, typename Callback>
        auto make(Callback&& callback) & noexcept {
            assert(valid());
            return Operation(*this, std::forward<Callback>(callback));
        }

        // Explicitly specify operation type to create
        template<template<typename...> class Operation>
        auto make() & noexcept {
            assert(valid());
            return Operation(*this);
        }

        template<template<typename...> class Operation>
        auto make_sync() & noexcept {
            assert(valid());
            using result_type = typename Operation<dummy_callback>::result_type;
            return Operation(*this, sync_wait_callback<result_type>{});
        }

        std::error_code submit(::io_uring_sqe* sqe) noexcept {
            assert(valid());
            if (!sqe) {
                return std::make_error_code(std::errc::resource_unavailable_try_again);
            }
            int ev = ::io_uring_submit(&ring);
            if (ev < 0) {
                return utility::make_system_error_code(-ev);
            }
            return std::error_code();
        }

        std::expected<operation_result, std::error_code> fetch_result() noexcept {
            assert(valid());
            ::io_uring_cqe* cqe = nullptr;
            int ev = ::io_uring_peek_cqe(&ring, &cqe);
            if (ev < 0) {
                return std::unexpected(utility::make_system_error_code(-ev));
            }
            operation_result result(cqe);
            ::io_uring_cqe_seen(&ring, cqe);
            return result;
        }

        auto wait_for_result(std::chrono::nanoseconds timeout = {})
            noexcept -> std::expected<operation_result, std::error_code> {
            assert(valid());
            ::io_uring_cqe* cqe = nullptr;
            int ev = 0;
            if (timeout.count() != 0) {
                auto ts = utility::to_kernel_timespec(timeout);
                ev = ::io_uring_wait_cqe_timeout(&ring, &cqe, &ts);
            } else {
                ev = ::io_uring_wait_cqe(&ring, &cqe);
            }
            if (ev < 0) {
                return std::unexpected(utility::make_system_error_code(-ev));
            }
            operation_result result(cqe);
            ::io_uring_cqe_seen(&ring, cqe);
            return result;
        }

        template<utility::buffer_range Buffers>
        std::error_code register_buffers(Buffers&& buffers) noexcept {
            assert(valid());
            std::vector<::iovec> iovecs = std::forward<Buffers>(buffers)
                | std::views::transform([]<typename Buffer>(Buffer&& buffer) {
                    using byte_type = std::ranges::range_value_t<std::remove_cvref_t<Buffer>>;
                    return utility::to_iovec(
                        std::span<byte_type>(std::forward<Buffer>(buffer))
                    );
                })
                | std::ranges::to<std::vector<::iovec>>();
            int ev = ::io_uring_register_buffers(
                &ring, iovecs.data(), iovecs.size());
            return utility::make_system_error_code(-ev);
        }

        // TODO
        std::error_code register_direct_descriptor_table(std::size_t size) noexcept {
            assert(valid());
            int ev = ::io_uring_register_files_sparse(&ring, size);
            return utility::make_system_error_code(-ev);
        }

        ::io_uring* native() & noexcept {
            assert(valid());
            return &ring;
        }

#if IOUXX_IORING_FEATURE_TESTS_ENABLED == 1
        ::io_uring_probe* ring_probe() const noexcept {
            return probe.get();
        }
#endif // IOUXX_IORING_FEATURE_TESTS_ENABLED

    private:
        static ::io_uring invalid_ring() noexcept {
            return { .ring_fd = -1, .enter_ring_fd = -1 };
        }

        std::error_code do_init(std::size_t queue_depth) noexcept {
            assert(!valid());
            int ev = ::io_uring_queue_init(queue_depth, &ring, 0);
            if (ev < 0) {
                ring = invalid_ring();
                return utility::make_system_error_code(-ev);
            }
#if IOUXX_IORING_FEATURE_TESTS_ENABLED == 1
            if (::io_uring_probe* raw = ::io_uring_get_probe_ring(&ring)) {
                probe.reset(raw);
            } else {
                exit();
                return std::make_error_code(std::errc::not_enough_memory);
            }
#endif // IOUXX_IORING_FEATURE_TESTS_ENABLED
            return std::error_code();
        }

        ::io_uring ring = invalid_ring(); // using ring_fd to detect if valid
#if IOUXX_IORING_FEATURE_TESTS_ENABLED == 1
        struct probe_deleter {
            void operator()(::io_uring_probe* probe) const noexcept {
                ::io_uring_free_probe(probe);
            }
        };
        using probe_handle = std::unique_ptr<::io_uring_probe, probe_deleter>;
        probe_handle probe = nullptr;
#endif // IOUXX_IORING_FEATURE_TESTS_ENABLED
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
