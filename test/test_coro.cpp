#ifdef IOUXX_CONFIG_USE_CXX_MODULE

import std;
import iouxx.ring;
import iouxx.ops;

#else // !IOUXX_CONFIG_USE_CXX_MODULE

#include <concepts>
#include <cstddef>
#include <variant>
#include <exception>
#include <coroutine>
#include <utility>
#include <memory>

#include <chrono>
#include <print>

#include "iouxx/iouringxx.hpp"
#include "iouxx/iouops/noop.hpp"
#include "iouxx/iouops/timeout.hpp"

#endif // IOUXX_CONFIG_USE_CXX_MODULE

#include <cassert>

// Copy from mylib::task and mylib::detached_task

namespace mylib {

    namespace details {

        struct symmetric_task_storage_base
        {
            static constexpr std::size_t empty = 0;
            static constexpr std::size_t value = 1;
            static constexpr std::size_t exception = 2;
            using empty_type = std::monostate;
            using exception_type = std::exception_ptr;
            template<typename DataType>
            using storage_type = std::variant<empty_type, DataType, exception_type>;

            void throw_if_exception(this auto&& self) {
                assert(self.storage.index() != empty && "Task result is uninitialized.");
                if (exception_type* ex = std::get_if<exception>(&self.storage)) {
                    std::rethrow_exception(*ex);
                }
            }

            void unhandled_exception(this auto&& self, exception_type e = std::current_exception()) noexcept {
                self.storage.template emplace<exception>(std::move(e));
            }
        };

    } // namespace mylib::details

    template<typename ReturnType>
    class symmetric_task_storage : public details::symmetric_task_storage_base
    {
    public:
        using return_type = ReturnType;
    private:
        using base = details::symmetric_task_storage_base;
        friend base;
        static constexpr bool return_reference = std::is_reference_v<return_type>;
    public:
        using data_type = std::conditional_t<return_reference, std::add_pointer_t<return_type>, return_type>;
        using storage_type = base::storage_type<data_type>;

        void return_value(return_type rt) noexcept requires (return_reference) {
            this->storage.template emplace<value>(std::addressof(rt));
        }

        template<typename U = return_type>
            requires (not return_reference) && std::convertible_to<U, return_type> && std::constructible_from<return_type, U>
        void return_value(U&& rt) noexcept(std::is_nothrow_constructible_v<return_type, U>) {
            this->storage.template emplace<value>(std::forward<U>(rt));
        }

        return_type do_resume() {
            throw_if_exception();
            if constexpr (return_reference) {
                return static_cast<return_type>(*std::get<value>(this->storage));
            } else {
                return std::move(std::get<value>(this->storage));
            }
        }

    private:
        storage_type storage;
    };

    template<typename Void>
        requires (std::is_void_v<Void>)
    class symmetric_task_storage<Void> : public details::symmetric_task_storage_base
    {
    private:
        using base = details::symmetric_task_storage_base;
        friend base;
    public:
        using return_type = void;
        using data_type = std::monostate;
        using storage_type = base::storage_type<data_type>;

        void return_void() noexcept { this->storage.template emplace<value>(); }

        void do_resume() { throw_if_exception(); }

    private:
        storage_type storage;
    };

} // namespace mylib

namespace mylib {

    class [[nodiscard]] detached_task
    {
    private:
        struct detached_task_promise
        {
            using handle_type = std::coroutine_handle<detached_task_promise>;
            detached_task get_return_object() noexcept { return detached_task(handle_type::from_promise(*this)); }
            void return_void() const noexcept {}
            std::suspend_always initial_suspend() const noexcept { return {}; }
            std::suspend_never final_suspend() const noexcept { return {}; } // coroutine destroyed on final suspend

            void unhandled_exception() noexcept(false) {
                // propagate exception to caller, executor, or whatever
                throw detached_task_unhandled_exit_exception(handle_type::from_promise(*this));
            }
        };

