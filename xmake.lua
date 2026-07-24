add_rules("mode.debug", "mode.release")
add_includedirs("include")
add_defines("IOUXX_CONFIG_ENABLE_FEATURE_TESTS")
add_requires("liburing >=2.14")
set_languages("c++26")
set_encodings("utf-8")
add_rules("plugin.compile_commands.autoupdate", {outputdir = ".vscode"})

local function scan_and_add_tests(testdir)
    for _, testfile in ipairs(os.files(testdir .. "/test_*.cpp")) do
        add_tests(path.basename(testfile), {
            files = testfile,
            kind = "binary",
        })
    end
end

local function configure_toolchains(name)
    if name == "clang" then
        set_toolchains("clang")
        set_runtimes("c++_shared")
    elseif name == "gcc" then
        set_toolchains("gcc")
        set_runtimes("stdc++_shared")
        add_cxflags("-Wno-interference-size")
-- Note: module build passes with gcc trunk (GCC 17), but it dont work well with contracts.
-- It could be disabled by defining IOUXX_CONFIG_NOT_USE_CONTRACTS and remove these flags.
        add_cxflags("-fcontracts")
        add_ldflags("-fcontracts")
    else
        raise("unknown toolchain: %s", name)
    end
end

target("iouxx")
    set_kind("headeronly")
    add_packages("liburing")
    add_headerfiles("include/(**/*.hpp)")

target("llvm")
    set_kind("headeronly")
    configure_toolchains("clang")
    add_packages("liburing")
    scan_and_add_tests("test")

target("gnu")
    set_kind("headeronly")
    configure_toolchains("gcc")
    add_packages("liburing")
    scan_and_add_tests("test")

-- target("llvm-test")
--     set_kind("binary")
--     add_files("src/main.cpp")
--     configure_toolchains("clang")
--     add_packages("liburing")

-- target("gnu-test")
--     set_kind("binary")
--     add_files("src/main.cpp")
--     configure_toolchains("gcc")
--     add_packages("liburing")

target("llvm-module")
    set_kind("binary")
    add_files("src/modules/**/*.mpp", { public = true })
    add_files("src/modules/*.mpp", { public = true })
    configure_toolchains("clang")
    set_policy("build.c++.modules", true)
    add_packages("liburing")
    add_defines("IOUXX_CONFIG_USE_CXX_MODULE")
    scan_and_add_tests("test")

-- FIXME: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=124264
-- target("gnu-module")
--     set_kind("binary")
--     add_files("src/modules/**/*.mpp", { public = true })
--     add_files("src/modules/*.mpp", { public = true })
--     configure_toolchains("gcc")
--     set_policy("build.c++.modules", true)
--     set_policy("build.c++.modules.gcc.cxx11abi", true)
--     add_packages("liburing")
--     add_defines("IOUXX_CONFIG_USE_CXX_MODULE")
--     scan_and_add_tests("test")
