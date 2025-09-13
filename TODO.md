# TODO List

## Work in Progress
- [ ] Implement socket IO operations in `include/iouops/socketio.hpp`
- [ ] Implement file IO operations in `include/iouops/fileio.hpp`
- [ ] Add C++ module support
- [ ] Add module build for gcc when liburing upstream fixed

## High Priority
- [ ] Find a suitable environment to really test networking

## Medium Priority
- [ ] Add registration of resources (files, buffers, etc.)

## Low Priority
- [ ] Remove fallback around chrono when libc++ implementation is complete
- [ ] Remove start_lifetime_as in test_network.cpp when supported

## Completed
- [x] ~~Rewrite operations~~
- [x] ~~Add license file~~
- [x] ~~Add readme file~~
- [x] ~~Add test for cancel operation~~

## Postponed
- [ ] Add CI/CD pipeline (due to linux kernel in github actions is too old)

## Notes
- Add any additional notes or context here
- Use `- [ ]` for incomplete tasks
- Use `- [x]` for completed tasks