        detached_task() = default;

        using handle_type = detached_task_promise::handle_type;
        explicit detached_task(handle_type handle) noexcept : handle(handle) {}

    public:
        using promise_type = detached_task_promise;

        detached_task(const detached_task&) = delete;
        detached_task& operator=(const detached_task&) = delete;

        detached_task(detached_task&& other) noexcept
            : handle(std::exchange(other.handle, nullptr))
        {}

        detached_task& operator=(detached_task&& other) noexcept {
            detached_task().swap(other);
            return *this;
        }

        ~detached_task() { if (this->handle) { handle.destroy(); } }

        void swap(detached_task& other) noexcept {
            if (this == std::addressof(other)) { return; }
            std::ranges::swap(this->handle, other.handle);
        }

        // thrown when detached task exits with unhandled exception
        // nested exception is the exception thrown by the detached task
        // responsible for destroying the coroutine
        class detached_task_unhandled_exit_exception : public std::exception, public std::nested_exception
        {
        public:
            friend promise_type;

            // satisfy standard exception requirements

            detached_task_unhandled_exit_exception() = default;
            detached_task_unhandled_exit_exception(const detached_task_unhandled_exit_exception&) = default;
            detached_task_unhandled_exit_exception(detached_task_unhandled_exit_exception&&) = default;
            detached_task_unhandled_exit_exception& operator=(const detached_task_unhandled_exit_exception&) = default;
            detached_task_unhandled_exit_exception& operator=(detached_task_unhandled_exit_exception&&) = default;

            ~detached_task_unhandled_exit_exception() override = default;

            const char* what() const noexcept override { return message; }

        private:
            static constexpr auto message = "Detached task exits with unhandled exception.";

            static void handle_destroyer(void* p) noexcept { handle_type::from_address(p).destroy(); }

            explicit detached_task_unhandled_exit_exception(handle_type handle)
                // implicitly catch current exception via default init std::nested_exception
                : handle_holder(handle.address(), &handle_destroyer)
            {}

            std::shared_ptr<void> handle_holder = nullptr;
        };

        // can only be called once
        // once called, detached_task object is not responsible for destroying the coroutine
        void start() && {
            assert(this->handle && "Task already start!");
            std::exchange(this->handle, nullptr).resume();
        }

        [[nodiscard]]
        handle_type to_handle() && noexcept { return std::exchange(this->handle, nullptr); }

    private:
        handle_type handle = nullptr;
    };

} // namespace mylib

namespace mylib {

    namespace details {
    
        template<typename TaskType>
        class task_promise : public mylib::symmetric_task_storage<typename TaskType::return_type>
        {
        public:
            using task_type = TaskType;
            using handle_type = std::coroutine_handle<task_promise>;
            using return_type = typename task_type::return_type;

            // inherited from symmetric_task_storage:
            // unhandled_exception
            // return_value or return_void
            // do_resume

            struct [[nodiscard]] final_awaiter
            {
                bool await_ready() const noexcept { return false; }

                template<typename PromiseType>
                std::coroutine_handle<> await_suspend(std::coroutine_handle<PromiseType> current_coroutine) noexcept {
                    return static_cast<task_promise&>(current_coroutine.promise()).continuation;
                }

                void await_resume() const noexcept { std::unreachable(); }
            };

            task_type get_return_object() { return task_type(handle_type::from_promise(*this)); }
            std::suspend_always initial_suspend() noexcept { return {}; }
            final_awaiter final_suspend() noexcept { return {}; }

            void set_continuation(std::coroutine_handle<> c) noexcept { continuation = c; }

        protected:
            std::coroutine_handle<> continuation = std::noop_coroutine();
        };

        template<typename TaskType>
        class [[nodiscard]] task_awaiter
        {
        public:
            using task_type = TaskType;
            using handle_type = typename task_type::handle_type;
            using return_type = typename task_type::return_type;

