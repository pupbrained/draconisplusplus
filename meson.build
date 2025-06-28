# ----------------------- #
#  Project Configuration  #
# ----------------------- #
project(
  'draconis++',
  ['cpp', 'c'],
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
  '-DDRAC_VERSION="' + meson.project_version() + '"',
  language: ['cpp', 'objcpp'],
)

cpp = meson.get_compiler('cpp')
host_system = host_machine.system()

# ------------------------ #
#  Compiler Configuration  #
# ------------------------ #
common_warning_flags = [
  '-Wno-c++17-extensions',
  '-Wno-c++20-compat',
  '-Wno-c++20-extensions',
  '-Wno-c++98-compat-pedantic',
  '-Wno-c++98-compat',
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
  'unix_extra': [],
  'windows_extra': '-DCURL_STATICLIB',
}

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

if host_system == 'windows'
  add_project_arguments(
    '-march=native',
    '-DNOMINMAX',
    '-DWIN32_LEAN_AND_MEAN',
    '-DWINRT_LEAN_AND_MEAN',
    language: ['cpp', 'objcpp'],
  )
endif

# --------------------- #
#  Include Directories  #
# --------------------- #
project_public_includes = include_directories('include', is_system: true)
third_party_includes = include_directories('third_party', is_system: true)

includes_dep = declare_dependency(include_directories: [project_public_includes, third_party_includes])

# --------------------- #
#  Dependencies Config  #
# --------------------- #
lib_deps = [includes_dep]
fs = import('fs')

if get_option('use_linked_pci_ids') == true
  add_project_arguments('-DUSE_LINKED_PCI_IDS', language: ['cpp'])
endif

if get_option('precompiled_config') == true
  add_project_arguments('-DDRAC_PRECOMPILED_CONFIG', language: ['cpp'])
  config_hpp_path = meson.project_source_root() / 'config.hpp'
  if not fs.is_file(config_hpp_path)
    error(
      'Option "precompiled_config" is enabled, but "@0@" was not found.'.format(config_hpp_path),
    )
  endif
else
  lib_deps += dependency('tomlplusplus', include_type: 'system', static: true)
endif

if get_option('enable_nowplaying') == true
  add_project_arguments('-DDRAC_ENABLE_NOWPLAYING', language: ['cpp'])
endif

if get_option('enable_caching') == true
  add_project_arguments('-DDRAC_ENABLE_CACHING', language: ['cpp'])
endif

if get_option('enable_weather') == true
  lib_deps += dependency('libcurl', include_type: 'system', static: true)
  add_project_arguments('-DDRAC_ENABLE_WEATHER', language: ['cpp'])
endif

if get_option('enable_packagecount') == true
  if host_system == 'darwin' or host_system == 'linux'
    lib_deps += dependency('SQLiteCpp')
  endif
  add_project_arguments('-DDRAC_ENABLE_PACKAGECOUNT', language: ['cpp'])
endif

platform_deps = []
if host_system == 'darwin'
  platform_deps += [
    dependency(
      'appleframeworks',
      modules: [
        'coregraphics',
        'foundation',
        'iokit',
        'mediaplayer',
        'metal',
        'systemconfiguration',
      ],
      static: true,
    ),
    cpp.find_library('iconv'),
  ]
elif host_system == 'windows'
  platform_deps += [
    cpp.find_library('dwmapi'),
    cpp.find_library('windowsapp'),
    cpp.find_library('setupapi'),
    cpp.find_library('dxgi'),
    cpp.find_library('dxguid'),
  ]

  add_project_arguments('-D_WIN32_WINNT=0x0601', language: 'cpp')
elif host_system != 'serenity' and host_system != 'haiku'
  dbus_dep = dependency('dbus-1', required: false)
  xcb_dep = dependency('xcb', required: false)
  xcb_randr_dep = dependency('xcb-randr', required: false)
  xau_dep = dependency('xau', required: false)
  xdmcp_dep = dependency('xdmcp', required: false)
  wayland_dep = dependency('wayland-client', required: false)
  pugixml_dep = dependency('pugixml', required: false)

  if host_system == 'linux' and get_option('use_pugixml') == true and pugixml_dep.found()
    platform_deps += pugixml_dep
    add_project_arguments('-DHAVE_PUGIXML', language: 'cpp')
  elif get_option('use_pugixml') == true and not pugixml_dep.found()
    error('Option "use_pugixml" is enabled, but pugixml was not found.')
  endif

  if get_option('enable_nowplaying') == true and dbus_dep.found()
    platform_deps += dbus_dep
    add_project_arguments('-DHAVE_DBUS', language: 'cpp')
  elif get_option('enable_nowplaying') == true and not dbus_dep.found()
    error(
      'Option "enable_nowplaying" is enabled, but dbus (dbus-1/libdbus) was not found.',
    )
  endif

  if get_option('use_xcb') == true
    if xcb_dep.found() and xcb_randr_dep.found() and xau_dep.found() and xdmcp_dep.found()
      platform_deps += [xcb_dep, xcb_randr_dep, xau_dep, xdmcp_dep]
      add_project_arguments('-DHAVE_XCB', language: 'cpp')
    else
      error(
        'Option "use_xcb" is enabled, but one or more of its dependencies (xcb, xcb-randr, xau, xdmcp) were not found.',
      )
    endif
  endif

  if get_option('use_wayland') == true
    if wayland_dep.found()
      platform_deps += wayland_dep
      add_project_arguments('-DHAVE_WAYLAND', language: 'cpp')
    else
      error('Option "use_wayland" is enabled, but wayland-client was not found.')
    endif
  endif
endif
lib_deps += platform_deps

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
lib_deps += ftxui_dep

glaze_dep = dependency('glaze', include_type: 'system', required: false)
if not glaze_dep.found()
  cmake = import('cmake')
  options = cmake.subproject_options()
  options.add_cmake_defines(
    {
      'glaze_ENABLE_SSL': false,
      'glaze_BUILD_EXAMPLES': false,
      'glaze_DEVELOPER_MODE': false,
    },
  )

  glaze_proj = cmake.subproject(
    'glaze',
    options: options,
  )

  glaze_dep = glaze_proj.dependency('glaze_glaze', include_type: 'system')
endif
lib_deps += glaze_dep

if (host_system == 'linux' or host_system == 'darwin') and run_command(find_program('printenv'), 'IN_NIX_SHELL', check: false).returncode() == 0
  add_project_arguments('-DDRAC_NIX_BUILD', language: ['cpp', 'objcpp'])
endif

# ---------------- #
#  Subdirectories  #
# ---------------- #
subdir('src/Lib')
subdir('src/CLI')

# -------------------- #
#  Example Executable  #
# -------------------- #
if get_option('build_examples')
  lib_deps += dependency('asio', static: true, fallback: ['asio', 'asio_dep'])
  subdir('examples')
endif

# ------- #
#  Tests  #
# ------- #
if get_option('build_tests')
  subdir('src/Lib/Tests')
endif