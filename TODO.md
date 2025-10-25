# TODO List

## Work in Progress
- [ ] Add module build for gcc when liburing upstream fixed
- [ ] Add event related operations
- [ ] Add provided ring buffer and add support in recv/read operations

## High Priority
- [ ] Find a suitable environment to really test networking
- [ ] Find a suitable environment to really test fixed fd/buffer
- [ ] Add a default buffer management utility

## Medium Priority
- [ ] Add epoll support
- [ ] Add version check for libiouxx itself when 0.1.0 is released

## Low Priority
- [ ] Remove fallback around chrono when libc++ implementation is complete
- [ ] Remove start_lifetime_as in test_network.cpp when supported
- [ ] Use more start_lifetime_as in buffer related operations when supported

## Completed
- [x] ~~Rewrite registration mechanism~~
- [x] ~~Implement socket IO operations in `include/iouops/socketio.hpp`~~
- [x] ~~Implement file IO operations in `include/iouops/fileio.hpp`~~
- [x] ~~Add registration of resources (files, buffers, etc.)~~
- [x] ~~Rewrite operations~~
- [x] ~~Add license file~~
- [x] ~~Add readme file~~
- [x] ~~Add test for cancel operation~~
- [x] ~~Add C++ module support~~
- [x] ~~Add io_uring_prep_cmd_sock operations~~
- [x] ~~Add version check for liburing in both compile time and runtime~~

## Postponed
- [ ] Add CI/CD pipeline (due to linux kernel in github actions is too old)

## Notes
- Add any additional notes or context here
- Use `- [ ]` for incomplete tasks
- Use `- [x]` for completed tasks