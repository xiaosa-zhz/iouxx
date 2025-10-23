# [iouxx](https://github.com/xiaosa-zhz/iouxx)

A C++26 style thin wrapper around [liburing](https://github.com/axboe/liburing).

(Disclaimer: still in early stage, API may change)

(Also disclaimer: part of this document is written by AI)

## ✨ Features

- Wrapper of io_uring instance itself.
- Wrappers around following io_uring operations (IORING_OP_*): 
  - (list may be incomplete)
  - NOP
  - TIMEOUT, TIMEOUT_REMOVE
  - ASYNC_CANCEL
  - SOCKET, BIND, CONNECT, ACCEPT, LISTEN, SHUTDOWN
  - SEND, SEND_ZC, RECV
  - OPENAT, CLOSE
  - READ, READ_FIXED, WRITE, WRITE_FIXED
  - POLL_ADD, POLL_REMOVE
- Other helper facilities, such as IP address utilities and Linux specific timer.

## 🧱 Design Note

- Restrict the use of `user_data` in `io_uring_sqe` to callback object pointer only. All these callback objects are managed by user, and need to outlive the submission and completion of io_uring.
- The callback object (`*_operation` types defined in `iouxx::iouops`) is a 'One to rule them all' object, contains all arguments needed by io_uring and user-provided callback.
  - It provides a thin type-safe wrapper over `io_uring_sqe` and `io_uring_cqe`.
  - As long as its lifetime is guarenteed, all things need to be keeped alive is safe to use by io_uring and in the callback (except fd and buffer, which are specially treated by io_uring, and need a overall management mechanism anyway).
  - It is pinned and non-allocating. It is recommended to be embedded in the context of a larger asyncronous operation to combine allocation (e.g. on the 'stack' of coroutine).
- Provide convenience facilities:
  - `syncwait_callback` for simple synchronous wait use case (e.g. in tests).
  - `awaiter_callback` to transform most of io_uring operations into coroutine awaitable (naturally forked operations need to be treated seperately).
- Provide wrappers around registration API for io_uring fixed fd and buffer. These things still need to be managed by user.
  - Reserve lower 3 bits of `user_data` to tag the unregister operation types.
  - User can provide resource tags marking the registered resource, which will be returned in completion event.
  - User-provided resource tags are also required to not use lower 3 bits (can be proper aligned pointers).
  - Once unregistration callback is set and a tagged resource is unregistered, a completion event will be generated with the corresponding tag.
  - File descriptor and buffer can have seperate unregister callbacks.
- Provide C++20 module support (not yet due to upstream issues).

## 📦 Dependencies

| Dependency | Requirement |
|------|------|
| OS | Linux (support io_uring, recommended >= 6.8) |
| lib | [liburing](https://github.com/axboe/liburing) >= 2.12 |
| compiler | -std=c++26（recommended clang >= 20 / gcc >= 15） |
| building tool | [xmake](https://xmake.io) (>= 3.0.4 if build modules) |

## 🚀 Build

```bash
# Config (debug/release)
xmake f -m debug
# Build, noop if not build modules
xmake b
# Run tests
xmake test
```

## 🕹️ Examples

No examples yet, but test cases can be referenced for usage, see below.

## 🧪 Test Coverage

- `test_noop.cpp`: `iouops/noop.hpp`
- `test_timeout.cpp`: `iouops/timeout.hpp`
- `test_ip_utils.cpp`: `iouops/network/ip.hpp`
- `test_coro.cpp`: `awaiter_callback` in `iouringxx.hpp`
- `test_cancel.cpp`: `iouops/cancel.hpp`
- `test_network.cpp`: `iouops/network/socketio.hpp`, some features are not working on older kernels thus may not be covered.

## 🛣️ Roadmap / TODO

See [TODO.md](https://github.com/xiaosa-zhz/iouxx/blob/main/TODO.md)。

## 📄 License

[LICENSE](https://github.com/xiaosa-zhz/iouxx/blob/main/LICENSE)

## 🤝 Contribution

TODO
