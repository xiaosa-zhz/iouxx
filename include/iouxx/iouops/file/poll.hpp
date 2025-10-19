#pragma once
#ifndef IOUXX_IOUOPS_FILE_POLL_H
#define IOUXX_IOUOPS_FILE_POLL_H 1

#ifndef IOUXX_USE_CXX_MODULE

#include <sys/poll.h>

#include <utility>

#include "iouxx/util/utility.hpp"
#include "iouxx/iouringxx.hpp"
#include "file.hpp"

#endif // IOUXX_USE_CXX_MODULE

namespace iouxx::details {

    class poll_base
    {
    public:
        template<typename Self>
        Self& file(this Self& self, file::file file) noexcept {
            self.fd = file.native_handle();
            self.is_fixed = false;
            return self;
        }

        template<typename Self>
        Self& file(this Self& self, file::fixed_file file) noexcept {
            self.fd = file.index();
            self.is_fixed = true;
            return self;
        }

    protected:
        int fd = -1;
        bool is_fixed = false;
    };

} // namespace iouxx::details

IOUXX_EXPORT
namespace iouxx::inline iouops::file {

    enum class poll_event : unsigned {
        in = POLLIN,
        pri = POLLPRI,
        out = POLLOUT,
        err = POLLERR,
        hup = POLLHUP,
        msg = POLLMSG,
    };

    constexpr poll_event operator|(poll_event lhs, poll_event rhs) noexcept {
        return static_cast<poll_event>(
            std::to_underlying(lhs) | std::to_underlying(rhs)
        );
    }

    template<utility::eligible_callback<poll_event> Callback>
    class file_poll_add_operation : public operation_base, private details::poll_base
    {
    public:
        template<utility::not_tag F>
        explicit file_poll_add_operation(iouxx::ring& ring, F&& f)
            noexcept(utility::nothrow_constructible_callback<F>) :
            operation_base(iouxx::op_tag<file_poll_add_operation>, ring),
            callback(std::forward<F>(f))
        {}

        template<typename F, typename... Args>
        explicit file_poll_add_operation(iouxx::ring& ring, std::in_place_type_t<F>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<F, Args...>) :
            operation_base(iouxx::op_tag<file_poll_add_operation>, ring),
            callback(std::forward<Args>(args)...)
        {}

        using callback_type = Callback;
        using result_type = poll_event;

        static constexpr std::uint8_t opcode = IORING_OP_POLL_ADD;

        using details::poll_base::file;

        file_poll_add_operation& events(poll_event events) & noexcept {
            this->mask = events;
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_poll_add(sqe, fd, std::to_underlying(mask));
            if (is_fixed) {
                sqe->flags |= IOSQE_FIXED_FILE;
            }
        }

        void do_callback(int ev, std::uint32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
            utility::eligible_nothrow_callback<callback_type, result_type>) {
            if (ev >= 0) {
                std::invoke(callback, static_cast<poll_event>(ev));
            } else {
                std::invoke(callback, utility::fail(-ev));
            }
        }

        poll_event mask{};
        [[no_unique_address]] callback_type callback;
    };

    struct multishot_poll_result {
        poll_event events;
        bool more;
    };

    template<utility::eligible_callback<multishot_poll_result> Callback>
    class file_poll_multishot_operation : public operation_base, private details::poll_base
    {
        static_assert(!utility::is_specialization_of_v<syncwait_callback, Callback>,
            "Multishot operation does not support syncronous wait.");
        static_assert(!utility::is_specialization_of_v<awaiter_callback, Callback>,
            "Multishot operation does not support coroutine await.");
    public:
        template<utility::not_tag F>
        explicit file_poll_multishot_operation(iouxx::ring& ring, F&& f)
            noexcept(utility::nothrow_constructible_callback<F>) :
            operation_base(iouxx::op_tag<file_poll_multishot_operation>, ring),
            callback(std::forward<F>(f))
        {}

        template<typename F, typename... Args>
        explicit file_poll_multishot_operation(iouxx::ring& ring, std::in_place_type_t<F>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<F, Args...>) :
            operation_base(iouxx::op_tag<file_poll_multishot_operation>, ring),
            callback(std::forward<Args>(args)...)
        {}

        using callback_type = Callback;
        using result_type = multishot_poll_result;

        static constexpr std::uint8_t opcode = IORING_OP_POLL_ADD;

        using details::poll_base::file;

        file_poll_multishot_operation& events(poll_event events) & noexcept {
            this->mask = events;
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_poll_multishot(sqe, fd, std::to_underlying(mask));
            if (is_fixed) {
                sqe->flags |= IOSQE_FIXED_FILE;
            }
        }