            ~task_awaiter() { if (this->coroutine) { this->coroutine.destroy(); } }

            [[nodiscard]] bool await_ready() noexcept { return !this->coroutine; }

            template<typename PromiseType>
            handle_type await_suspend(std::coroutine_handle<PromiseType> current) noexcept {
                this->coroutine.promise().set_continuation(current);
                return this->coroutine;
            }

            return_type await_resume() { return this->coroutine.promise().do_resume(); }

        private:
            friend task_type;
            explicit task_awaiter(handle_type handle) noexcept : coroutine(handle) {}

            handle_type coroutine = nullptr;
        };

    } // namespace mylib::details

    template<typename ReturnType>
    class [[nodiscard]] task
    {
    public:
        using return_type = ReturnType;
        using promise_type = details::task_promise<task>;
        using handle_type = typename promise_type::handle_type;
        using task_awaiter = details::task_awaiter<task>;

        task(const task&) = delete;
        task& operator=(const task&) = delete;

        task(task&& other) noexcept : coroutine(std::exchange(other.coroutine, nullptr)) {}
        task& operator=(task&& other) noexcept {
            task().swap(other);
            return *this;
        }

        void swap(task& other) noexcept {
            if (this == std::addressof(other)) return;
            std::ranges::swap(this->coroutine, other.coroutine);
        }

        ~task() { if (this->coroutine) { this->coroutine.destroy(); } }

        task_awaiter operator co_await() && noexcept {
            return task_awaiter(std::exchange(this->coroutine, nullptr));
        }

        // Test only
        return_type sync_await() && {
            task_awaiter awaiter = std::move(*this).operator co_await();
            awaiter.await_suspend(std::noop_coroutine()).resume();
            return awaiter.await_resume();
        }

    private:
        friend promise_type;
        task() = default;
        explicit task(handle_type handle) noexcept : coroutine(handle) {}

        handle_type coroutine = nullptr;
    };

} // namespace mylib

#define TEST_EXPECT(...) do { \
    if (!(__VA_ARGS__)) { \
        std::println("Assertion failed: {}, {}:{}\n", #__VA_ARGS__, \
        __FILE__, __LINE__); \
        std::exit(-(__COUNTER__ + 1)); \
    } \
} while(0)

struct noizy {
    inline static int count = 0;
    noizy() { ++count; std::println("noizy constructed, count = {}", count); }
    ~noizy() { --count; std::println("noizy destructed"); }
};

mylib::task<int> wait_for(iouxx::ring& ring, std::chrono::nanoseconds duration) {
    noizy _;
    auto op = ring.make_await<iouxx::timeout_operation>();
    op.wait_for(duration);
    std::println("Awaiting timeout operation...");
    auto res = co_await op;
    TEST_EXPECT(res.has_value());
    std::println("Timeout operation completed.");
    co_return 42;
}

mylib::detached_task test(iouxx::ring& ring, int& result) {
    noizy _;
    auto op = ring.make_await<iouxx::noop_operation>();
    std::println("Awaiting noop operation...");
    auto res = co_await op;
    TEST_EXPECT(res.has_value());
    std::println("Noop operation completed.");
    using namespace std::chrono_literals;
    result = co_await wait_for(ring, 10ms);
    std::println("wait_for completed.");
}

int main() {
    iouxx::ring ring(8);
    int result = 0;
    std::println("Starting task...");
    test(ring, result).start();
    if (auto res = ring.wait_for_result()) {
        std::println("Invoking callback 1...");
        res->callback();
    } else {
        std::println("Error: {}", res.error().message());
        return -1;
    }
    if (auto res = ring.wait_for_result()) {
        std::println("Invoking callback 2...");
        res->callback();
    } else {
        std::println("Error: {}", res.error().message());
        return -1;
    }
    TEST_EXPECT(result == 42);
    TEST_EXPECT(noizy::count == 0);
}
