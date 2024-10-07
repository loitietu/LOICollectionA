add_rules("mode.debug", "mode.release")

add_repositories("liteldev-repo https://github.com/LiteLDev/xmake-repo.git")

add_requires("sqlite3 3.46.0+100", {configs = {shared = true}})
add_requires("sqlitecpp 3.2.1", {
    configs = {
        shared = true
    }
})
add_requires(
    "levilamina",
    "nlohmann_json 3.11.3"
)

if not has_config("vs_runtime") then
    set_runtimes("MD")
end

target("LOICollectionA")
    add_cxflags(
        "/EHa",
        "/utf-8",
        "/W4",
        "/w44265",
        "/w44289",
        "/w44296",
        "/w45263",
        "/w44738",
        "/w45204"
    )
    add_defines("NOMINMAX", "UNICODE")
    add_files("src/plugin/**.cpp")
    add_includedirs("src/plugin")
    add_packages(
        "levilamina",
        "nlohmann_json",
        "sqlite3",
        "sqlitecpp"
    )
    add_shflags("/DELAYLOAD:bedrock_server.dll")
    set_exceptions("none")
    set_kind("shared")
    set_languages("c++20")
    set_symbols("debug")

    after_build(function (target)
        local plugin_packer = import("scripts.after_build")

        local major, minor, patch = 1, 4, 8
        local plugin_define = {
            pluginName = target:name(),
            pluginFile = path.filename(target:targetfile()),
            pluginVersion = major .. "." .. minor .. "." .. patch,
        }
        
        plugin_packer.pack_plugin(target, plugin_define)
    end)