        void do_callback(int ev, std::uint32_t cqe_flags) IOUXX_CALLBACK_NOEXCEPT_IF(
            utility::eligible_nothrow_callback<callback_type, result_type>) {
            if (ev >= 0) {
                std::invoke(callback, multishot_poll_result{
                    static_cast<poll_event>(ev),
                    (cqe_flags & IORING_CQE_F_MORE) != 0
                });
            } else {
                std::invoke(callback, utility::fail(-ev));
            }
        }

        poll_event mask{};
        [[no_unique_address]] callback_type callback;
    };

    template<utility::eligible_callback<void> Callback>
    class file_poll_remove_operation : public operation_base
    {
    public:
        template<utility::not_tag F>
        explicit file_poll_remove_operation(iouxx::ring& ring, F&& f)
            noexcept(utility::nothrow_constructible_callback<F>) :
            operation_base(iouxx::op_tag<file_poll_remove_operation>, ring),
            callback(std::forward<F>(f))
        {}

        template<typename F, typename... Args>
        explicit file_poll_remove_operation(iouxx::ring& ring, std::in_place_type_t<F>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<F, Args...>) :
            operation_base(iouxx::op_tag<file_poll_remove_operation>, ring),
            callback(std::forward<Args>(args)...)
        {}

        using callback_type = Callback;
        using result_type = void;

        static constexpr std::uint8_t opcode = IORING_OP_POLL_REMOVE;

        file_poll_remove_operation& target(operation_identifier identifier) & noexcept {
            id = identifier;
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_poll_remove(sqe, id.user_data64());
        }

        void do_callback(int ev, std::uint32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
            utility::eligible_nothrow_callback<callback_type, result_type>) {
            if constexpr (utility::callback<callback_type, void>) {
                if (ev == 0) {
                    std::invoke(callback, utility::void_success());
                } else {
                    std::invoke(callback, utility::fail(-ev));
                }
            } else if constexpr (utility::errorcode_callback<callback_type>) {
                std::invoke(callback, utility::make_system_error_code(-ev));
            } else {
                static_assert(false, "Unreachable");
            }
        }

        operation_identifier id = operation_identifier();
        [[no_unique_address]] callback_type callback;
    };

    template<utility::eligible_callback<void> Callback>
    class file_poll_update_operation : public operation_base
    {
    public:
        template<utility::not_tag F>
        explicit file_poll_update_operation(iouxx::ring& ring, F&& f)
            noexcept(utility::nothrow_constructible_callback<F>) :
            operation_base(iouxx::op_tag<file_poll_update_operation>, ring),
            callback(std::forward<F>(f))
        {}

        template<typename F, typename... Args>
        explicit file_poll_update_operation(iouxx::ring& ring, std::in_place_type_t<F>, Args&&... args)
            noexcept(std::is_nothrow_constructible_v<F, Args...>) :
            operation_base(iouxx::op_tag<file_poll_update_operation>, ring),
            callback(std::forward<Args>(args)...)
        {}

        using callback_type = Callback;
        using result_type = void;

        static constexpr std::uint8_t opcode = IORING_OP_POLL_REMOVE;

        file_poll_update_operation& target(operation_identifier identifier) & noexcept {
            id = identifier;
            return *this;
        }

        file_poll_update_operation& events(poll_event events) & noexcept {
            this->mask = events;
            return *this;
        }

        file_poll_update_operation& multishot() & noexcept {
            flags |= IORING_POLL_ADD_MULTI;
            return *this;
        }

    private:
        friend operation_base;
        void build(::io_uring_sqe* sqe) & noexcept {
            ::io_uring_prep_poll_update(sqe, id.user_data64(), 0,
                std::to_underlying(mask), flags);
        }

        void do_callback(int ev, std::uint32_t) IOUXX_CALLBACK_NOEXCEPT_IF(
            utility::eligible_nothrow_callback<callback_type, result_type>) {
            if constexpr (utility::callback<callback_type, void>) {
                if (ev == 0) {
                    std::invoke(callback, utility::void_success());
                } else {
                    std::invoke(callback, utility::fail(-ev));
                }
            } else if constexpr (utility::errorcode_callback<callback_type>) {
                std::invoke(callback, utility::make_system_error_code(-ev));
            } else {
                static_assert(false, "Unreachable");
            }
        }

        operation_identifier id = operation_identifier();
        poll_event mask{};
        unsigned int flags = IORING_POLL_UPDATE_EVENTS;
        [[no_unique_address]] callback_type callback;
    };

} // namespace iouxx::iouops::file

#endif // IOUXX_IOUOPS_FILE_POLL_H
