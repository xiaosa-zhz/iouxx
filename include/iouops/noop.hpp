#pragma once
#ifndef IOUXX_OPERATION_NOOP_H
#define IOUXX_OPERATION_NOOP_H 1

#include <functional>

#include "iouringxx.hpp"


namespace iouxx::inline iouops {

    // Noop operation with user-defined callback.
    template<typename Callback>
        requires (std::is_void_v<Callback>) || std::invocable<Callback, std::error_code>
    class noop_operation : public operation_base
    {
    public:
        template<typename F>
        explicit noop_operation(iouxx::io_uring_xx& ring, F&& f) :
            operation_base(iouxx::op_tag<noop_operation>, &ring.native()),
            callback(std::forward<F>(f))
        {}

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_nop(sqe);
        }

        void do_callback(int ev, std::int32_t) {
            std::invoke(callback, details::make_system_error_code(-ev));
        }

        Callback callback;
    };

    // Pure noop operation, does nothing on completion.
    // Mainly used for waking up the kernel thread or testing.
    template<>
    class noop_operation<void> : public operation_base
    {
    public:
        explicit noop_operation(iouxx::io_uring_xx& ring)
            : operation_base(iouxx::op_tag<noop_operation>, &ring.native())
        {}

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_nop(sqe);
        }

        void do_callback(int, std::int32_t) noexcept {}
    };

    template<typename F>
    noop_operation(iouxx::io_uring_xx&, F) -> noop_operation<std::decay_t<F>>;

    noop_operation(iouxx::io_uring_xx&) -> noop_operation<void>;

} // namespace iouxx::iouops

#endif // IOUXX_OPERATION_NOOP_H
