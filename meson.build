project(
  'draconis++', 'cpp',
  version: '0.1.0',
  default_options: [
    'cpp_std=c++26',
    'default_library=static',
    'warning_level=everything',
    'buildtype=debugoptimized'
  ]
)

clangtidy = find_program('clang-tidy', required: false)

cpp = meson.get_compiler('cpp')

add_project_arguments(
  cpp.get_supported_arguments([
    '-Wno-c++20-compat',
    '-Wno-c++98-compat',
    '-Wno-c++98-compat-pedantic',
    '-Wno-missing-prototypes',
    '-Wno-padded',
    '-Wno-pre-c++20-compat-pedantic',
    '-Wno-switch-default',
    '-Wunused-function'
  ]),
  language: 'cpp'
)

source_file_names = [
  'src/main.cpp',
  'src/os/os.h',
  'src/config/config.cpp',
  'src/config/config.h',
  'src/config/weather.cpp'
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

quill = subproject('quill')

deps = []

deps += cpp.find_library('cpr')
deps += cpp.find_library('curl')
deps += cpp.find_library('tomlplusplus')
deps += dependency('boost', static: true)
deps += dependency('fmt')
deps += quill.get_variable('quill_dep')

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
