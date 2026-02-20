# TODO List

## Work in Progress
- [ ] Add event related operations
- [ ] Add provided ring buffer and add support in recv/read operations

## High Priority
- [ ] Add module build for gcc when gcc 16 released
- [ ] Find a suitable environment to really test fixed fd/buffer

## Medium Priority
- [ ] Add epoll support
- [ ] Add version check for libiouxx itself when 0.1.0 is released
- [ ] Add file system related operations

## Low Priority
- [ ] Remove fallback around chrono when libc++ implementation is complete
- [ ] Remove start_lifetime_as in test_network.cpp when supported
- [ ] Use more start_lifetime_as in buffer related operations when supported

## Completed
- [x] ~~Find a suitable environment to really test networking~~
- [x] ~~Add direct fd tests in network tests~~
- [x] ~~Add async mutex support~~
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
- [x] ~~Update test coverage in README~~
- [x] ~~Add work thread related registration~~
- [x] ~~Add enable R_DISABLED ring~~

## Postponed
- [ ] Add CI/CD pipeline (due to linux kernel in github actions is too old)

## Notes
- Add any additional notes or context here
- Use `- [ ]` for incomplete tasks
- Use `- [x]` for completed tasks
