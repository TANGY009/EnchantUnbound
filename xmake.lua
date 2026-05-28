set_project("EnchantUnbound")
set_version("1.1.1")

add_rules("mode.release")
add_repositories("xmake-repo https://github.com/xmake-io/xmake-repo.git")

option("dev")
    set_default(false)
    set_showmenu(true)
    add_defines("_DEV")
option_end()

target("EnchantUnbound")
    set_kind("shared")
    add_options("dev")
    
    add_files("src/*.cpp")
    add_includedirs("src")
    set_policy("build.optimization.lto", true)
    
    if is_plat("windows") then
        set_languages("c++20")
        set_symbols("debug")
        add_defines("WIN32_LEAN_AND_MEAN")
        add_syslinks("kernel32", "user32")
        
        add_cxflags("/Os", "/GF", "/Gy", "/Gw", "/w")
        add_ldflags("/OPT:REF", "/OPT:ICF")
        
        local suffix = has_config("dev") and "debug" or "release"
        set_targetdir("build/windows/x86_64/" .. suffix)

    elseif is_plat("android") then
        set_languages("cxx23")
        add_links("log")
        
        add_cxflags("-O2", "-fvisibility=hidden", "-ffunction-sections", "-fdata-sections", "-flto", "-w")
        add_ldflags("-Wl,--gc-sections", "-Wl,--strip-all", "-Wl,-z,max-page-size=16384", "-s")
    end