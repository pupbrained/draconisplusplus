---@diagnostic disable: undefined-field, undefined-global

add_requires("fmt", "libcurl", "tomlplusplus", "yyjson")

if os.host() == "macosx" then
	add_requires("Foundation", "MediaPlayer", "SystemConfiguration", "iconv")
elseif os.host() == "linux" or os.host() == "bsd" then
	add_requires("sdbus-c++", "x11")
end

add_cxxflags(
	"-Wno-c++20-compat",
	"-Wno-c++20-extensions",
	"-Wno-c++98-compat",
	"-Wno-c++98-compat-pedantic",
	"-Wno-disabled-macro-expansion",
	"-Wno-missing-prototypes",
	"-Wno-padded",
	"-Wno-pre-c++20-compat-pedantic",
	"-Wno-switch-default",
	"-Wunused-function",
	"-fvisibility=hidden"
)

if os.host() == "macosx" then
	add_mxxflags(
		"-Wno-c++20-compat",
		"-Wno-c++20-extensions",
		"-Wno-c++98-compat",
		"-Wno-c++98-compat-pedantic",
		"-Wno-disabled-macro-expansion",
		"-Wno-missing-prototypes",
		"-Wno-padded",
		"-Wno-pre-c++20-compat-pedantic",
		"-Wno-switch-default",
		"-Wunused-function",
		"-fvisibility=hidden",
		"-fobjc-arc"
	)
elseif os.host() == "windows" then
	add_cxxflags("-DCURL_STATICLIB")
	add_ldflags("-LC:/Program Files (x86)/Windows Kits/10/Lib/10.0.22621.0/um/x64", "-lwindowsapp", "static")
end

target("draconis++")
set_languages("c++20")
set_kind("binary")

add_rules("plugin.compile_commands.autoupdate", { outputdir = "." })
add_rules("mode.debug", "mode.release")

add_files("src/main.cpp", "src/config/config.cpp", "src/config/weather.cpp")

if os.host() == "bsd" then
	add_files("src/os/freebsd.cpp")
elseif os.host() == "linux" then
	add_files("src/os/linux.cpp")
elseif os.host() == "macosx" then
	add_files("src/os/macos.cpp", "src/os/macos/bridge.mm")
elseif os.host() == "windows" then
	add_files("src/os/windows.cpp")
end

add_packages("fmt", "libcurl", "tomlplusplus", "yyjson")

if os.host() == "macosx" then
	add_packages("Foundation", "MediaPlayer", "SystemConfiguration", "iconv")
elseif os.host() == "linux" or os.host() == "bsd" then
	add_packages("sdbus-c++", "x11")
end
