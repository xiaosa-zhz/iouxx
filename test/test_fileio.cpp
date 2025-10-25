#ifdef IOUXX_CONFIG_USE_CXX_MODULE

import std;
import iouxx.ring;
import iouxx.ops.file.fileio;

#else // !IOUXX_CONFIG_USE_CXX_MODULE

#include <string_view>
#include <string>
#include <span>
#include <print>

#include "iouxx/iouringxx.hpp"
#include "iouxx/iouops/file/fileio.hpp"

#endif // IOUXX_CONFIG_USE_CXX_MODULE

int main() {
    using namespace iouxx;
    using namespace std::literals;
    ring ring(256);
    file::file fd = [&] {
        auto open = ring.make_sync<file::file_open_operation>();
        open.path("/tmp")
            .options(file::open_flag::temporary_file
                | file::open_flag::cloexec
                | file::open_flag::readwrite)
            .mode(file::open_mode::uread
                | file::open_mode::uwrite);
        if (auto res = open.submit_and_wait()) {
            std::println("Temporary fixed file opened with fd {}", res->native_handle());
            return *res;
        } else {
            std::println(stderr, "Fail to open temporary file: {}", res.error().message());
            std::abort();
        }
    }();
    auto write = ring.make_sync<file::file_write_operation>();
    auto msg = "Hello, io_uring fixed file!"sv;
    write.file(fd)
        .buffer(std::as_bytes(std::span(msg)))
        .offset(0);
    if (auto res = write.submit_and_wait()) {
        std::println("Wrote {} bytes to fixed file", *res);
    } else {
        std::println(stderr, "Fail to write to fixed file: {}", res.error().message());
        std::abort();
    }
    auto read = ring.make_sync<file::file_read_operation>();
    std::string buffer(msg.size() + 1, '\0');
    read.file(fd)
        .buffer(std::as_writable_bytes(std::span(buffer)))
        .offset(0);
    if (auto res = read.submit_and_wait()) {
        std::println("Read {} bytes from fixed file: {}", *res, buffer);
    } else {
        std::println(stderr, "Fail to read from fixed file: {}", res.error().message());
        std::abort();
    }
    auto close = ring.make_sync<file::file_close_operation>();
    close.file(fd);
    if (auto res = close.submit_and_wait()) {
        std::println("Fixed file closed");
    } else {
        std::println(stderr, "Fail to close fixed file: {}", res.error().message());
        std::abort();
    }
}
