add_rules("mode.debug", "mode.release", "mode.releasedbg")
add_includedirs("include")
set_languages("c++26")
set_encodings("utf-8")
-- Note: If anyone wants to enable -vD flags or to debug building itself, disable CBD gen.
--       It is very laggy and will generate a lot of more output.
add_rules("plugin.compile_commands.autoupdate", {outputdir = ".vscode"})

local liburing_min_version = "2.14" -- config liburing minimum version here
local liburing_min_major, liburing_min_minor = liburing_min_version:match("^(%d+)%.(%d+)$")

add_requires("liburing >=" .. liburing_min_version)

local function scan_and_add_tests(testdir)
    for _, testfile in ipairs(os.files(testdir .. "/test_*.cpp")) do
        add_tests(path.basename(testfile), {
            files = testfile,
            kind = "binary",
            defines = {
                "IOUXX_LIBURING_MIN_VERSION=\"" .. liburing_min_version .. "\"",
                "IOUXX_LIBURING_MIN_MAJOR=" .. liburing_min_major,
                "IOUXX_LIBURING_MIN_MINOR=" .. liburing_min_minor,
                "IOUXX_CONFIG_ENABLE_FEATURE_TESTS"
            }
        })
    end
end

local function configure_toolchains(name)
    if name == "llvm" then
        set_toolchains("clang")
        set_runtimes("c++_shared")
    elseif name == "gnu" then
        set_toolchains("gcc")
        set_runtimes("stdc++_shared")
        add_cxflags("-Wno-interference-size")
-- Note: Module build passes with gcc trunk (GCC 17), but it dont work well with contracts.
--       It could be disabled by defining IOUXX_CONFIG_NOT_USE_CONTRACTS and remove these flags.
-- FIXME: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=124264
        -- add_cxflags("-fcontracts")
        -- add_ldflags("-fcontracts")
        add_defines("IOUXX_CONFIG_NOT_USE_CONTRACTS")
    else
        raise("unknown toolchain: %s", name)
    end
end

local function add_module_test_target(name)
    target(name .. "-iouxx-modules")
        set_kind("moduleonly")
        add_packages("liburing", {public = true})
        set_policy("build.c++.modules", true)
        add_files("src/modules/**.mpp", { public = true })
        add_defines("IOUXX_CONFIG_USE_CXX_MODULE", {public = true})
        set_default(false)
        configure_toolchains(name)
    target_end()
    target(name .. "-modules")
        set_kind("moduleonly")
        add_deps(name .. "-iouxx-modules")
        configure_toolchains(name)
        scan_and_add_tests("test")
        set_default(false)
    target_end()
end

local function add_test_target(name)
    target(name)
        set_kind("headeronly")
        add_packages("liburing", {public = true})
        configure_toolchains(name)
        scan_and_add_tests("test")
        set_default(false)
    target_end()
end

target("iouxx")
    set_kind("headeronly")
    add_packages("liburing")
    add_headerfiles("include/(**.hpp)", {public = true})

target("iouxx-modules")
    set_kind("moduleonly")
    add_packages("liburing", {public = true})
    set_policy("build.c++.modules", true)
    add_deps("iouxx")
    add_files("src/modules/**.mpp", { public = true })
    add_defines("IOUXX_CONFIG_USE_CXX_MODULE", {public = true})

add_test_target("llvm")
add_test_target("gnu")
add_module_test_target("llvm")
add_module_test_target("gnu") -- TODO: see details in configure_toolchains()
