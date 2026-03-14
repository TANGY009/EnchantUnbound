set_project("EnchantUnbound")
set_version("1.0.0")

set_languages("cxx23")

add_rules("mode.release")

add_cxflags("-O2", "-fvisibility=hidden", "-ffunction-sections", "-fdata-sections", "-flto", "-w")
add_ldflags("-Wl,--gc-sections", "-Wl,--strip-all", "-s")

add_repositories(
    "xmake-repo https://github.com/xmake-io/xmake-repo.git",
    "levimc-repo https://github.com/LiteLDev/xmake-repo.git"
)

add_requires("preloader_android 0.1.14")

target("EnchantUnbound")
    set_kind("shared")
    add_files("src/main.cpp")
    add_packages("preloader_android")
    add_links("log")
