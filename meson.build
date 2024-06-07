project(
  'draconis++', 'cpp',
  version: '0.1.0',
  default_options: [
    'objc_std=c++20',
    'objcpp_std=c++20',
    'cpp_std=c++20',
    'default_library=static',
    'warning_level=everything',
    'buildtype=debugoptimized',
  ]
)

clangtidy = find_program('clang-tidy', required: false)

cpp = meson.get_compiler('cpp')

if host_machine.system() == 'darwin'
  add_languages('objcpp')

  objcpp = meson.get_compiler('objcpp')

  add_project_arguments(
    objcpp.get_supported_arguments([
      '-Wno-c++20-compat',
      '-Wno-c++98-compat',
      '-Wno-c++98-compat-pedantic',
      '-Wno-disabled-macro-expansion',
      '-Wno-missing-prototypes',
      '-Wno-padded',
      '-Wno-pre-c++20-compat-pedantic',
      '-Wno-switch-default',
      '-Wunused-function',
    ]),
    language: 'objcpp'
  )
endif

add_project_arguments(
  cpp.get_supported_arguments([
    '-Wno-c++20-compat',
    '-Wno-c++98-compat',
    '-Wno-c++98-compat-pedantic',
    '-Wno-disabled-macro-expansion',
    '-Wno-missing-prototypes',
    '-Wno-padded',
    '-Wno-pre-c++20-compat-pedantic',
    '-Wno-switch-default',
    '-Wunused-function',
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
  source_file_names += [
    'src/os/macos.cpp',
    'src/os/macos/NowPlayingBridge.h',
    'src/os/macos/NowPlayingBridge.mm'
  ]
endif

sources = []

foreach file : source_file_names
  sources += files(file)
endforeach

deps = []

deps += dependency('fmt')
deps += dependency('libcurl')
deps += dependency('tomlplusplus')
deps += dependency('yyjson')

if host_machine.system() == 'darwin'
  deps += dependency('Foundation')
endif

if host_machine.system() == 'linux'
  deps += dependency('sdbus-c++')
endif

incdir = include_directories(
  'include',
  is_system: true # Ignores warnings from include dir
)

objc_args = []
link_args = []

if host_machine.system() == 'darwin'
  objc_args += ['-fobjc-arc']
  link_args += ['-framework', 'Foundation', '-framework', 'MediaPlayer']
endif

executable(
  'draconis++',
  sources,
  objc_args: objc_args,
  link_args: link_args,
  dependencies: deps,
  include_directories: incdir,
)
