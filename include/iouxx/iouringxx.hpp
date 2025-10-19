#pragma once
#ifndef IOUXX_LIBURING_CXX_WRAPER_H
#define IOUXX_LIBURING_CXX_WRAPER_H 1

/*
    * iouxx is a C++ wrapper for the liburing library.
*/

#ifndef IOUXX_USE_CXX_MODULE

#include <liburing.h>
#include <sys/mman.h>

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
#include <memory>
#include <coroutine>
#include <charconv>
#include <limits>
#include <format>
#include <functional>

#include "macro_config.hpp"
#include "cxxmodule_helper.hpp"
#include "util/utility.hpp"
#include "util/assertion.hpp"

#endif // IOUXX_USE_CXX_MODULE

IOUXX_EXPORT
namespace iouxx {

    // Forward declaration
    class ring;

    // Forward declaration
    class operation_result;

} // namespace iouxx

namespace iouxx::details {

    // Dummy callback type for type deduction only.
    // Never actually used in operation.
    struct dummy_callback {
        constexpr void operator()(auto&&) const noexcept;
    };

    // Forward declaration
    template<typename Operation>
    consteval bool test_operation_methods() noexcept;

    // Forward declaration
    template<typename Operation>
    consteval bool test_operation_members() noexcept;

    // Forward declaration
    inline std::uint32_t get_native_handle(const ring& r) noexcept;

    template<typename Promise>
    concept has_unhandled_stopped = requires (Promise& p) {
        { p.unhandled_stopped() } noexcept -> std::convertible_to<std::coroutine_handle<>>;
    };

    template<typename Promise>
    constexpr std::coroutine_handle<Promise> handle_cast(std::coroutine_handle<> h) noexcept {
        return std::coroutine_handle<Promise>::from_address(h.address());
    }

    using cancel_handler_type = std::coroutine_handle<>(*)(std::coroutine_handle<>) noexcept;

    template<has_unhandled_stopped Promise>
    inline std::coroutine_handle<> cancel_handler(std::coroutine_handle<> h) noexcept {
        return handle_cast<Promise>(h).promise().unhandled_stopped();
    }

} // namespace iouxx::details

IOUXX_EXPORT
namespace iouxx::inline iouops {

    // Forward declaration
    class operation_base;

    // Enable sync wait support for operations.
    // To use this, the operation type must have:
    //   using result_type = ...;
    //   using callback_type = syncwait_callback<result_type>;
    //   callback_type callback; // accessible from operation_base
    // Recommended to use ring::make_sync to create such operations.
    template<typename Result>
    class syncwait_callback
    {
        using expected_type = std::expected<Result, std::error_code>;
    public:
        void operator()(expected_type res) noexcept {
            result = std::move(res);
        }

    private:
        friend operation_base;
        expected_type result = std::unexpected(std::error_code());
    };

    // Enable coroutine support for operations.
    // To use this, the operation type must have:
    //   using result_type = ...;
    //   using callback_type = awaiter_callback<result_type>;
    //   callback_type callback; // accessible from operation_base
    // Recommended to use ring::make_await to create such operations.
    template<typename Result>
    class awaiter_callback
    {
        using result_type = Result;
        using expected_type = std::expected<result_type, std::error_code>;
    public:
        void operator()(expected_type res) IOUXX_CALLBACK_NOEXCEPT {
            if (cancel_handler && !res && res.error() == std::errc::operation_canceled) {
                cancel_handler(handle).resume(); // should not throw
            } else {
                *result = std::move(res);
                // if the outest coroutine of current coroutine stack is a
                // 'propagate to scheduler' coroutine, this may eventually throw
                handle.resume();
            }
        }

    private:
        friend operation_base;
        expected_type* result = nullptr;
        details::cancel_handler_type cancel_handler = nullptr;
        std::coroutine_handle<> handle = nullptr;
    };

    template<template<typename...> class Operation>
    using syncwait_operation_t =
        Operation<syncwait_callback<typename Operation<details::dummy_callback>::result_type>>;

    template<template<typename...> class Operation>
    using awaiter_operation_t =
        Operation<awaiter_callback<typename Operation<details::dummy_callback>::result_type>>;

    class operation_identifier
    {
    public:
        operation_identifier() = default;
        operation_identifier(const operation_identifier&) = default;
        operation_identifier& operator=(const operation_identifier&) = default;

        friend constexpr bool operator==(
            const operation_identifier&, const operation_identifier&) = default;
        friend constexpr auto operator<=>(
            const operation_identifier&, const operation_identifier&) = default;

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
        friend operation_result;
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
    inline constexpr operation_t<Operation> op_tag = {};

    template<typename Operation>
    struct operation_traits {};

    template<typename Operation>
    concept operation = std::derived_from<Operation, operation_base>
        && requires {
            typename Operation::callback_type;
            typename Operation::result_type;
            { Operation::opcode } -> std::convertible_to<std::uint8_t>;
            requires (details::test_operation_methods<Operation>());
        };

    template<typename Operation>
    concept syncwait_operation = operation<Operation>
        && utility::is_specialization_of_v<syncwait_callback, typename Operation::callback_type>
        && (details::test_operation_members<Operation>());
    
    template<typename Operation>
    concept awaiter_operation = operation<Operation>
        && utility::is_specialization_of_v<awaiter_callback, typename Operation::callback_type>
        && (details::test_operation_members<Operation>());

    // Base class for operations.
    // Derived class must implement:
    //   void build() & noexcept;
    //   void do_callback(int ev, std::uint32_t cqe_flags);
    // Warning:
    // User of operations should create and store operation objects
    // in their own context, make sure the operation object outlives
    // the whole execution of io_uring task.
    class alignas(std::uint64_t) operation_base
    {
    public:
        operation_base() = delete;
        operation_base(const operation_base&) = delete;
        operation_base& operator=(const operation_base&) = delete;
        operation_base(operation_base&&) = delete;
        operation_base& operator=(operation_base&&) = delete;

