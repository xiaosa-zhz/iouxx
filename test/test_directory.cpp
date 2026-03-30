#ifdef IOUXX_CONFIG_USE_CXX_MODULE

import std;
import iouxx;

#else // !IOUXX_CONFIG_USE_CXX_MODULE

#include <system_error>
#include <string>
#include <string_view>
#include <print>
#include <cstdlib>
#include <filesystem>

#include "iouxx/iouringxx.hpp"
#include "iouxx/iouops/file/directory.hpp"
#include "iouxx/iouops/file/fileio.hpp"

#endif // IOUXX_CONFIG_USE_CXX_MODULE

#include <stdio.h>
#include <stdlib.h>

#define LOG_INFO(fmtstr, ...) \
    std::println("[INFO] " fmtstr __VA_OPT__(,) __VA_ARGS__)

#define LOG_ERR(fmtstr, ...) \
    std::println(stderr, "[ERROR] " fmtstr __VA_OPT__(,) __VA_ARGS__)

namespace fs = std::filesystem;

static iouxx::fileops::file create_temp_file(iouxx::ring& ring,
    const std::string& path)
{
    using namespace iouxx;
    auto open = ring.make_sync<fileops::file_open_operation>();
    open.path(path)
        .options(fileops::open_flag::create
            | fileops::open_flag::writeonly
            | fileops::open_flag::cloexec)
        .mode(fileops::open_mode::uread | fileops::open_mode::uwrite);
    if (auto res = open.submit_and_wait()) {
        return *res;
    } else {
        LOG_ERR("Failed to create temp file {}: {}", path, res.error().message());
        std::exit(1);
    }
}

static void close_file(iouxx::ring& ring, iouxx::fileops::file fd) {
    using namespace iouxx;
    auto close = ring.make_sync<fileops::file_close_operation>();
    close.file(fd);
    if (auto res = close.submit_and_wait(); !res) {
        LOG_ERR("Failed to close file: {}", res.error().message());
        std::exit(1);
    }
}

void test_mkdir(std::string_view base) {
    using namespace iouxx;
    ring ring(256);

    std::string dir_path = std::string(base) + "/subdir";

    {
        auto op = ring.make_sync<fileops::mkdir_operation>();
        op.path(dir_path)
            .mode(fileops::open_mode::uread
                | fileops::open_mode::uwrite
                | fileops::open_mode::uexec);
        if (auto res = op.submit_and_wait()) {
            LOG_INFO("mkdir: created {}", dir_path);
        } else {
            LOG_ERR("mkdir: failed to create {}: {}", dir_path, res.error().message());
            std::exit(1);
        }
    }

    if (!fs::is_directory(dir_path)) {
        LOG_ERR("mkdir: {} is not a directory", dir_path);
        std::exit(1);
    }

    // mkdir on existing dir should fail
    {
        auto op = ring.make_sync<fileops::mkdir_operation>();
        op.path(dir_path)
            .mode(fileops::open_mode::uread
                | fileops::open_mode::uwrite
                | fileops::open_mode::uexec);
        if (auto res = op.submit_and_wait()) {
            LOG_ERR("mkdir: should have failed on existing directory");
            std::exit(1);
        } else {
            LOG_INFO("mkdir: correctly failed on existing dir: {}", res.error().message());
        }
    }
}

void test_symlink(std::string_view base) {
    using namespace iouxx;
    ring ring(256);

    std::string target = std::string(base) + "/subdir";
    std::string link = std::string(base) + "/symlink_to_subdir";

    auto op = ring.make_sync<fileops::symlink_operation>();
    op.target(target)
        .link_path(link);
    if (auto res = op.submit_and_wait()) {
        LOG_INFO("symlink: created {} -> {}", link, target);
    } else {
        LOG_ERR("symlink: failed: {}", res.error().message());
        std::exit(1);
    }

    if (!fs::is_symlink(link)) {
        LOG_ERR("symlink: {} is not a symlink", link);
        std::exit(1);
    }

    if (fs::read_symlink(link) != target) {
        LOG_ERR("symlink: target mismatch");
        std::exit(1);
    }
}

