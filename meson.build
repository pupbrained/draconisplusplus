project(
  'draconis++', 'cpp',
  version : '0.1.0',
  default_options : [
    'cpp_std=c++20',
    'default_library=static',
    'warning_level=everything',
    'buildtype=debugoptimized'
  ]
)

cpp = meson.get_compiler('cpp')

add_project_arguments(
  cpp.get_supported_arguments(
    [
      '-DCURL_STATICLIB',
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
    ]
  ),
  language : 'cpp'
)

source_file_names = [
  'src/main.cpp',
  'src/config/config.cpp',
  'src/config/weather.cpp',
  'src/os/windows.cpp'
]

sources = []
foreach file : source_file_names
  sources += files(file)
endforeach

windows_sdk_lib_dir = 'C:/Program Files (x86)/Windows Kits/10/Lib/10.0.22621.0/um/x64'
link_args = ['-L' + windows_sdk_lib_dir, '-lwindowsapp', '-static']

deps = []

deps += dependency('fmt', static : true)
deps += dependency('libcurl', static : true)
deps += dependency('tomlplusplus', static : true)
deps += dependency('yyjson', static : true)

executable(
  'draconis++',
  sources,
  dependencies : deps,
  link_args : link_args
)