        template<operation Self>
        ::io_uring_sqe* to_sqe(this Self& self) noexcept {
            ::io_uring_sqe* sqe = ::io_uring_get_sqe(self.ring_ptr->native());
            if (!sqe) return nullptr;
            self.build(sqe); // Provided by derived class
            ::io_uring_sqe_set_data(sqe, static_cast<operation_base*>(&self));
            return sqe;
        }

        template<operation Self>
            requires (!syncwait_operation<Self>) && (!awaiter_operation<Self>) 
        std::error_code submit(this Self& self) noexcept {
            return self.do_submit();
        }

        template<syncwait_operation Self>
        auto submit_and_wait(this Self& self)
            noexcept -> typename Self::callback_type::expected_type {
            if (std::error_code res = self.do_submit()) {
                return std::unexpected(res);
            }
            // Submit success, wait
            if (auto cqe_result = self.ring_ptr->wait_for_result()) {
                cqe_result->callback();
                return std::move(self.callback.result);
            } else {
                return std::unexpected(cqe_result.error());
            }
        }

        template<awaiter_operation Self>
        class operation_awaiter
        {
            using operation_type = Self;
            using callback_type = operation_type::callback_type;
            using result_type = operation_type::result_type;
            using expected_type = std::expected<result_type, std::error_code>;
        public:
            operation_awaiter() = delete;
            operation_awaiter(const operation_awaiter&) = delete;
            operation_awaiter& operator=(const operation_awaiter&) = delete;

            constexpr bool await_ready() const noexcept { return false; }

            template<typename CallerPromise>
            bool await_suspend(std::coroutine_handle<CallerPromise> handle) noexcept {
                self.setup_awaiter_callback(handle, this->result);
                if (std::error_code res = self.do_submit()) {
                    // fail to submit, resume immediately
                    result = std::unexpected(res);
                    return false;
                } else {
                    return true; // suspend
                }
            }

            expected_type await_resume() noexcept { return std::move(result); }

        private:
            friend operation_base;
            explicit operation_awaiter(Self& self) noexcept : self(self) {}

            Self& self;
            expected_type result = std::unexpected(std::error_code());
        };

        template<awaiter_operation Self>
        operation_awaiter<Self> operator co_await(this Self& self) noexcept {
            return operation_awaiter<Self>(self);
        }

        void callback(int ev, std::int32_t cqe_flags) & IOUXX_CALLBACK_NOEXCEPT {
            do_callback_ptr(this, ev, cqe_flags);
        }

        operation_identifier identifier() & noexcept {
            return operation_identifier(this);
        }

    protected:
        // Note:
        // Override method will receive raw error code from kernel, because:
        // 1. The positive value may be meaningful result,
        //    and it is operation rather than user-defined callback
        //    that knows what result means.
        // 2. Some error codes are not real error depends on context,
        //    e.g., -ETIME for pure timeout operation.
        //    The operation itself should decide how to handle it.
        using callback_wrapper_type =
            void (*)(operation_base*, int, std::int32_t) IOUXX_CALLBACK_NOEXCEPT;

        template<operation Derived>
        static void callback_wrapper(operation_base* base, int ev, std::int32_t cqe_flags)
            IOUXX_CALLBACK_NOEXCEPT_IF(utility::eligible_nothrow_callback<
                typename Derived::callback_type, typename Derived::result_type>) {
            // Provided by derived class
            static_cast<Derived*>(base)->do_callback(ev, cqe_flags);
        }

        // Type erasure here
        template<operation Derived>
        explicit operation_base(operation_t<Derived>, ring& ring) noexcept
            : do_callback_ptr(&callback_wrapper<Derived>), ring_ptr(&ring)
        {}

        // Enable feature test by define IOUXX_CONFIG_ENABLE_FEATURE_TESTS.
        // Always returns success if feature test is disabled.
        template<operation Self>
        std::error_code feature_test(this Self& self) noexcept {
#if IOUXX_IORING_FEATURE_TESTS_ENABLED == 1
            if (!::io_uring_opcode_supported(self.ring_ptr->ring_probe(),
                Self::opcode)) {
                return std::make_error_code(std::errc::function_not_supported);
            }
#endif // IOUXX_IORING_FEATURE_TESTS_ENABLED
            return std::error_code();
        }

        template<operation Self>
        std::error_code do_submit(this Self& self) noexcept {
            if (std::error_code test = self.feature_test()) {
                return test;
            }
            // Feature test passed
            return self.ring_ptr->submit(self.to_sqe());
        }

        template<awaiter_operation Self, typename CallerPromise, typename Result>
        void setup_awaiter_callback(this Self& self,
            std::coroutine_handle<CallerPromise> handle,
            std::expected<Result, std::error_code>& result) noexcept {
            auto& cb = self.callback;
            cb.handle = handle;
            cb.result = &result;
            if constexpr (details::has_unhandled_stopped<CallerPromise>) {
                cb.cancel_handler = &details::cancel_handler<CallerPromise>;
            }
        }

        template<typename Operation>
        static constexpr bool test_operation_methods_v = requires (
            Operation op, ::io_uring_sqe* sqe, int ev, std::int32_t cqe_flags) {
            { op.build(sqe) } noexcept;
            op.do_callback(ev, cqe_flags);
        };

        template<typename Operation>
        static constexpr bool test_operation_members_v = requires {
            requires std::same_as<typename Operation::callback_type,
                decltype(std::declval<Operation&>().callback)>;
        };

        template<typename Operation>
        friend consteval bool details::test_operation_methods() noexcept;

        template<typename Operation>
        friend consteval bool details::test_operation_members() noexcept;

        callback_wrapper_type do_callback_ptr = nullptr;
        ring* ring_ptr = nullptr;
    };