void test_link(std::string_view base) {
    using namespace iouxx;
    ring ring(256);

    std::string original = std::string(base) + "/original_file";
    std::string hardlink = std::string(base) + "/hardlink_file";

    auto fd = create_temp_file(ring, original);
    close_file(ring, fd);

    auto op = ring.make_sync<fileops::link_operation>();
    op.old_path(original)
        .new_path(hardlink);
    if (auto res = op.submit_and_wait()) {
        LOG_INFO("link: created {} -> {}", hardlink, original);
    } else {
        LOG_ERR("link: failed: {}", res.error().message());
        std::exit(1);
    }

    if (!fs::exists(hardlink)) {
        LOG_ERR("link: {} does not exist", hardlink);
        std::exit(1);
    }

    if (fs::hard_link_count(original) != 2) {
        LOG_ERR("link: expected hard link count 2, got {}",
            fs::hard_link_count(original));
        std::exit(1);
    }
}

void test_rename(std::string_view base) {
    using namespace iouxx;
    ring ring(256);

    std::string old_name = std::string(base) + "/original_file";
    std::string new_name = std::string(base) + "/renamed_file";

    auto op = ring.make_sync<fileops::rename_operation>();
    op.old_path(old_name)
        .new_path(new_name);
    if (auto res = op.submit_and_wait()) {
        LOG_INFO("rename: {} -> {}", old_name, new_name);
    } else {
        LOG_ERR("rename: failed: {}", res.error().message());
        std::exit(1);
    }

    if (fs::exists(old_name)) {
        LOG_ERR("rename: old path {} still exists", old_name);
        std::exit(1);
    }

    if (!fs::exists(new_name)) {
        LOG_ERR("rename: new path {} does not exist", new_name);
        std::exit(1);
    }
}

void test_unlink(std::string_view base) {
    using namespace iouxx;
    ring ring(256);

    // Unlink regular files
    for (constexpr std::string_view names[] = { "/hardlink_file", "/renamed_file" };
        std::string_view name : names)
    {
        std::string path = std::string(base) + std::string(name);
        auto op = ring.make_sync<fileops::unlink_operation>();
        op.path(path);
        if (auto res = op.submit_and_wait()) {
            LOG_INFO("unlink: removed {}", path);
        } else {
            LOG_ERR("unlink: failed to remove {}: {}", path, res.error().message());
            std::exit(1);
        }
    }

    // Unlink symlink
    {
        std::string path = std::string(base) + "/symlink_to_subdir";
        auto op = ring.make_sync<fileops::unlink_operation>();
        op.path(path);
        if (auto res = op.submit_and_wait()) {
            LOG_INFO("unlink: removed symlink {}", path);
        } else {
            LOG_ERR("unlink: failed to remove symlink {}: {}", path, res.error().message());
            std::exit(1);
        }
    }

    // Unlink directory with AT_REMOVEDIR
    {
        std::string path = std::string(base) + "/subdir";
        auto op = ring.make_sync<fileops::unlink_operation>();
        op.path(path)
            .flags(fileops::unlink_flag::removedir);
        if (auto res = op.submit_and_wait()) {
            LOG_INFO("unlink: removed directory {}", path);
        } else {
            LOG_ERR("unlink: failed to remove directory {}: {}", path, res.error().message());
            std::exit(1);
        }
    }

    // Remove base directory
    {
        std::string path(base);
        auto op = ring.make_sync<fileops::unlink_operation>();
        op.path(path)
            .flags(fileops::unlink_flag::removedir);
        if (auto res = op.submit_and_wait()) {
            LOG_INFO("unlink: removed base directory {}", path);
        } else {
            LOG_ERR("unlink: failed to remove base directory {}: {}",
                path, res.error().message());
            std::exit(1);
        }
    }

    if (fs::exists(base)) {
        LOG_ERR("unlink: base directory {} still exists", base);
        std::exit(1);
    }
}

int main() {
    std::string tmpl = "/tmp/iouxx_test_XXXXXX";
    if (!::mkdtemp(tmpl.data())) {
        LOG_ERR("mkdtemp failed");
        return 1;
    }
    std::string_view base(tmpl);
    LOG_INFO("Test base directory: {}", base);
    test_mkdir(base);
    test_symlink(base);
    test_link(base);
    test_rename(base);
    test_unlink(base);
    LOG_INFO("All directory operation tests passed");
}
