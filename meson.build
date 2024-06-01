project(
  'draconis++', 'cpp',
  default_options: [
    'cpp_std=c++26',
    'default_library=static',
    'warning_level=everything'
  ]
)

clangtidy = find_program('clang-tidy', required: false)

cc = meson.get_compiler('cpp')

add_project_arguments(
  cc.get_supported_arguments([
    '-Wno-c++98-compat',
    '-Wno-c++98-compat-pedantic',
    '-Wno-padded',
    '-Wno-pre-c++20-compat-pedantic',
    '-Wno-switch-default',
    '-Wno-missing-prototypes'
  ]),
  language: 'cpp'
)

source_file_names = [
  'src/main.cpp',
  'src/os/os.h',
  'src/config/config.cpp',
  'src/config/config.h'
]

if host_machine.system() == 'linux'
  source_file_names += ['src/os/linux.cpp']
endif

if host_machine.system() == 'darwin'
  source_file_names += ['src/os/macos.cpp']
endif

sources = []

foreach file : source_file_names
  sources += files(file)
endforeach

deps = []

deps += cc.find_library('cpr')
deps += cc.find_library('curl')
deps += cc.find_library('tomlplusplus')
deps += dependency('boost')
deps += dependency('fmt')

if host_machine.system() == 'linux'
  deps += dependency('playerctl')
endif

incdir = include_directories(
  'include',
  is_system: true # Ignores warnings from include dir
)

executable(
  'draconis++',
  sources,
  dependencies: deps,
  include_directories: incdir
)
