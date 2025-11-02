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

#define LOG_INFO(fmtstr, ...) \
    std::println("[INFO] " fmtstr __VA_OPT__(,) __VA_ARGS__)

#define LOG_ERR(fmtstr, ...) \
    std::println(stderr, "[ERROR] " fmtstr __VA_OPT__(,) __VA_ARGS__)

int main() {
    using namespace iouxx;
    ring ring(256);
    fileops::file fd = [&] {
        auto open = ring.make_sync<fileops::file_open_operation>();
        open.path("/tmp")
            .options(fileops::open_flag::temporary_file
                | fileops::open_flag::cloexec
                | fileops::open_flag::readwrite)
            .mode(fileops::open_mode::uread
                | fileops::open_mode::uwrite);
        if (auto res = open.submit_and_wait()) {
            LOG_INFO("Temporary fixed file opened with fd {}", res->native_handle());
            return *res;
        } else {
            LOG_ERR("Fail to open temporary file: {}", res.error().message());
            std::abort();
        }
    }();
    std::string_view msg = "Hello, io_uring fixed file!";
    {
        auto write = ring.make_sync<fileops::file_write_operation>();
        write.file(fd)
            .buffer(std::as_bytes(std::span(msg)))
            .offset(0);
        if (auto res = write.submit_and_wait()) {
            LOG_INFO("Wrote {} bytes to fixed file", *res);
        } else {
            LOG_ERR("Fail to write to fixed file: {}", res.error().message());
            std::abort();
        }
    }
    {
        std::string buffer(msg.size(), '\0');
        auto read = ring.make_sync<fileops::file_read_operation>();
        read.file(fd)
            .buffer(std::as_writable_bytes(std::span(buffer)))
            .offset(0);
        if (auto res = read.submit_and_wait()) {
            LOG_INFO("Read {} bytes from fixed file: {}", *res, buffer);
        } else {
            LOG_ERR("Fail to read from fixed file: {}", res.error().message());
            std::abort();
        }
        if (buffer != msg) {
            LOG_ERR("Data read does not match data written");
            std::abort();
        }
    }
    {
        auto close = ring.make_sync<fileops::file_close_operation>();
        close.file(fd);
        if (auto res = close.submit_and_wait()) {
            LOG_INFO("Fixed file closed");
        } else {
            LOG_ERR("Fail to close fixed file: {}", res.error().message());
            std::abort();
        }
    }
}