    template<template<typename...> class Operation, typename Callback, typename... Args>
        requires operation<Operation<Callback, Args...>>
    struct operation_traits<Operation<Callback, Args...>> {
        using operation_type = Operation<Callback, Args...>;
        using callback_type = typename operation_type::callback_type;
        using result_type = typename operation_type::result_type;
        static constexpr std::uint8_t opcode = operation_type::opcode;
        template<typename... NewArgs>
        using rebind = Operation<NewArgs...>;
    };

    struct management_info {
        std::int32_t ev;
        std::uint32_t cqe_flags;
        iouxx::ring* ring;
    };

    // Operation for management purpose.
    // Callback will receive an pointer to ring object.
    template<std::invocable<management_info> Callback>
    class ring_management_operation : public operation_base
    {
    public:
        template<utility::not_tag F>
        ring_management_operation(iouxx::ring& ring, F&& f)
            noexcept(utility::nothrow_constructible_callback<F>) :
            operation_base(iouxx::op_tag<ring_management_operation>, ring),
            callback(std::forward<F>(f))
        {}

        template<typename F, typename... Args>
        ring_management_operation(iouxx::ring& ring, std::in_place_type_t<F>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<F, Args...>) :
            operation_base(iouxx::op_tag<ring_management_operation>, ring),
            callback(std::forward<Args>(args)...)
        {}

        using callback_type = Callback;
        using result_type = management_info;

        static constexpr std::uint8_t opcode = IORING_OP_NOP;

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_nop(sqe);
        }

        void do_callback(int ev, std::uint32_t cqe_flags) IOUXX_CALLBACK_NOEXCEPT_IF(
            std::is_nothrow_invocable_v<callback_type, result_type>) {
            std::invoke(callback, management_info{
                ev, cqe_flags, this->ring_ptr
            });
        }

        [[no_unique_address]] callback_type callback;
    };

    template<utility::not_tag F>
    ring_management_operation(iouxx::ring&, F) 
        -> ring_management_operation<std::decay_t<F>>;

    template<typename F, typename... Args>
    ring_management_operation(iouxx::ring&, std::in_place_type_t<F>, Args&&...)
        -> ring_management_operation<F>;

    struct fd_unregistration_info {
        management_info info;
        int fd;
    };

    template<std::invocable<fd_unregistration_info> Callback>
    class fd_unregister_operation : public operation_base
    {
    public:
        template<utility::not_tag F>
        fd_unregister_operation(iouxx::ring& ring, F&& f)
            noexcept(utility::nothrow_constructible_callback<F>) :
            operation_base(iouxx::op_tag<fd_unregister_operation>, ring),
            fd(fd), callback(std::forward<F>(f))
        {}

        template<typename F, typename... Args>
        fd_unregister_operation(iouxx::ring& ring, std::in_place_type_t<F>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<F, Args...>) :
            operation_base(iouxx::op_tag<fd_unregister_operation>, ring),
            fd(fd), callback(std::forward<Args>(args)...)
        {}

        using callback_type = Callback;
        using result_type = fd_unregistration_info;

        static constexpr std::uint8_t opcode = IORING_OP_NOP;

        fd_unregister_operation& file(int fd) & noexcept {
            this->fd = fd;
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            // This operation should not be submitted by user.
            // Only generated from unreigistration of registered fd.
            IOUXX_ASSERT(false);
            std::unreachable();
        }

        // Operation state (include callback object) will be destroyed
        // after callback is invoked.
        void do_callback(int ev, std::uint32_t cqe_flags) IOUXX_CALLBACK_NOEXCEPT_IF(
            std::is_nothrow_invocable_v<callback_type, result_type>) {
            std::unique_ptr<fd_unregister_operation> self_guard(this);
            std::invoke(callback, fd_unregistration_info{
                { ev, cqe_flags, this->ring_ptr }, fd
            });
        }

        int fd = -1;
        [[no_unique_address]] callback_type callback;
    };

    template<utility::byte_unit Byte>
    struct buffer_unregister_info {
        management_info info;
        std::span<Byte> buffer;
    };

    template<typename Callback>
        requires std::invocable<buffer_unregister_info<std::byte>>
        || std::invocable<buffer_unregister_info<unsigned char>>
    class buffer_unregister_operation : public operation_base
    {
    public:
        template<utility::not_tag F>
        buffer_unregister_operation(iouxx::ring& ring, F&& f)
            noexcept(utility::nothrow_constructible_callback<F>) :
            operation_base(iouxx::op_tag<buffer_unregister_operation>, ring),
            callback(std::forward<F>(f))
        {}

        template<typename F, typename... Args>
        buffer_unregister_operation(iouxx::ring& ring, std::in_place_type_t<F>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<F, Args...>) :
            operation_base(iouxx::op_tag<buffer_unregister_operation>, ring),
            callback(std::forward<Args>(args)...)
        {}

        using callback_type = Callback;
        using result_type = std::conditional_t<
            std::invocable<Callback, buffer_unregister_info<std::byte>>,
            buffer_unregister_info<std::byte>,
            buffer_unregister_info<unsigned char>
        >;

        static constexpr std::uint8_t opcode = IORING_OP_NOP;

        template<utility::buffer_like Buffer>
        buffer_unregister_operation& buffer(Buffer&& buf) & noexcept {
            auto buffer = utility::to_buffer(std::forward<Buffer>(buf));
            data = buffer.data();
            length = buffer.size_bytes();
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            // This operation should not be submitted by user.
            // Only generated from unreigistration of registered buffer.
            IOUXX_ASSERT(false);
            std::unreachable();
        }

        // Operation state (include callback object) will be destroyed
        // after callback is invoked.
        void do_callback(int ev, std::uint32_t cqe_flags) IOUXX_CALLBACK_NOEXCEPT_IF(
            std::is_nothrow_invocable_v<callback_type, result_type>) {
            std::unique_ptr<buffer_unregister_operation> self_guard(this);
            if constexpr (std::same_as<result_type, buffer_unregister_info<std::byte>>) {
                std::invoke(callback, buffer_unregister_info<std::byte>{
                    { ev, cqe_flags, this->ring_ptr },
                    std::span(static_cast<std::byte*>(data), length)
                });
            } else {
                std::invoke(callback, buffer_unregister_info<unsigned char>{
                    { ev, cqe_flags, this->ring_ptr },
                    std::span(static_cast<unsigned char*>(data), length)
                });
            }
        }

