add_rules("mode.debug", "mode.release")
add_includedirs("include")
add_defines("IOUXX_CONFIG_ENABLE_FEATURE_TESTS")
add_requires("liburing >= 2.12")
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

local function add_linkdir_rpath(dir)
    add_linkdirs(dir)
    add_rpathdirs(dir)
end

local function configure_toochains(name)
    if name == "clang" then
        set_toolchains("clang")
        set_runtimes("c++_shared")
        add_linkdir_rpath("/usr/local/lib/x86_64-unknown-linux-gnu")
    elseif name == "gcc" then
        set_toolchains("gcc")
        set_runtimes("stdc++_shared")
        add_cxxflags("-Wno-interference-size")
        add_linkdir_rpath("/usr/local/lib/../lib64")
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
    configure_toochains("clang")
    add_packages("liburing")
    scan_and_add_tests("test")

target("gnu")
    set_kind("headeronly")
    configure_toochains("gcc")
    add_packages("liburing")
    scan_and_add_tests("test")

-- target("llvm")
--     set_kind("binary")
--     add_files("test/.cpp")
--     configure_toochains("clang")
--     add_packages("liburing")
--     scan_and_add_tests("test")

-- target("gnu")
--     set_kind("binary")
--     add_files("test/.cpp")
--     configure_toochains("gcc")
--     add_packages("liburing")
--     scan_and_add_tests("test")

-- target("llvm-module")
--     set_kind("binary")
--     add_files("src/modules/**/*.mpp", { public = true })
--     add_files("src/modules/*.mpp", { public = true })
--     configure_toochains("clang")
--     set_policy("build.c++.modules", true)
--     add_packages("liburing")
--     add_defines("IOUXX_CONFIG_USE_CXX_MODULE")
--     scan_and_add_tests("test")

-- target("gnu-module")
--     set_kind("binary")
--     add_files("src/modules/**/*.mpp", { public = true })
--     add_files("src/modules/*.mpp", { public = true })
--     configure_toochains("gcc")
--     set_policy("build.c++.modules", true)
--     set_policy("build.c++.modules.gcc.cxx11abi", true)
--     add_packages("liburing")
--     add_defines("IOUXX_CONFIG_USE_CXX_MODULE")
