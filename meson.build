# ----------------------- #
#  Project Configuration  #
# ----------------------- #
project(
  'draconis++',
  'cpp',
  version: '0.1.0',
  meson_version: '>=1.1',
  default_options: [
    'default_library=static',
    'buildtype=debugoptimized',
    'b_vscrt=mt',
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
  '-Wno-gnu-conditional-omitted-operand',
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
    '/MT',
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
project_internal_includes = include_directories('src/lib')
project_public_includes = include_directories('include', is_system: true)

# ------- #
#  Files  #
# ------- #
lib_base_sources = [
  files(
    'src/lib/Config/Config.cpp',
    'src/lib/Core/System.cpp',
    'src/lib/UI/UI.cpp',
  ),
]

if get_option('enable_packagecount') == true
  lib_base_sources += files('src/lib/Services/PackageCounting.cpp')
endif

if get_option('enable_weather') == true
  lib_base_sources += files(
    'src/lib/Services/Weather/MetNoService.cpp',
    'src/lib/Services/Weather/OpenMeteoService.cpp',
    'src/lib/Services/Weather/OpenWeatherMapService.cpp',
    'src/lib/Services/Weather/WeatherUtils.cpp',
  )
endif

main_app_sources = files('src/main.cpp')

platform_sources = {
  'darwin': ['src/lib/OS/macOS.cpp', 'src/lib/OS/macOS/bridge.mm'],
  'dragonfly': ['src/lib/OS/BSD.cpp'],
  'freebsd': ['src/lib/OS/BSD.cpp'],
  'haiku': ['src/lib/OS/Haiku.cpp'],
  'linux': ['src/lib/OS/Linux.cpp'],
  'netbsd': ['src/lib/OS/BSD.cpp'],
  'serenity': ['src/lib/OS/Serenity.cpp'],
  'windows': ['src/lib/OS/Windows.cpp'],
}

lib_platform_sources = files(platform_sources.get(host_system, []))
lib_all_sources = lib_base_sources + lib_platform_sources

# --------------------- #
#  Dependencies Config  #
# --------------------- #
common_deps = []

fs = import('fs')

if get_option('precompiled_config') == true
  add_project_arguments('-DDRAC_PRECOMPILED_CONFIG', language: ['cpp'])

  config_hpp_path = meson.project_source_root() / 'config.hpp'

  if not fs.is_file(config_hpp_path)
    error(
      'Option "precompiled_config" is enabled, but "@0@" was not found.'.format(config_hpp_path),
    )
  endif
else
  common_deps += dependency('tomlplusplus', include_type: 'system', static: true)
endif

if get_option('enable_nowplaying') == true
  add_project_arguments('-DDRAC_ENABLE_NOWPLAYING', language: ['cpp'])
endif

if get_option('enable_weather') == true
  common_deps += dependency('libcurl', include_type: 'system', static: true)
  add_project_arguments('-DDRAC_ENABLE_WEATHER', language: ['cpp'])
endif

if get_option('enable_packagecount') == true
  if host_system == 'darwin' or host_system == 'linux'
    common_deps += dependency('SQLiteCpp')
  endif

  add_project_arguments('-DDRAC_ENABLE_PACKAGECOUNT', language: ['cpp'])
endif

# Platform-specific dependencies
platform_deps = []

if host_system == 'darwin'
  platform_deps += [
    dependency(
      'appleframeworks',
      modules: ['foundation', 'iokit', 'mediaplayer', 'systemconfiguration'],
      static: true,
    ),
    dependency('iconv'),
  ]
elif host_system == 'windows'
  platform_deps += [
    cpp.find_library('dwmapi'),
    cpp.find_library('windowsapp'),
    cpp.find_library('setupapi'),
  ]
elif host_system != 'serenity' and host_system != 'haiku'
  dbus_dep = dependency('dbus-1', required: false)
  xcb_dep = dependency('xcb', required: false)
  xau_dep = dependency('xau', required: false)
  xdmcp_dep = dependency('xdmcp', required: false)
  wayland_dep = dependency('wayland-client', required: false)
  pugixml_dep = dependency('pugixml', required: false)

  if host_system == 'linux' and get_option('use_pugixml') == true and pugixml_dep.found()
    platform_deps += pugixml_dep
    add_project_arguments('-DHAVE_PUGIXML', language: 'cpp')
  elif not pugixml_dep.found()
    error('Option "use_pugixml" is enabled, but pugixml was not found.')
  endif

  if get_option('enable_nowplaying') == true and dbus_dep.found()
    platform_deps += dbus_dep
    add_project_arguments('-DHAVE_DBUS', language: 'cpp')
  elif not dbus_dep.found()
    error(
      'Option "enable_nowplaying" is enabled, but dbus (dbus-1/libdbus) was not found.',
    )
  endif

  if get_option('use_xcb') == true and xcb_dep.found() and xau_dep.found() and xdmcp_dep.found()
    platform_deps += [xcb_dep, xau_dep, xdmcp_dep]
    add_project_arguments('-DHAVE_XCB', language: 'cpp')
  elif not xcb_dep.found()
    error('Option "use_xcb" is enabled, but xcb was not found.')
  elif not xau_dep.found()
    error('Option "use_xcb" is enabled, but xau was not found (required by xcb).')
  elif not xdmcp_dep.found()
    error(
      'Option "use_xcb" is enabled, but xdmcp was not found (required by xcb).',
    )
  endif

  if get_option('use_wayland') == true and wayland_dep.found()
    platform_deps += wayland_dep
    add_project_arguments('-DHAVE_WAYLAND', language: 'cpp')
  elif not wayland_dep.found()
    error('Option "use_wayland" is enabled, but wayland-client was not found.')
  endif
endif

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

lib_deps = common_deps + platform_deps + ftxui_dep + glaze_dep

# ------------------------- #
#  Link/ObjC Configuration  #
# ------------------------- #
link_args = []
objc_args = []

if host_system == 'darwin'
  objc_args += ['-fobjc-arc']
elif host_system == 'windows'
  add_project_arguments(
    '-DNOMINMAX',
    '-DWIN32_LEAN_AND_MEAN',
    '-DWINRT_LEAN_AND_MEAN',
    language: ['cpp', 'objcpp'],
  )
elif host_system == 'linux'
  if get_option('build_for_musl') == true
    link_args += ['-static']
  else
    link_args += ['-static-libgcc', '-static-libstdc++']
  endif
elif host_system == 'haiku'
  link_args += ['-lpackage', '-lbe']
endif

# ----------------- #
#  Static Library   #
# ----------------- #
lib = static_library(
  'draconis++',
  lib_all_sources,
  include_directories: [project_internal_includes, project_public_includes],
  dependencies: lib_deps,
  objc_args: objc_args,
  install: false,
)

# ------------------- #
#  Executable Target  #
# ------------------- #
executable(
  'draconis++',
  main_app_sources,
  include_directories: [project_internal_includes, project_public_includes],
  link_with: lib,
  dependencies: lib_deps,
  link_args: link_args,
  objc_args: objc_args,
  install: true,
)

# -------------------- #
#  Example Executable  #
# -------------------- #
if get_option('build_examples')
  subdir('examples')
endif

# ----------------- #
#  Testing Support  #
# ----------------- #
gtest_dep = dependency('gtest', fallback: ['gtest', 'gtest_dep'], main: false, required: false)

if gtest_dep.found()
  test_common_args = ['--gtest_color=yes', '--gtest_print_time=1']
  test_common_includes = [project_internal_includes, project_public_includes]
  test_common_link_with = [lib]
  test_common_dependencies = [gtest_dep] + lib_deps

  individual_test_files_paths = [
    'tests/ConfigTest.cpp',
    'tests/CoreTypesTest.cpp',
    'tests/LoggingUtilsTest.cpp',
  ]

  if get_option('enable_weather')
    individual_test_files_paths += ['tests/WeatherServiceTest.cpp']
  endif

  foreach test_fpath : individual_test_files_paths
    test_name = fs.stem(test_fpath)

    current_test_exe = executable(
      test_name,
      files(test_fpath),
      include_directories: test_common_includes,
      link_with: test_common_link_with,
      dependencies: test_common_dependencies,
      link_args: link_args,
      objc_args: objc_args,
    )

    test(
      test_name,
      current_test_exe,
      args: test_common_args,
      protocol: 'gtest',
    )
  endforeach
endif