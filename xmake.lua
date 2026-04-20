set_project("EnchantUnbound")
set_version("1.1.0")

set_languages("cxx23")

add_rules("mode.release")

add_cxflags("-O2", "-fvisibility=hidden", "-ffunction-sections", "-fdata-sections", "-flto", "-w")
add_ldflags("-Wl,--gc-sections", "-Wl,--strip-all", "-s")

add_repositories(
    "xmake-repo https://github.com/xmake-io/xmake-repo.git"
)

target("EnchantUnbound")
    set_kind("shared")
    add_files("src/main.cpp")
    add_includedirs("src")
    if is_arch("arm64-v8a") then
        add_linkdirs("lib/ARM64")
    elseif is_arch("armeabi-v7a") then
        add_linkdirs("lib/ARM")
    end
    
    add_links("GlossHook", "log")