        void* data;
        std::size_t length;
        [[no_unique_address]] callback_type callback;
    };

} // namespace iouxx::iouops

IOUXX_EXPORT
namespace iouxx {

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

        std::uint32_t flags() const noexcept { return cqe_flags; }
        std::uint32_t reset_flags(std::uint32_t flags = 0) noexcept {
            return std::exchange(cqe_flags, flags);
        }

        // Test if result is from a operation.
        // Some operation results may not come from sqe submission.
        explicit constexpr operator bool() const noexcept {
            return cb;
        }

        void callback() const IOUXX_CALLBACK_NOEXCEPT {
            cb->callback(res, cqe_flags);
        }

        void operator()() const IOUXX_CALLBACK_NOEXCEPT {
            callback();
        }

        operation_identifier identifier() const noexcept {
            return operation_identifier(cb);
        }

    private:
        friend ring;
        operation_result(iouops::operation_base* cb, std::int32_t res, std::uint32_t cqe_flags) noexcept
            : cb(cb), res(res), cqe_flags(cqe_flags)
        {}

        iouops::operation_base* cb;
        std::int32_t res;
        std::uint32_t cqe_flags;
    };

    class ring_option
    {
    public:
        ring_option() = default;
        ring_option(ring_option&) = default;
        ring_option& operator=(ring_option&) = default;

        // Note:
        //  Use setup_sqpoll() instead if you want to enable SQPOLL.
        //  Use setup_cqsize() instead if you want to specify CQ size seperately.
        //  Use setup_attach() instead if you want to attach to an existing ring.
        enum class flag : std::uint32_t {
            iopoll = IORING_SETUP_IOPOLL,
            clamp = IORING_SETUP_CLAMP,
            r_disabled = IORING_SETUP_R_DISABLED,
            submit_all = IORING_SETUP_SUBMIT_ALL,
            coop_taskrun = IORING_SETUP_COOP_TASKRUN,
            taskrun_flag = IORING_SETUP_TASKRUN_FLAG,
            sqe128 = IORING_SETUP_SQE128,
            cqe32 = IORING_SETUP_CQE32,
            single_issuer = IORING_SETUP_SINGLE_ISSUER,
            defer_taskrun = IORING_SETUP_DEFER_TASKRUN,
            no_mmap = IORING_SETUP_NO_MMAP,
            registered_fd_only = IORING_SETUP_REGISTERED_FD_ONLY,
            no_sqarray = IORING_SETUP_NO_SQARRAY,
            hybrid_iopoll = IORING_SETUP_HYBRID_IOPOLL,

            // sqpoll = IORING_SETUP_SQPOLL,
            // sq_aff = IORING_SETUP_SQ_AFF,
            // cqsize = IORING_SETUP_CQSIZE,
            // attach_wq = IORING_SETUP_ATTACH_WQ,
        };

        ring_option& flags(flag f) noexcept {
            ring_flags |= std::to_underlying(f);
            return *this;
        }

        ring_option& setup_sqpoll(
            std::uint32_t thread_cpu = std::numeric_limits<std::uint32_t>::max(),
            std::chrono::milliseconds idle = std::chrono::milliseconds(100)) noexcept {
            ring_flags |= IORING_SETUP_SQPOLL;
            sq_thread_idle = idle.count();
            if (thread_cpu < std::numeric_limits<std::uint32_t>::max()) {
                ring_flags |= IORING_SETUP_SQ_AFF;
                sq_thread_cpu = thread_cpu;
            } else {
                ring_flags &= ~IORING_SETUP_SQ_AFF;
            }
            return *this;
        }

        ring_option& setup_cqsize(std::uint32_t cq_size) noexcept {
            ring_flags |= IORING_SETUP_CQSIZE;
            this->cq_entries = cq_size;
            return *this;
        }

        ring_option& setup_attach(const ring& wq) noexcept {
            ring_flags |= IORING_SETUP_ATTACH_WQ;
            this->wq_fd = details::get_native_handle(wq);
            return *this;
        }

    private:
        friend ring;
        ::io_uring_params to_params() const noexcept {
            ::io_uring_params params{};
            params.flags = ring_flags;
            params.wq_fd = wq_fd;
            params.cq_entries = cq_entries;
            params.sq_thread_cpu = sq_thread_cpu;
            params.sq_thread_idle = sq_thread_idle;
            return params;
        }

        friend constexpr flag operator|(flag lhs, flag rhs) noexcept {
            return static_cast<flag>(
                std::to_underlying(lhs) | std::to_underlying(rhs)
            );
        }

        friend constexpr flag& operator|=(flag& lhs, flag rhs) noexcept {
            lhs = lhs | rhs;
            return lhs;
        }

        friend constexpr flag operator&(flag lhs, flag rhs) noexcept {
            lhs = static_cast<flag>(
                std::to_underlying(lhs) & std::to_underlying(rhs)
            );
            return lhs;
        }

        friend constexpr flag& operator&=(flag& lhs, flag rhs) noexcept {
            lhs = lhs & rhs;
            return lhs;
        }

        friend constexpr flag operator~(flag f) noexcept {
            return static_cast<flag>(~std::to_underlying(f));
        }

        std::uint32_t ring_flags = 0;
        std::uint32_t wq_fd = 0;
        std::uint32_t cq_entries = 0;
        std::uint32_t sq_thread_cpu = 0;
        std::uint32_t sq_thread_idle = 0;
    };

    class ring
    {
    public:
        explicit ring() = default;

        explicit ring(std::size_t queue_depth, const ring_option& opt = ring_option()) {
            std::error_code ec = do_init(queue_depth, opt);
            if (ec) {
                throw std::system_error(ec, "Failed to initialize io_uring");
            }
        }

        ring(const ring&) = delete;
        ring& operator=(const ring&) = delete;
        ring(ring&& other) = delete;
        ring& operator=(ring&& other) = delete;

        void swap(ring& other) noexcept {
            std::ranges::swap(raw_ring, other.raw_ring);
        }

        ~ring() { exit(); }

        struct version_info {
            int major = std::numeric_limits<int>::max();
            int minor = std::numeric_limits<int>::max();

            // Get current liburing version.
            // Note: this is compile-time constant if used in consteval context,
            //  fetch version info from header file;
            //  Otherwise, fetch version info from loaded liburing.so at runtime.
            static constexpr version_info current() noexcept {
                if consteval {
                    return { IO_URING_VERSION_MAJOR, IO_URING_VERSION_MINOR };
                } else {
                    return {
                        ::io_uring_major_version(),
                        ::io_uring_minor_version(),
                    };
                }
            }

            consteval static version_info invalid() noexcept { return {}; }

            static constexpr version_info from_string(std::string_view version) noexcept {
                // Simple parser for "major.minor" format
                namespace stdv = std::views;
                int major = -1, minor = -1;
                auto parts = version | stdv::split('.');
                for (auto&& part : parts) {
                    std::string_view num(part);
                    if (major == -1) {
                        auto [ptr, ec] = std::from_chars(
                            num.data(), num.data() + num.size(), major);
                        if (ec != std::errc() || ptr != num.data() + num.size()) {
                            return invalid();
                        }
                        if (major < 0) return invalid();
                    } else if (minor == -1) {
                        auto [ptr, ec] = std::from_chars(
                            num.data(), num.data() + num.size(), minor);
                        if (ec != std::errc() || ptr != num.data() + num.size()) {
                            return invalid();
                        }
                        if (minor < 0) return invalid();
                    } else {
                        return invalid();
                    }
                }
                if (major == -1 || minor == -1) return invalid();
                return { major, minor };
            }

            friend constexpr bool operator==(
                const version_info&, const version_info&) = default;
            friend constexpr auto operator<=>(
                const version_info&, const version_info&) = default;
            
            friend constexpr bool operator==(
                const version_info& ver, std::string_view str) noexcept {
                return ver == from_string(str);
            }
            friend constexpr auto operator<=>(
                const version_info& ver, std::string_view str) noexcept {
                // Note: if str is invalid version, str > ver always holds,
                //  which means invalid version is always NOT supported.
                return ver <=> from_string(str);
            }
        };

        // Get current liburing version, see version_info::current()
        static constexpr version_info version() noexcept {
            return version_info::current();
        }

        // Input minimum required version.
        // Returns TRUE if requirement > current version (i.e. NOT supported).
        static constexpr bool check_version(version_info requirement) noexcept {
            auto&& [major, minor] = requirement;
            if consteval {
                return IO_URING_CHECK_VERSION(major, minor);
            } else {
                return ::io_uring_check_version(major, minor);
            }
        }

        // See above, input string in "major.minor" format.
        // Returns TRUE if NOT supported.
        static constexpr bool check_version(std::string_view requirement) noexcept {
            return check_version(version_info::from_string(requirement));
        }

        bool valid() const noexcept { return raw_ring.ring_fd >= 0; }

        std::error_code reinit(std::size_t queue_depth,
            const ring_option& opt = ring_option()) noexcept {
            exit();
            return do_init(queue_depth, opt);
        }

        // Note: user still needs to consume all CQEs of canceled operations.
        std::error_code stop(std::chrono::nanoseconds timeout = {}) noexcept {
            if (!valid()) {
                return std::error_code();
            }
            ::io_uring_sync_cancel_reg reg = {};
            if (timeout.count() != 0) {
                reg.timeout = utility::to_kernel_timespec(timeout);
            } else {
                reg.timeout = { -1, -1 };
            }
            reg.flags |= IORING_ASYNC_CANCEL_ANY | IORING_ASYNC_CANCEL_ALL;
            int ev = ::io_uring_register_sync_cancel(&raw_ring, &reg);
            return utility::make_system_error_code(-ev);
        }

        void exit() noexcept {
            if (valid()) {
                [[maybe_unused]] std::error_code res = stop();
                IOUXX_ASSERT(res != utility::fail_invalid_argument().error());
                probe.reset();
                ::io_uring_queue_exit(&raw_ring);
                raw_ring = invalid_ring();
            }
        }

        // Explicitly specify operation template to create.
        template<template<typename...> class Operation, utility::not_tag Callback>
        Operation<std::decay_t<Callback>> make(Callback&& callback) &
            noexcept(utility::nothrow_constructible_callback<Callback>) {
            IOUXX_ASSERT(valid());
            using operation_type = Operation<std::decay_t<Callback>>;
            return operation_type(*this, std::forward<Callback>(callback));
        }

        // No callback variant.
        // Explicitly specify operation template to create.
        template<template<typename...> class Operation>
        Operation<void> make() & noexcept {
            IOUXX_ASSERT(valid());
            using operation_type = Operation<void>;
            return operation_type(*this);
        }

        // Construct callback in place.
        // Explicitly specify operation template to create.
        template<template<typename...> class Operation, typename F, typename... Args>
        Operation<F> make(std::in_place_type_t<F> tag, Args&&... args) &
            noexcept(std::is_nothrow_constructible_v<F, Args...>) {
            IOUXX_ASSERT(valid());
            using operation_type = Operation<F>;
            return operation_type(*this, tag, std::forward<Args>(args)...);
        }

        // Construct callback in place.
        // Explicitly specify operation<callback> to create.
        template<typename Operation, typename... Args>
        Operation make_in_place(Args&&... args) &
            noexcept(std::is_nothrow_constructible_v<
                typename Operation::callback_type, Args...>) {
            IOUXX_ASSERT(valid());
            using operation_type = Operation;
            using callback_type = operation_type::callback_type;
            return operation_type(*this, std::in_place_type<callback_type>,
                std::forward<Args>(args)...);
        }

        // Create a sync-waitable operation.
        // Explicitly specify operation template to create.
        template<template<typename...> class Operation>
        syncwait_operation_t<Operation> make_sync() & noexcept {
            IOUXX_ASSERT(valid());
            using operation_type = syncwait_operation_t<Operation>;
            using callback_type = operation_type::callback_type;
            return operation_type(*this, std::in_place_type<callback_type>);
        }

        // Create a coroutine-awaitable operation.
        // Explicitly specify operation template to create.
        template<template<typename...> class Operation>
        awaiter_operation_t<Operation> make_await() & noexcept {
            IOUXX_ASSERT(valid());
            using operation_type = awaiter_operation_t<Operation>;
            using callback_type = operation_type::callback_type;
            return operation_type(*this, std::in_place_type<callback_type>);
        }

        std::error_code submit(::io_uring_sqe* sqe) noexcept {
            IOUXX_ASSERT(valid());
            if (!sqe) {
                return std::make_error_code(std::errc::resource_unavailable_try_again);
            }
            int ev = ::io_uring_submit(&raw_ring);
            if (ev < 0) {
                return utility::make_system_error_code(-ev);
            }
            return std::error_code();
        }

        std::expected<operation_result, std::error_code> fetch_result() noexcept {
            IOUXX_ASSERT(valid());
            ::io_uring_cqe* cqe = nullptr;
            int ev = ::io_uring_peek_cqe(&raw_ring, &cqe);
            if (ev < 0) {
                return utility::fail(-ev);
            }
            operation_result result = to_result(cqe);
            ::io_uring_cqe_seen(&raw_ring, cqe);
            return result;
        }

        auto wait_for_result(std::chrono::nanoseconds timeout = {})
            noexcept -> std::expected<operation_result, std::error_code> {
            IOUXX_ASSERT(valid());
            ::io_uring_cqe* cqe = nullptr;
            int ev = 0;
            if (timeout.count() != 0) {
                auto ts = utility::to_kernel_timespec(timeout);
                ev = ::io_uring_wait_cqe_timeout(&raw_ring, &cqe, &ts);
            } else {
                ev = ::io_uring_wait_cqe(&raw_ring, &cqe);
            }
            if (ev < 0) {
                return utility::fail(-ev);
            }
            operation_result result = to_result(cqe);
            ::io_uring_cqe_seen(&raw_ring, cqe);
            return result;
        }

        // Maximum size of resource tag used by fd and buffer registration.
        using resource_tag_type = ::__u64;
        static constexpr std::size_t max_resource_tag_size = std::numeric_limits<std::uint32_t>::max();

        template<std::invocable<resource_tag_type> Callback>
        void register_buffer_unregistration_callback(Callback&& callback) {
            IOUXX_ASSERT(valid());
            buffer_unregister_callback = make_callback_handle(std::forward<Callback>(callback));
        }

        template<std::invocable<resource_tag_type> Callback>
        void register_direct_descriper_unregistration_callback(Callback&& callback) {
            IOUXX_ASSERT(valid());
            fd_unregister_callback = make_callback_handle(std::forward<Callback>(callback));
        }

        std::error_code register_buffer_table(std::size_t size) noexcept {
            IOUXX_ASSERT(valid());
            int ev = ::io_uring_register_buffers_sparse(&raw_ring, size);
            return utility::make_system_error_code(-ev);
        }

        template<utility::buffer_range Buffers>
        std::error_code update_buffer_table(std::size_t offset, Buffers&& buffers,
            const std::span<const resource_tag_type> tags) noexcept {
            IOUXX_ASSERT(valid());
            try {
                std::vector<::iovec> iovecs = std::forward<Buffers>(buffers)
                    | std::views::transform([]<typename Buffer>(Buffer&& buffer) {
                        using byte_type = std::ranges::range_value_t<std::remove_cvref_t<Buffer>>;
                        return utility::to_iovec(std::span<byte_type>(std::forward<Buffer>(buffer)));
                    })
                    | std::ranges::to<std::vector<::iovec>>();
                int ev = 0;
                if (tags.empty()) {
                    ev = ::io_uring_register_buffers_update_tag(&raw_ring, offset,
                        iovecs.data(), nullptr, iovecs.size());
                } else {
                    IOUXX_ASSERT(tags.size() == iovecs.size());
                    ev = ::io_uring_register_buffers_update_tag(&raw_ring, offset,
                        iovecs.data(), tags.data(), iovecs.size());
                }
                return utility::make_system_error_code(-ev);
            } catch (...) {
                return std::make_error_code(std::errc::not_enough_memory);
            }
        }

        template<utility::buffer_range Buffers>
        std::error_code register_buffers(Buffers&& buffers,
            const std::span<const resource_tag_type> tags) noexcept {
            IOUXX_ASSERT(valid());
            try {
                std::vector<::iovec> iovecs = std::forward<Buffers>(buffers)
                    | std::views::transform([]<typename Buffer>(Buffer&& buffer) {
                        using byte_type = std::ranges::range_value_t<std::remove_cvref_t<Buffer>>;
                        return utility::to_iovec(std::span<byte_type>(std::forward<Buffer>(buffer)));
                    })
                    | std::ranges::to<std::vector<::iovec>>();
                int ev = 0;
                if (tags.empty()) {
                    ev = ::io_uring_register_buffers(&raw_ring,
                        iovecs.data(), iovecs.size());
                } else {
                    IOUXX_ASSERT(tags.size() == iovecs.size());
                    ev = ::io_uring_register_buffers_tags(&raw_ring,
                        iovecs.data(), tags.data(), iovecs.size());
                }
                return utility::make_system_error_code(-ev);
            } catch (...) {
                return std::make_error_code(std::errc::not_enough_memory);
            }
        }

        std::error_code register_direct_descriptor_table(std::size_t size) noexcept {
            IOUXX_ASSERT(valid());
            int ev = ::io_uring_register_files_sparse(&raw_ring, size);
            return utility::make_system_error_code(-ev);
        }

        std::error_code update_direct_descriptor_table(std::size_t offset, const std::span<const int> fds,
            const std::span<const resource_tag_type> tags) noexcept {
            IOUXX_ASSERT(valid());
            int ev = 0;
            if (tags.empty()) {
                ev = ::io_uring_register_files_update(&raw_ring, offset,
                    fds.data(), fds.size());
            } else {
                IOUXX_ASSERT(tags.size() == fds.size());
                ev = ::io_uring_register_files_update_tag(&raw_ring, offset,
                    fds.data(), tags.data(), fds.size());
            }
            return utility::make_system_error_code(-ev);
        }

        std::error_code register_direct_descriptors(const std::span<const int> fds,
            const std::span<const resource_tag_type> tags) noexcept {
            IOUXX_ASSERT(valid());
            int ev = 0;
            if (tags.empty()) {
                ev = ::io_uring_register_files(&raw_ring,
                    fds.data(), fds.size());
            } else {
                IOUXX_ASSERT(tags.size() == fds.size());
                ev = ::io_uring_register_files_tags(&raw_ring,
                    fds.data(), tags.data(), fds.size());
            }
            return utility::make_system_error_code(-ev);
        }

        ::io_uring* native() & noexcept {
            IOUXX_ASSERT(valid());
            return &raw_ring;
        }

        int native_handle() const noexcept {
            IOUXX_ASSERT(valid());
            return raw_ring.ring_fd;
        }

        ::io_uring_probe* ring_probe() const noexcept {
            return probe.get();
        }

        enum class feature : std::uint32_t {
            single_mmap = IORING_FEAT_SINGLE_MMAP,
            nodrop = IORING_FEAT_NODROP,
            submit_stable = IORING_FEAT_SUBMIT_STABLE,
            rw_cur_pos = IORING_FEAT_RW_CUR_POS,
            cur_personality = IORING_FEAT_CUR_PERSONALITY,
            fast_poll = IORING_FEAT_FAST_POLL,
            poll_32bits = IORING_FEAT_POLL_32BITS,
            sqpoll_nofixed = IORING_FEAT_SQPOLL_NONFIXED,
            ext_arg = IORING_FEAT_EXT_ARG,
            native_workers = IORING_FEAT_NATIVE_WORKERS,
            rsrc_tags = IORING_FEAT_RSRC_TAGS,
            cqe_skip = IORING_FEAT_CQE_SKIP,
            linked_file = IORING_FEAT_LINKED_FILE,
            reg_reg_ring = IORING_FEAT_REG_REG_RING,
            recvsend_bundle = IORING_FEAT_RECVSEND_BUNDLE,
            min_timeout = IORING_FEAT_MIN_TIMEOUT,
        };

        feature supported_features() const noexcept {
            IOUXX_ASSERT(valid());
            return static_cast<feature>(raw_ring.features);
        }

        bool test_feature(feature f) const noexcept {
            IOUXX_ASSERT(valid());
            return (supported_features() & f) == f;
        }

        ring_option::flag setup_flags() const noexcept {
            IOUXX_ASSERT(valid());
            return static_cast<ring_option::flag>(raw_ring.flags);
        }

        bool test_flag(ring_option::flag f) const noexcept {
            IOUXX_ASSERT(valid());
            return (setup_flags() & f) == f;
        }

        struct napi_config {
            std::chrono::microseconds busy_poll_to = std::chrono::microseconds(0);
            bool prefer_busy_poll = false;
        };

        // Only available if ring is set up with IOPOLL.
        auto register_napi(std::chrono::microseconds timeout, bool prefer_busy_poll = true) & noexcept
            -> std::expected<napi_config, std::error_code> {
            IOUXX_ASSERT(valid());
            IOUXX_ASSERT(test_flag(ring_option::flag::iopoll));
            ::io_uring_napi napi = {};
            napi.prefer_busy_poll = prefer_busy_poll ? 1 : 0;
            napi.busy_poll_to = static_cast<std::uint32_t>(timeout.count());
            int ev = ::io_uring_register_napi(&raw_ring, &napi);
            if (ev == 0) {
                return napi_config{
                    std::chrono::microseconds(napi.busy_poll_to),
                    napi.prefer_busy_poll != 0,
                };
            } else {
                return utility::fail(-ev);
            }
        }

        auto unregister_napi() & noexcept -> std::expected<napi_config, std::error_code> {
            IOUXX_ASSERT(valid());
            IOUXX_ASSERT(test_flag(ring_option::flag::iopoll));
            ::io_uring_napi napi = {};
            int ev = ::io_uring_unregister_napi(&raw_ring, &napi);
            if (ev == 0) {
                return napi_config{
                    std::chrono::microseconds(napi.busy_poll_to),
                    napi.prefer_busy_poll != 0,
                };
            } else {
                return utility::fail(-ev);
            }
        }

    private:
        static ::io_uring invalid_ring() noexcept {
            return { .ring_fd = -1, .enter_ring_fd = -1 };
        }

        std::error_code do_init(std::size_t queue_depth, const ring_option& opt) noexcept {
            IOUXX_ASSERT(!valid());
            ::io_uring_params params = opt.to_params();
            int ev = ::io_uring_queue_init_params(
                queue_depth, &raw_ring, &params);
            if (ev < 0) {
                raw_ring = invalid_ring();
                return utility::make_system_error_code(-ev);
            }
            if (::io_uring_probe* raw = ::io_uring_get_probe_ring(&raw_ring)) {
                probe.reset(raw);
            } else {
                exit();
                return std::make_error_code(std::errc::not_enough_memory);
            }
            return std::error_code();
        }

        struct probe_deleter {
            void operator()(::io_uring_probe* probe) const noexcept {
                ::io_uring_free_probe(probe);
            }
        };

        using probe_handle = std::unique_ptr<::io_uring_probe, probe_deleter>;

        struct noop_callback {
            static constexpr void operator()(iouops::management_info) noexcept {}
        };

        using noop_callback_operation = iouops::ring_management_operation<noop_callback>;

        using deleter_type = void (*)(iouops::operation_base*) noexcept;

        template<typename Operation>
        static void do_delete(iouops::operation_base* op) noexcept {
            std::default_delete<Operation>{}(static_cast<Operation*>(op));
        }

        using unregistration_callback_handle = std::unique_ptr<operation_base, deleter_type>;

        static unregistration_callback_handle empty_callback_handle() noexcept {
            return unregistration_callback_handle(nullptr, nullptr);
        }

        template<typename Callback>
        unregistration_callback_handle make_callback_handle(Callback&& callback) {
            using operation_type = iouops::ring_management_operation<std::decay_t<Callback>>;
            auto op = std::make_unique<operation_type>(*this, std::forward<Callback>(callback));
            return unregistration_callback_handle(op.release(), &do_delete<operation_type>);
        }

        static constexpr std::uint64_t pointer_tag_mask = 0b111;
        static constexpr std::uint64_t tag_normal_callback = 0;
        static constexpr std::uint64_t tag_fd_unregister = 1;
        static constexpr std::uint64_t tag_buffer_unregister = 2;

        operation_result to_result(::io_uring_cqe* cqe) noexcept {
            IOUXX_ASSERT(cqe != nullptr);
            const std::uint64_t user_data = ::io_uring_cqe_get_data64(cqe);
            const std::uint64_t tag = user_data & pointer_tag_mask;
            iouops::operation_base* cb = nullptr;
            if (tag == tag_normal_callback) [[likely]] {
                cb = static_cast<iouops::operation_base*>(::io_uring_cqe_get_data(cqe));
                return operation_result(cb, cqe->res, cqe->flags);
            } else {
                // For fd and buffer unregister, higher bits are resource tag that set during registration.
                const std::uint32_t resource_tag = static_cast<std::uint32_t>(user_data >> 3);
                if (tag == tag_fd_unregister) {
                    cb = fd_unregister_callback.get();
                } else if (tag == tag_buffer_unregister) {
                    cb = buffer_unregister_callback.get();
                } else {
                    std::unreachable();
                }
                return operation_result(cb, 0, resource_tag);
            }
        }

        friend constexpr feature operator|(feature lhs, feature rhs) noexcept {
            return static_cast<feature>(
                std::to_underlying(lhs) | std::to_underlying(rhs)
            );
        }

        friend constexpr feature& operator|=(feature& lhs, feature rhs) noexcept {
            lhs = lhs | rhs;
            return lhs;
        }

        friend constexpr feature operator&(feature lhs, feature rhs) noexcept {
            return static_cast<feature>(
                std::to_underlying(lhs) & std::to_underlying(rhs)
            );
        }

        friend constexpr feature& operator&=(feature& lhs, feature rhs) noexcept {
            lhs = lhs & rhs;
            return lhs;
        }

        friend constexpr feature operator~(feature f) noexcept {
            return static_cast<feature>(~std::to_underlying(f));
        }

        ::io_uring raw_ring = invalid_ring(); // using ring_fd to detect if valid
        probe_handle probe = nullptr;
        unregistration_callback_handle fd_unregister_callback = empty_callback_handle();
        unregistration_callback_handle buffer_unregister_callback = empty_callback_handle();
    };

} // namespace iouxx

