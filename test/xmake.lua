local function scan_and_add_tests()
    for _, testfile in ipairs(os.files("test_*.cpp")) do
        add_tests(path.basename(testfile), {
            files = testfile,
            kind = "binary",
            defines = {
                "IOUXX_LIBURING_MIN_VERSION=\"" .. IOUXX_LIBURING_MIN_VERSION .. "\"",
                "IOUXX_LIBURING_MIN_MAJOR=" .. IOUXX_LIBURING_MIN_MAJOR,
                "IOUXX_LIBURING_MIN_MINOR=" .. IOUXX_LIBURING_MIN_MINOR,
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
-- Note: Module build passes with latest GCC (16.2), but it dont work well with contracts.
--       It could be disabled by defining IOUXX_CONFIG_NOT_USE_CONTRACTS and remove these flags.
-- FIXME: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=124264
        -- add_cxflags("-fcontracts")
        -- add_ldflags("-fcontracts")
        add_defines("IOUXX_CONFIG_NOT_USE_CONTRACTS")
    elseif name == "llvm-dropin" then
        set_toolchains("clang")
        set_runtimes("stdc++_shared")
        add_ldflags("-lgcc_s")
    else
        raise("unknown toolchain: %s", name)
    end
end

local function configure_liburing(name)
    local config = {}
    if name == "llvm" then
        config.toolchains = "clang"
        config.runtimes = "c++_shared"
    elseif name == "gnu" then
        config.toolchains = "gcc"
        config.runtimes = "stdc++_shared"
    elseif name == "llvm-dropin" then
        config.toolchains = "clang"
        config.runtimes = "stdc++_shared"
    else
        raise("unknown toolchain: %s", name)
    end
    add_requires("liburing~" .. name, { configs = config })
end

local function depends_on_liburing(name)
    add_packages("liburing~" .. name, { public = true })
end

local network_test_port_base = {
    ["llvm"] = 38080,
    ["gnu"] = 38084,
    ["llvm-dropin"] = 38088
}

local function add_module_test_target(name)
    target(name .. "-iouxx-modules")
        set_kind("moduleonly")
        depends_on_liburing(name)
        set_policy("build.c++.modules", true)
        add_files("../src/modules/**.mpp", { public = true })
        add_defines("IOUXX_CONFIG_USE_CXX_MODULE", {public = true})
        configure_toolchains(name)
        set_default(false)
        set_policy("generator.compile_commands", false)
    target_end()
    target(name .. "-modules")
        set_kind("moduleonly")
        add_deps(name .. "-iouxx-modules")
        configure_toolchains(name)
        add_defines("IOUXX_TEST_NETWORK_SERVER_PORT=" .. network_test_port_base[name] + 2)
        add_defines("IOUXX_TEST_NETWORK_CLIENT_PORT=" .. network_test_port_base[name] + 3)
        scan_and_add_tests()
        set_default(false)
        set_policy("generator.compile_commands", false)
    target_end()
end

local function add_test_target(name)
    target(name)
        set_kind("headeronly")
        depends_on_liburing(name)
        configure_toolchains(name)
        add_defines("IOUXX_TEST_NETWORK_SERVER_PORT=" .. network_test_port_base[name] + 0)
        add_defines("IOUXX_TEST_NETWORK_CLIENT_PORT=" .. network_test_port_base[name] + 1)
        scan_and_add_tests()
        set_default(false)
        -- This target generates CDB
    target_end()
end

local function generate_tests(toolchain)
    configure_liburing(toolchain)
    add_test_target(toolchain)
    add_module_test_target(toolchain)
end

for _, toolchain in ipairs({"llvm", "gnu", "llvm-dropin"}) do
    generate_tests(toolchain)
end
