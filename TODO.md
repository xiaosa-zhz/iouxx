# TODO List

## Work in Progress
- [ ] Implement socket IO operations in `include/iouops/socketio.hpp`
- [ ] Implement file IO operations in `include/iouops/fileio.hpp`
- [ ] Add module build for gcc when liburing upstream fixed

## High Priority
- [ ] Find a suitable environment to really test networking
- [ ] Add io_uring_prep_cmd_sock operations

## Medium Priority
- [ ] Add registration of resources (files, buffers, etc.)
- [ ] Add version check in both compile time and runtime (when liburing has it)
- [ ] Add version check for libiouxx itself when 0.1.0 is released

## Low Priority
- [ ] Remove fallback around chrono when libc++ implementation is complete
- [ ] Remove start_lifetime_as in test_network.cpp when supported

## Completed
- [x] ~~Rewrite operations~~
- [x] ~~Add license file~~
- [x] ~~Add readme file~~
- [x] ~~Add test for cancel operation~~
- [x] ~~Add C++ module support~~

## Postponed
- [ ] Add CI/CD pipeline (due to linux kernel in github actions is too old)

## Notes
- Add any additional notes or context here
- Use `- [ ]` for incomplete tasks
- Use `- [x]` for completed tasks