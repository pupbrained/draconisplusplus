project(
  'draconis++', 'cpp',
  version: '0.1.0',
  default_options: [
    'cpp_std=c++23',
    'default_library=static',
    'warning_level=everything',
    'buildtype=debugoptimized'
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
      '-Wno-c++20-extensions',
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

common_cpp_args = [
  '-Wno-c++20-compat',
  '-Wno-c++20-extensions',
  '-Wno-c++98-compat',
  '-Wno-c++98-compat-pedantic',
  '-Wno-disabled-macro-expansion',
  '-Wno-missing-prototypes',
  '-Wno-padded',
  '-Wno-pre-c++20-compat-pedantic',
  '-Wno-switch-default',
  '-Wunused-function'
]

if host_machine.system() == 'windows'
  common_cpp_args += ['-DCURL_STATICLIB']
endif

add_project_arguments(
  cpp.get_supported_arguments(common_cpp_args),
  language: 'cpp'
)

source_file_names = [
  'src/main.cpp',
  'src/config/config.cpp',
  'src/config/weather.cpp'
]

if host_machine.system() == 'linux'
  source_file_names += ['src/os/linux.cpp']
elif host_machine.system() == 'darwin'
  source_file_names += [
    'src/os/macos.cpp',
    'src/os/macos/bridge.mm',
  ]
elif host_machine.system() == 'windows'
  source_file_names += ['src/os/windows.cpp']
endif

sources = []

foreach file : source_file_names
  sources += files(file)
endforeach

deps = [
  dependency('fmt', static: host_machine.system() != 'darwin'),
  dependency('libcurl', static: host_machine.system() != 'darwin'),
  dependency('tomlplusplus', static: host_machine.system() != 'darwin'),
  dependency('yyjson', static: host_machine.system() != 'darwin')
]

if host_machine.system() == 'darwin'
  deps += dependency('Foundation')
  deps += dependency('MediaPlayer')
elif host_machine.system() == 'linux'
  deps += dependency('sdbus-c++')
endif

objc_args = []
link_args = []

if host_machine.system() == 'darwin'
  objc_args += ['-fobjc-arc']
  link_args += ['-framework', 'Foundation', '-framework', 'MediaPlayer']
elif host_machine.system() == 'windows'
  windows_sdk_lib_dir = 'C:/Program Files (x86)/Windows Kits/10/Lib/10.0.22621.0/um/x64'
  link_args += ['-L' + windows_sdk_lib_dir, '-lwindowsapp', '-static']
endif

executable(
  'draconis++',
  sources,
  objc_args: objc_args,
  link_args: link_args,
  dependencies: deps
)

