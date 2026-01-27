#include <system_error>
#ifdef IOUXX_CONFIG_USE_CXX_MODULE

import std;
import iouxx;

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

void test_fileops() {
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
            LOG_INFO("Temporary file opened with fd {}", res->native_handle());
            return *res;
        } else {
            LOG_ERR("Fail to open temporary file: {}", res.error().message());
            std::exit(1);
        }
    }();
    std::string_view msg = "Hello, io_uring file!";
    {
        auto write = ring.make_sync<fileops::file_write_operation>();
        write.file(fd)
            .buffer(std::as_bytes(std::span(msg)))
            .offset(0);
        if (auto res = write.submit_and_wait()) {
            LOG_INFO("Wrote {} bytes to file", *res);
        } else {
            LOG_ERR("Fail to write to file: {}", res.error().message());
            std::exit(1);
        }
    }
    {
        std::string buffer(msg.size(), '\0');
        auto read = ring.make_sync<fileops::file_read_operation>();
        read.file(fd)
            .buffer(std::as_writable_bytes(std::span(buffer)))
            .offset(0);
        if (auto res = read.submit_and_wait()) {
            LOG_INFO("Read {} bytes from file: {}", *res, buffer);
        } else {
            LOG_ERR("Fail to read from file: {}", res.error().message());
            std::exit(1);
        }
        if (buffer != msg) {
            LOG_ERR("Data read does not match data written");
            std::exit(1);
        }
    }
    {
        auto close = ring.make_sync<fileops::file_close_operation>();
        close.file(fd);
        if (auto res = close.submit_and_wait()) {
            LOG_INFO("File closed");
        } else {
            LOG_ERR("Fail to close file: {}", res.error().message());
            std::exit(1);
        }
    }
}

void test_fileops_fixed() {
    using namespace iouxx;
    ring ring(256);
    if (std::error_code ec = ring.register_direct_descriptor_table(64)) {
        LOG_ERR("Fail to register direct descriptor table: {}", ec.message());
        std::exit(1);
    }
    fileops::fixed_file fd = [&] {
        auto open = ring.make_sync<fileops::fixed_file_open_operation>();
        open.path("/tmp")
            .options(fileops::open_flag::temporary_file
                | fileops::open_flag::readwrite)
            .mode(fileops::open_mode::uread
                | fileops::open_mode::uwrite);
        if (auto res = open.submit_and_wait()) {
            LOG_INFO("Temporary fixed file opened with index {}", res->index());
            return *res;
        } else {
            LOG_ERR("Fail to open temporary file: {}", res.error().message());
            std::exit(1);
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
            std::exit(1);
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
            std::exit(1);
        }
        if (buffer != msg) {
            LOG_ERR("Data read does not match data written: expected '{}', got '{}'", msg, buffer);
            std::exit(1);
        }
    }
    {
        auto close = ring.make_sync<fileops::file_close_operation>();
        close.file(fd);
        if (auto res = close.submit_and_wait()) {
            LOG_INFO("Fixed file closed");
        } else {
            LOG_ERR("Fail to close fixed file: {}", res.error().message());
            std::exit(1);
        }
    }
}

int main() {
    test_fileops();
    test_fileops_fixed();
}
