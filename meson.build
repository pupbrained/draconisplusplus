# ========================= #
#   Project Configuration   #
# ========================= #
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

# ========================== #
#   Compiler & Environment   #
# ========================== #
cpp = meson.get_compiler('cpp')
host_system = host_machine.system()

# Preprocessor definitions
project_string_defines = {
  'DRAC_VERSION': '"' + meson.project_version() + '"',
  'fn': 'auto',
  '_WIN32_WINNT': '0x0602',
}

project_flag_defines = {
  'DRAC_ARCH_64BIT': cpp.sizeof('void*') == 8,
  'DRAC_ARCH_AARCH64': host_machine.cpu_family() == 'aarch64',
  'DRAC_ARCH_ARM': host_machine.cpu_family() == 'arm',
  'DRAC_ARCH_X86': host_machine.cpu_family() == 'x86',
  'DRAC_ARCH_X86_64': host_machine.cpu_family() == 'x86_64',
  'DRAC_DEBUG': get_option('debug'),
}

if host_system == 'windows'
  project_flag_defines += {
    'NOMINMAX': true,
    'WIN32_LEAN_AND_MEAN': true,
    'WINRT_LEAN_AND_MEAN': true,
    'CURL_STATICLIB': true,
  }
elif (host_system == 'linux' or host_system == 'darwin') and run_command(
  find_program('printenv'),
  'IN_NIX_SHELL',
  check: false,
).returncode() == 0
  project_flag_defines += {'DRAC_NIX_BUILD': true}
endif

# Feature detection
feature_states = {}

# Define which options are features vs booleans
feature_options = [
  'nowplaying',
  'caching',
  'weather',
  'packagecount',
  'xcb',
  'wayland',
  'pugixml',
]

foreach option, define : {
  'caching': 'DRAC_ENABLE_CACHING',
  'nowplaying': 'DRAC_ENABLE_NOWPLAYING',
  'packagecount': 'DRAC_ENABLE_PACKAGECOUNT',
  'precompiled_config': 'DRAC_PRECOMPILED_CONFIG',
  'pugixml': 'DRAC_USE_PUGIXML',
  'use_linked_pci_ids': 'DRAC_USE_LINKED_PCI_IDS',
  'wayland': 'DRAC_USE_WAYLAND',
  'weather': 'DRAC_ENABLE_WEATHER',
  'xcb': 'DRAC_USE_XCB',
}
  opt_val = get_option(option)

  # Check if this is a feature option
  if option in feature_options
    enabled = opt_val.enabled()
  else
    enabled = opt_val
  endif

  if enabled
    project_flag_defines += {define: true}
  endif
  feature_states += {option: enabled}
endforeach

cpp_args = []

foreach name, value : project_string_defines
  cpp_args += '-D@0@=@1@'.format(name, value)
endforeach

foreach name, value : project_flag_defines
  cpp_args += '-D@0@=@1@'.format(name, value ? '1' : '0')
endforeach

if cpp.get_id() in ['msvc', 'clang-cl']
  cpp_args += '/std:c++latest'
  cpp_args += cpp.get_supported_arguments(
    '/Zc:__cplusplus',
    '/Zc:preprocessor',
    '/external:W0',
    '/external:anglebrackets',
  )
else
  cpp_args += '-std=c++26'
endif

# Check compiler arguments in groups
cpp_args += cpp.get_supported_arguments(
  [
    # Common flags
    '-fno-strict-enums',
    '-fvisibility=hidden',
    '-fvisibility-inlines-hidden',
    '-march=native',

    # Warning flags
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
  ],
)

add_project_arguments(cpp_args, language: 'cpp')

if host_system == 'darwin'
  add_languages('objcpp', native: false)
  add_project_arguments(cpp_args, language: 'objcpp')
endif

# =================== #
#   Include Folders   #
# =================== #
project_public_includes = include_directories('include', is_system: true)
third_party_includes = include_directories('third_party', is_system: true)
includes_dep = declare_dependency(include_directories: [project_public_includes, third_party_includes])

# ================ #
#   Dependencies   #
# ================ #
fs = import('fs')
lib_deps = [includes_dep]

# Core dependencies
lib_deps += dependency(
  'magic_enum',
  fallback: ['magic_enum', 'magic_enum_dep'],
  include_type: 'system',
  static: true,
)

