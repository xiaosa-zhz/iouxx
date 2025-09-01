add_rules("mode.debug", "mode.release")
add_files("src/*.cpp")
add_includedirs("include")
add_defines("IOUXX_CONFIG_ENABLE_FEATURE_TESTS")
add_requires("liburing")
set_languages("c++26")
set_encodings("utf-8")

local function scan_and_add_tests(testdir)
    for _, testfile in ipairs(os.files(testdir .. "/test_*.cpp")) do
        add_tests(path.basename(testfile), {
            files = testfile,
            remove_files = "src/main.cpp",
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
        add_cxxflags("-stdlib=libc++")
        add_ldflags("-lc++")
        add_linkdir_rpath("/usr/local/lib/x86_64-unknown-linux-gnu")
    elseif name == "gcc" then
        set_toolchains("gcc")
        add_cxxflags("-Wno-interference-size")
        add_linkdir_rpath("/usr/local/lib/../lib64")
    else
        raise("unknown toolchain: %s", name)
    end
end

target("llvm")
    set_kind("binary")
    configure_toochains("clang")
    add_packages("liburing")
    scan_and_add_tests("test")

target("gnu")
    set_kind("binary")
    configure_toochains("gcc")
    add_packages("liburing")
    scan_and_add_tests("test")
