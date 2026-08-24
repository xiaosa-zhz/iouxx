add_rules("mode.debug", "mode.release", "mode.releasedbg")
add_includedirs("include")
set_languages("c++26")
set_encodings("utf-8")
-- Note: If anyone wants to enable -vD flags or to debug building itself, disable CDB gen.
--       It is very laggy and will generate a lot of more output.
-- add_rules("plugin.compile_commands.autoupdate", {outputdir = ".vscode"})

IOUXX_LIBURING_MIN_VERSION = "2.14" -- config liburing minimum version here
IOUXX_LIBURING_MIN_MAJOR, IOUXX_LIBURING_MIN_MINOR = IOUXX_LIBURING_MIN_VERSION:match("^(%d+)%.(%d+)$")

add_requires("liburing >=" .. IOUXX_LIBURING_MIN_VERSION)

includes("test/xmake.lua")

target("iouxx")
    set_kind("headeronly")
    add_packages("liburing")
    add_headerfiles("include/(**.hpp)", { public = true })
    set_policy("generator.compile_commands", false)

target("iouxx-modules")
    set_kind("moduleonly")
    add_packages("liburing", { public = true })
    set_policy("build.c++.modules", true)
    add_deps("iouxx")
    add_files("src/modules/**.mpp", { public = true })
    add_defines("IOUXX_CONFIG_USE_CXX_MODULE", { public = true })
    set_policy("generator.compile_commands", false)
