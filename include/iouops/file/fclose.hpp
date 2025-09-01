#pragma once
#ifndef IOUXX_OPERATION_FILE_CLOSE_H
#define IOUXX_OPERATION_FILE_CLOSE_H 1

#include <expected>
#include <functional>

#include "iouringxx.hpp"
#include "util/utility.hpp"
#include "macro_config.hpp"

namespace iouxx::inline iouops::file {

    template<typename Callback>
        requires std::invocable<Callback, std::expected<void, std::error_code>>
        || std::invocable<Callback, std::error_code>
    class file_close_operation : public operation_base
    {
    public:
        template<typename F>
        explicit file_close_operation(iouxx::io_uring_xx& ring, F&& f) :
            operation_base(iouxx::op_tag<file_close_operation>, ring),
            callback(std::forward<F>(f))
        {}

        using callback_type = Callback;
        using result_type = void;

        static constexpr std::uint8_t IORING_OPCODE = IORING_OP_CLOSE;

        file_close_operation& file(int fd) & noexcept {
            this->fd = fd;
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_close(sqe, fd);
        }

        void do_callback(int ev, std::int32_t) IOUXX_CALLBACK_NOEXCEPT {
            if constexpr (std::invocable<Callback, std::expected<void, std::error_code>>) {
                if (ev == 0) {
                    std::invoke(callback, std::expected<void, std::error_code>{});
                } else {
                    std::invoke(callback, std::unexpected(
                        utility::make_system_error_code(-ev)
                    ));
                }
            } else if constexpr (std::invocable<Callback, std::error_code>) {
                std::invoke(callback, utility::make_system_error_code(-ev));
            } else {
                static_assert(utility::always_false<Callback>, "Unreachable");
            }
        }

        int fd = -1;
        [[no_unique_address]] callback_type callback;
    };

} // namespace iouxx::inline iouops::file

#endif // IOUXX_OPERATION_FILE_CLOSE_H