namespace iouxx::details {

    template<typename Operation>
    consteval bool test_operation_methods() noexcept {
        return operation_base::test_operation_methods_v<Operation>;
    }

    template<typename Operation>
    consteval bool test_operation_members() noexcept {
        return operation_base::test_operation_members_v<Operation>;
    }

    inline std::uint32_t get_native_handle(const ring& r) noexcept {
        return static_cast<std::uint32_t>(r.native_handle());
    }

} // namespace iouxx::details

IOUXX_EXPORT
namespace std {

    // Hash support for iouxx::iouops::operation_identifier
    template<>
    struct hash<iouxx::iouops::operation_identifier>
    {
        std::size_t operator()(const iouxx::iouops::operation_identifier& id) const noexcept {
            return std::hash<void*>{}(id.user_data());
        }
    };

    // Formatter for iouxx::iouops::operation_identifier
    template<typename CharT>
    struct formatter<iouxx::iouops::operation_identifier, CharT> : formatter<void*, CharT>
    {
        using base = formatter<void*, char>;
        using base::parse;

        template<class FormatContext>
        constexpr auto format(const iouxx::iouops::operation_identifier& id, FormatContext& ctx) const {
            return base::format(id.user_data(), ctx);
        }
    };

    // Formatter for iouxx::ring::version_info
    template<>
    struct formatter<iouxx::ring::version_info, char>
    {
        char seperator = '.';

        // Accepts optional seperator character, default is '.'
        constexpr auto parse(format_parse_context& ctx) -> format_parse_context::iterator {
            auto it = ctx.begin();
            auto end = ctx.end();
            if (it != end && *it != '}') {
                seperator = *it++;
            }
            if (it != end && *it != '}') {
                throw std::format_error(
                    "Invalid format for iouxx::ring::version_info, "
                    "expect up to one character as seperator"
                );
            }
            return it;
        }

        template<class FormatContext>
        constexpr auto format(const iouxx::ring::version_info& ver, FormatContext& ctx) const {
            auto out = ctx.out();
            out = std::format_to(out, "{}", ver.major);
            out = std::format_to(out, "{}", seperator);
            out = std::format_to(out, "{}", ver.minor);
            return out;
        }
    };

} // namespace std

#endif // IOUXX_LIBURING_CXX_WRAPER_H
