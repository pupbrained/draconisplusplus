project(
  'draconis++', 'cpp',
  default_options: [
    'cpp_std=c++26',
    'default_library=static',
    'buildtype=debugoptimized',
    'warning_level=everything',
  ]
)

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

source_file_names = ['src/main.cpp', 'src/os/os.h']

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

boost_dep = dependency('boost')
cpr_dep = cc.find_library('cpr')
curl_dep = cc.find_library('curl')
fmt_dep = dependency('fmt')
tomlplusplus_dep = cc.find_library('tomlplusplus')
playerctl_dep = dependency('playerctl')

executable(
  'draconis++',
  sources,
  dependencies: [
    boost_dep,
    cpr_dep,
    curl_dep,
    fmt_dep,
    tomlplusplus_dep,
    playerctl_dep
  ]
)