# Precompiled configuration
if get_option('precompiled_config') == true
  config_hpp_path = meson.project_source_root() / 'config.hpp'

  if not fs.is_file(config_hpp_path)
    error(
      'Option "precompiled_config" is enabled, but "@0@" was not found.'.format(config_hpp_path),
    )
  endif
else
  lib_deps += dependency('tomlplusplus', include_type: 'system', static: true)
endif

if feature_states['weather']
  lib_deps += dependency('libcurl', include_type: 'system', static: true)
endif

if feature_states['packagecount'] and host_system in ['darwin', 'linux']
  lib_deps += dependency('SQLiteCpp')
endif

# Platform-specific dependencies
if host_system == 'darwin'
  lib_deps += dependency(
    'appleframeworks',
    modules: [
      'coregraphics',
      'foundation',
      'iokit',
      feature_states['nowplaying'] ? 'mediaplayer' : '',
      'metal',
      'systemconfiguration',
    ],
  )
  lib_deps += cpp.find_library('iconv')
elif host_system == 'windows'
  lib_deps += [
    cpp.find_library('dwmapi'),
    cpp.find_library('windowsapp'),
    cpp.find_library('setupapi'),
    cpp.find_library('dxgi'),
    cpp.find_library('dxguid'),
  ]
elif host_system not in ['serenity', 'haiku']
  lib_deps += dependency('dbus-1', required: feature_states['nowplaying'])

  lib_deps += dependency('xau', required: get_option('xcb'))
  lib_deps += dependency('xcb', required: get_option('xcb'))
  lib_deps += dependency('xcb-randr', required: get_option('xcb'))
  lib_deps += dependency('xdmcp', required: get_option('xcb'))

  lib_deps += dependency('pugixml', required: get_option('pugixml'))

  lib_deps += dependency('wayland-client', required: get_option('wayland'))
endif

# Glaze (JSON/BEVE serializer/deserializer)
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
  glaze_proj = cmake.subproject('glaze', options: options)
  glaze_dep = glaze_proj.dependency('glaze_glaze', include_type: 'system')
endif

lib_deps += glaze_dep

# =============== #
#   Subprojects   #
# =============== #
subdir('src/Lib')
subdir('src/CLI')

if get_option('build_examples')
  lib_deps += dependency('asio', static: true, fallback: ['asio', 'asio_dep'])
  subdir('examples')
endif

if get_option('build_tests')
  subdir('src/Lib/Tests')
endif

# =========================== #
#   Configuration Summaries   #
# =========================== #
summary(
  {
    '64-bit': project_flag_defines['DRAC_ARCH_64BIT'],
    'AArch64': project_flag_defines['DRAC_ARCH_AARCH64'],
    'ARM': project_flag_defines['DRAC_ARCH_ARM'],
    'X86': project_flag_defines['DRAC_ARCH_X86'],
    'X86_64': project_flag_defines['DRAC_ARCH_X86_64'],
    'Debug': project_flag_defines['DRAC_DEBUG'],
  },
  section: 'Definitions',
  bool_yn: true,
)

summary(
  {
    'Weather support': feature_states['weather'],
    'Package counting': feature_states['packagecount'],
    'Now Playing': feature_states['nowplaying'],
    'Caching': feature_states['caching'],
    'Precompiled config': get_option('precompiled_config'),
  },
  section: 'Features',
  bool_yn: true,
)

if host_system != 'windows' and host_system != 'darwin'
  summary(
    {
      'XCB (X11)': feature_states.get('xcb', false),
      'Wayland': feature_states.get('wayland', false),
      'pugixml': feature_states.get('pugixml', false),
      'Use Linked pci.ids': feature_states.get('use_linked_pci_ids', false),
    },
    section: 'Platform Support',
    bool_yn: true,
  )
endif

summary(
  {
    'Build examples': get_option('build_examples'),
    'Build tests': get_option('build_tests'),
    'Build Switch example': get_option('build_switch_example'),
  },
  section: 'Build Options',
  bool_yn: true,
)

summary(
  {
    'C++ Compiler': cpp.get_id(),
    'Host system': host_system,
    'Build type': get_option('buildtype'),
    'Install prefix': get_option('prefix'),
  },
  section: 'System',
)