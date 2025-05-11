# ----------------------- #
#  Project Configuration  #
# ----------------------- #
project(
  'draconis++',
  'cpp',
  version: '0.1.0',
  default_options: [
    'default_library=static',
    'buildtype=debugoptimized',
    'b_vscrt=mt',
    'b_lto=true',
    'b_ndebug=if-release',
    'warning_level=3',
  ],
)

add_project_arguments(
  '-DDRACONISPLUSPLUS_VERSION="' + meson.project_version() + '"',
  language: ['cpp', 'objcpp'],
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
  '-Wno-unused-command-line-argument',
  '-Wunused-function',
]

common_cpp_flags = {
  'common': [
    '-fno-strict-enums',
    '-fvisibility=hidden',
    '-fvisibility-inlines-hidden',
    '-std=c++26',
  ],
  'msvc': [
    '-DNOMINMAX', '/MT',
    '/Zc:__cplusplus',
    '/Zc:preprocessor',
    '/external:W0',
    '/external:anglebrackets',
    '/std:c++latest',
  ],
  'unix_extra': '-march=native',
  'windows_extra': '-DCURL_STATICLIB',
}

# Configure Objective-C++ for macOS
if host_system == 'darwin'
  add_languages('objcpp', native: false)
  objcpp = meson.get_compiler('objcpp')
  objcpp_flags = common_warning_flags + [
    '-std=c++26',
    '-fvisibility=hidden',
    '-fvisibility-inlines-hidden',
  ]
  add_project_arguments(objcpp.get_supported_arguments(objcpp_flags), language: 'objcpp')
endif

# Apply C++ compiler arguments
if cpp.get_id() in ['msvc', 'clang-cl']
  common_cpp_args = common_cpp_flags['msvc']
  if cpp.get_id() == 'clang-cl'
    common_cpp_args += common_warning_flags + common_cpp_flags['common'] + ['-fcolor-diagnostics', '-fdiagnostics-format=clang']
  endif
else
  common_cpp_args = common_warning_flags + common_cpp_flags['common']
  if host_system == 'windows'
    common_cpp_args += common_cpp_flags['windows_extra']
  elif host_system != 'serenity'
    common_cpp_args += common_cpp_flags['unix_extra']
  endif
endif

add_project_arguments(common_cpp_args, language: 'cpp')

# --------------------- #
#  Include Directories  #
# --------------------- #
project_internal_includes = include_directories('src')
project_public_includes = include_directories('include', is_system: true)

# ------- #
#  Files  #
# ------- #
base_sources = files(
  'src/Config/Config.cpp',
  'src/Core/SystemData.cpp',
  'src/Services/PackageCounting.cpp',
  'src/Services/Weather/MetNoService.cpp',
  'src/Services/Weather/OpenMeteoService.cpp',
  'src/Services/Weather/OpenWeatherMapService.cpp',
  'src/UI/UI.cpp',
  'src/main.cpp',
)

platform_sources = {
  'darwin': ['src/OS/macOS.cpp', 'src/OS/macOS/bridge.mm'],
  'dragonfly': ['src/OS/bsd.cpp'],
  'freebsd': ['src/OS/bsd.cpp'],
  'haiku': ['src/OS/haiku.cpp'],
  'linux': ['src/OS/linux.cpp'],
  'netbsd': ['src/OS/bsd.cpp'],
  'serenity': ['src/OS/serenity.cpp'],
  'windows': ['src/OS/Windows.cpp'],
}

sources = base_sources + files(platform_sources.get(host_system, []))

# --------------------- #
#  Dependencies Config  #
# --------------------- #
common_deps = [
  dependency('libcurl', include_type: 'system', static: true),
  dependency('tomlplusplus', include_type: 'system', static: true),
  dependency('openssl', include_type: 'system', static: true, required: false),
]

# Platform-specific dependencies
platform_deps = []

if host_system == 'darwin'
  platform_deps += [
    dependency('SQLiteCpp'),
    dependency(
      'appleframeworks',
      modules: ['foundation', 'mediaplayer', 'systemconfiguration'],
      static: true,
    ),
    dependency('iconv'),
  ]
elif host_system == 'windows'
  platform_deps += [
    cpp.find_library('dwmapi'),
    cpp.find_library('windowsapp'),
  ]
elif host_system != 'serenity' and host_system != 'haiku'
  # Make dbus, x11, and wayland dependencies optional
  dbus_dep = dependency('dbus-1', required: false)
  xcb_dep = dependency('xcb', required: false)
  xau_dep = dependency('xau', required: false)
  xdmcp_dep = dependency('xdmcp', required: false)
  wayland_dep = dependency('wayland-client', required: false)

  platform_deps += [
    dependency('SQLiteCpp'),
    dependency('pugixml'),
  ]

  if dbus_dep.found()
    platform_deps += dbus_dep
    add_project_arguments('-DHAVE_DBUS', language: 'cpp')
  endif
  if xcb_dep.found() and xau_dep.found() and xdmcp_dep.found()
    platform_deps += [xcb_dep, xau_dep, xdmcp_dep]
    add_project_arguments('-DHAVE_XCB', language: 'cpp')
  endif
  if wayland_dep.found()
    platform_deps += wayland_dep
    add_project_arguments('-DHAVE_WAYLAND', language: 'cpp')
  endif
endif

# FTXUI configuration
ftxui_components = ['ftxui::screen', 'ftxui::dom', 'ftxui::component']
ftxui_dep = dependency(
  'ftxui',
  modules: ftxui_components,
  include_type: 'system',
  static: true,
  required: false,
)

if not ftxui_dep.found()
  ftxui_dep = declare_dependency(
    dependencies: [
      dependency('ftxui-dom', fallback: ['ftxui', 'dom_dep']),
      dependency('ftxui-screen', fallback: ['ftxui', 'screen_dep']),
      dependency('ftxui-component', fallback: ['ftxui', 'component_dep']),
    ],
  )
endif

glaze_dep = dependency('glaze', include_type: 'system', required: false)

if not glaze_dep.found()
  cmake = import('cmake')
  glaze_proj = cmake.subproject('glaze')
  glaze_dep = glaze_proj.dependency('glaze_glaze', include_type: 'system')
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
elif host_system == 'linux'
  link_args += ['-static']
elif host_system == 'haiku'
  link_args += ['-lpackage', '-lbe']
endif

# ------------------- #
#  Executable Target  #
# ------------------- #
executable(
  'draconis++',
  sources,
  include_directories: [project_internal_includes, project_public_includes],
  objc_args: objc_args,
  link_args: link_args,
  dependencies: deps,
  install: true,
)