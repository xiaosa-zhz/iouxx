#pragma once
#ifndef IOUXX_OPERATION_CANCEL_H
#define IOUXX_OPERATION_CANCEL_H 1

#include <cstdint>
#include <functional>
#include <expected>

#include "iouringxx.hpp"
#include "util/utility.hpp"
#include "macro_config.hpp"

namespace iouxx::inline iouops {

	// Cancel operation with user-defined callback by provided identifier.
    // On success, callback receive a number indicating how many operations were cancelled.
	template<typename Callback>
        requires (std::is_void_v<Callback>)
        || std::invocable<Callback, std::expected<std::size_t, std::error_code>>
	class cancel_operation : public operation_base
	{
	public:
		template<typename F>
		cancel_operation(iouxx::io_uring_xx& ring, F&& f) noexcept :
			operation_base(iouxx::op_tag<cancel_operation>, ring),
            callback(std::forward<F>(f))
        {}

        static constexpr std::uint8_t IORING_OPCODE = IORING_OP_ASYNC_CANCEL;

		cancel_operation& target(operation_identifier identifier) & noexcept {
			id = identifier;
			return *this;
		}

        cancel_operation& cancel_one() & noexcept {
            flags &= ~IORING_ASYNC_CANCEL_ALL;
			return *this;
		}

		cancel_operation& cancel_all() & noexcept {
            flags |= IORING_ASYNC_CANCEL_ALL;
			return *this;
		}

	private:
		friend operation_base;
		void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_cancel(sqe, id.user_data(), flags);
		}

	    void do_callback(int ev, std::int32_t) IOUXX_CALLBACK_NOEXCEPT {
            std::size_t cancelled_count = 0;
            if (ev >= 0) {
                if (flags & IORING_ASYNC_CANCEL_ALL) {
                    cancelled_count = static_cast<std::size_t>(ev);
                } else {
                    cancelled_count = 1;
                }
                ev = 0;
            }
            if (ev == 0) {
                std::invoke(callback, cancelled_count);
            } else {
                std::invoke(callback, std::unexpected(
                    utility::make_system_error_code(-ev)
                ));
            }
		}

		operation_identifier id = operation_identifier();
		unsigned flags = IORING_ASYNC_CANCEL_USERDATA;
		[[no_unique_address]] Callback callback;
	};

    // Pure cancel operation, does nothing on completion.
    template<>
    class cancel_operation<void> : public operation_base
    {
    public:
        explicit cancel_operation(iouxx::io_uring_xx& ring) noexcept :
            operation_base(iouxx::op_tag<cancel_operation>, ring)
        {}

        static constexpr std::uint8_t IORING_OPCODE = IORING_OP_ASYNC_CANCEL;

        cancel_operation& target(operation_identifier identifier) & noexcept {
            id = identifier;
            return *this;
        }

        cancel_operation& cancel_one() & noexcept {
            flags &= ~IORING_ASYNC_CANCEL_ALL;
            return *this;
        }

        cancel_operation& cancel_all() & noexcept {
            flags |= IORING_ASYNC_CANCEL_ALL;
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_cancel(sqe, id.user_data(), flags);
        }

        void do_callback(int, std::int32_t) noexcept {}

        operation_identifier id = operation_identifier();
        unsigned flags = 0;
    };

	template<typename F>
	cancel_operation(iouxx::io_uring_xx&, F) -> cancel_operation<std::decay_t<F>>;

    cancel_operation(iouxx::io_uring_xx&) -> cancel_operation<void>;

    // Cancel operation with user-defined callback by provided file descriptor.
    // Callback may receive a second parameter indicating
    // how many operations were cancelled.
    template<typename Callback>
        requires (std::is_void_v<Callback>)
        || std::invocable<Callback, std::error_code>
        || std::invocable<Callback, std::error_code, std::size_t>
    class cancel_fd_operation : public operation_base
    {
    public:
        template<typename F>
        cancel_fd_operation(iouxx::io_uring_xx& ring, F&& f) noexcept :
            operation_base(iouxx::op_tag<cancel_fd_operation>, ring),
            callback(std::forward<F>(f))
        {}

        static constexpr std::uint8_t IORING_OPCODE = IORING_OP_ASYNC_CANCEL;

        cancel_fd_operation& target(int file_descriptor) & noexcept {
            fd = file_descriptor;
            flags &= ~IORING_ASYNC_CANCEL_FD_FIXED;
            return *this;
        }

        // Provide file descriptor that is an 'direct descripor' of io_uring
        cancel_fd_operation& target_direct(int file_descriptor) & noexcept {
            fd = file_descriptor;
            flags |= IORING_ASYNC_CANCEL_FD_FIXED;
            return *this;
        }

        cancel_fd_operation& cancel_one() & noexcept {
            flags &= ~IORING_ASYNC_CANCEL_ALL;
            return *this;
        }

        cancel_fd_operation& cancel_all() & noexcept {
            flags |= IORING_ASYNC_CANCEL_ALL;
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_cancel_fd(sqe, fd, flags);
        }

        void do_callback(int ev, std::int32_t) IOUXX_CALLBACK_NOEXCEPT {
            if constexpr (std::is_invocable_v<Callback, std::error_code, std::size_t>) {
                std::size_t cancelled_count = 0;
                if (ev >= 0) {
                    if (flags & IORING_ASYNC_CANCEL_ALL) {
                        cancelled_count = static_cast<std::size_t>(ev);
                    } else {
                        cancelled_count = 1;
                    }
                    ev = 0;
                }
                std::invoke(callback, utility::make_system_error_code(-ev), cancelled_count);
            } else {
                std::invoke(callback, utility::make_system_error_code(-ev));
            }
        }

        int fd = -1;
        unsigned flags = IORING_ASYNC_CANCEL_FD;
        [[no_unique_address]] Callback callback;
    };

    // Pure cancel fd operation, does nothing on completion.
    template<>
    class cancel_fd_operation<void> : public operation_base
    {
    public:
        explicit cancel_fd_operation(iouxx::io_uring_xx& ring) noexcept :
            operation_base(iouxx::op_tag<cancel_fd_operation>, ring)
        {}

        static constexpr std::uint8_t IORING_OPCODE = IORING_OP_ASYNC_CANCEL;

        cancel_fd_operation& target(int file_descriptor) & noexcept {
            fd = file_descriptor;
            flags &= ~IORING_ASYNC_CANCEL_FD_FIXED;
            return *this;
        }

        // Provide file descriptor that is an 'direct descripor' of io_uring
        cancel_fd_operation& target_direct(int file_descriptor) & noexcept {
            fd = file_descriptor;
            flags |= IORING_ASYNC_CANCEL_FD_FIXED;
            return *this;
        }

        cancel_fd_operation& cancel_one() & noexcept {
            flags &= ~IORING_ASYNC_CANCEL_ALL;
            return *this;
        }

        cancel_fd_operation& cancel_all() & noexcept {
            flags |= IORING_ASYNC_CANCEL_ALL;
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_cancel_fd(sqe, fd, flags);
        }

        void do_callback(int, std::int32_t) noexcept {}

        int fd = -1;
        unsigned flags = IORING_ASYNC_CANCEL_FD;
    };

    template<typename F>
    cancel_fd_operation(iouxx::io_uring_xx&, F) -> cancel_fd_operation<std::decay_t<F>>;

    cancel_fd_operation(iouxx::io_uring_xx&) -> cancel_fd_operation<void>;

} // namespace iouxx::iouops

#endif // IOUXX_OPERATION_CANCEL_H
