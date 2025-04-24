# ----------------------- #
#  Project Configuration  #
# ----------------------- #
project(
  'draconis++',
  'cpp',
  version : '0.1.0',
  default_options : [
    'default_library=static',
    'buildtype=debugoptimized',
    'b_vscrt=mt',
    'warning_level=3',
  ],
)

cpp = meson.get_compiler('cpp')
host_system = host_machine.system()

# ------------------------ #
#  Compiler Configuration  #
# ------------------------ #
common_warning_flags = [
  '-Wno-c++20-compat',
  '-Wno-c++20-extensions',
  '-Wno-c++98-compat',
  '-Wno-c++98-compat-pedantic',
  '-Wno-disabled-macro-expansion',
  '-Wno-missing-prototypes',
  '-Wno-padded',
  '-Wno-pre-c++20-compat-pedantic',
  '-Wunused-function',
]

common_cpp_flags = {
  'common' : [
    '-fno-strict-enums',
    '-fvisibility=hidden',
    '-fvisibility-inlines-hidden',
    '-std=c++26',
  ],
  'msvc' : [
    '-DNOMINMAX',
    '/MT',
    '/Zc:__cplusplus',
    '/Zc:preprocessor',
    '/external:W0',
    '/external:anglebrackets',
    '/std:c++latest',
    '/w44668',
    '/w44710',
    '/w44820',
  ],
  'unix_extra' : [
    '-march=native',
    '-nostdlib++',
  ],
  'windows_extra' : '-DCURL_STATICLIB',
}

# Configure Objective-C++ for macOS
if host_system == 'darwin'
  add_languages('objcpp', native : false)
  objcpp = meson.get_compiler('objcpp')
  objcpp_flags = common_warning_flags + [
    '-Wno-switch-default',
    '-std=c++23',
    '-fvisibility=hidden',
    '-fvisibility-inlines-hidden',
  ]
  add_project_arguments(objcpp.get_supported_arguments(objcpp_flags), language : 'objcpp')
endif

# Apply C++ compiler arguments
if cpp.get_id() in ['msvc', 'clang-cl']
  common_cpp_args = common_cpp_flags['msvc']
  if cpp.get_id() == 'clang-cl'
    common_cpp_args += common_warning_flags + common_cpp_flags['common']
  endif
else
  common_cpp_args = common_warning_flags + common_cpp_flags['common'] + common_cpp_flags['unix_extra']
  if host_system == 'windows'
    common_cpp_args += common_cpp_flags['windows_extra']
  endif
  common_cpp_args = cpp.get_supported_arguments(common_cpp_args)
endif

add_project_arguments(common_cpp_args, language : 'cpp')

# ------- #
#  Files  #
# ------- #
base_sources = files('src/core/system_data.cpp', 'src/config/config.cpp', 'src/config/weather.cpp', 'src/main.cpp')

platform_sources = {
  'linux' : ['src/os/linux.cpp', 'src/os/linux/issetugid_stub.cpp', 'src/os/linux/display_guards.cpp'],
  'darwin' : ['src/os/macos.cpp', 'src/os/macos/bridge.mm'],
  'windows' : ['src/os/windows.cpp'],
}

sources = base_sources + files(platform_sources.get(host_system, []))

# --------------------- #
#  Dependencies Config  #
# --------------------- #
common_deps = [
  dependency('libcurl', include_type : 'system', static : true),
  dependency('tomlplusplus', include_type : 'system', static : true),
  dependency('openssl', include_type : 'system', static : true, required : false),
]

# Platform-specific dependencies
platform_deps = []

if host_system == 'darwin'
  platform_deps += [
    dependency(
      'appleframeworks',
      modules : ['foundation', 'mediaplayer', 'systemconfiguration'],
      static : true,
    ),
    dependency('iconv'),
  ]
elif host_system == 'windows'
  platform_deps += [
    cpp.find_library('dwmapi'),
    cpp.find_library('windowsapp'),
  ]
elif host_system == 'linux'
  platform_deps += [
    dependency('SQLiteCpp'),
    dependency('x11'),
    dependency('xcb'),
    dependency('xau'),
    dependency('xdmcp'),
    dependency('wayland-client'),
    dependency('sigc++-3.0', include_type : 'system'),
    dependency('dbus-cxx', include_type : 'system'),
  ]
endif

# FTXUI configuration
ftxui_components = ['ftxui::screen', 'ftxui::dom', 'ftxui::component']
ftxui_dep = dependency(
  'ftxui',
  modules : ftxui_components,
  include_type : 'system',
  static : true,
  required : false,
)

if not ftxui_dep.found()
  ftxui_dep = declare_dependency(
    dependencies : [
      dependency('ftxui-dom', fallback : ['ftxui', 'dom_dep']),
      dependency('ftxui-screen', fallback : ['ftxui', 'screen_dep']),
      dependency('ftxui-component', fallback : ['ftxui', 'component_dep']),
    ],
  )
endif

glaze_dep = dependency('glaze', include_type : 'system', required : false)

if not glaze_dep.found()
  cmake = import('cmake')

  glaze_proj = cmake.subproject('glaze')

  glaze_dep = glaze_proj.dependency('glaze_glaze', include_type : 'system')
endif

# Combine all dependencies
deps = common_deps + platform_deps + ftxui_dep + glaze_dep

# ------------------------- #
#  Link/ObjC Configuration  #
# ------------------------- #
link_args = []
objc_args = []

if host_system == 'darwin'
  objc_args += ['-fobjc-arc']
elif cpp.get_id() == 'clang'
  link_args += ['-static-libgcc', '-static-libstdc++']
endif

# ------------------- #
#  Executable Target  #
# ------------------- #
executable(
  'draconis++',
  sources,
  objc_args : objc_args,
  link_args : link_args,
  dependencies : deps,
  install : true,
)
