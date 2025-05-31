add_rules("mode.debug", "mode.release")

add_repositories("liteldev-repo https://github.com/LiteLDev/xmake-repo.git")

option("shared")
    set_default(true)
    set_showmenu(true)
    set_description("Build shared library")
option_end()

add_requires("sqlitecpp 3.3.2", {configs = {shared = get_config("shared")}})
add_requires(
    "levilamina 1.2.0",
    "levibuildscript",
    "nlohmann_json 3.11.3"
)

if not has_config("vs_runtime") then
    set_runtimes("MD")
end

set_version("1.6.5")

target("LOICollectionA")
    add_rules("@levibuildscript/linkrule")
    add_cxflags(
        "/EHa",
        "/utf-8",
        "/permissive-",
        "/Ob3",
        "/W4",
        "/w44265",
        "/w44289",
        "/w44296",
        "/w45263",
        "/w44738",
        "/w45204"
    )
    add_defines(
        "NOMINMAX",
        "UNICODE",
        "LOICOLLECTION_A_EXPORTS"
    )
    add_files(
        "src/plugin/**.cpp"
    )
    add_includedirs(
        "src/plugin"
    )
    add_packages(
        "levilamina",
        "nlohmann_json",
        "sqlitecpp"
    )
    set_exceptions("none")
    set_kind("shared")
    set_languages("cxx20")
    set_symbols("debug")

    if is_mode("debug") then
        add_defines("DEBUG")
    elseif is_mode("release") then
        add_defines("NDEBUG")
    end

    after_build(function (target)
        local plugin_packer = import("scripts.after_build")

        local major, minor, patch = target:version():match("(%d+)%.(%d+)%.(%d+)")
        local plugin_define = {
            pluginName = target:name(),
            pluginFile = path.filename(target:targetfile()),
            pluginVersion = major .. "." .. minor .. "." .. patch,
        }
        
        plugin_packer.pack_plugin(target, plugin_define)
    end)
