project(
  'draconis++',
  'cpp',
  version: '0.1.0',
  default_options: [
    'default_library=static',
    'warning_level=everything',
    'buildtype=debugoptimized',
    'cpp_args=-fvisibility=hidden',
  ],
)

cpp = meson.get_compiler('cpp')

if host_machine.system() == 'darwin'
  add_languages('objcpp')
  objcpp = meson.get_compiler('objcpp')
  add_project_arguments(
    objcpp.get_supported_arguments(
      [
        '-std=c++2b',
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
      ],
    ),
    language: 'objcpp',
  )
endif

common_cpp_args = [
  '-std=c++26',
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
  '-fno-strict-enums',
  '-nostdlib++',
]

if host_machine.system() == 'windows'
  common_cpp_args += ['-DCURL_STATICLIB']
endif

add_project_arguments(cpp.get_supported_arguments(common_cpp_args), language: 'cpp')

source_file_names = ['src/main.cpp', 'src/config/config.cpp', 'src/config/weather.cpp']

if host_machine.system() == 'linux'
  source_file_names += ['src/os/linux.cpp', 'src/os/linux/issetugid_stub.cpp']
elif host_machine.system() == 'freebsd'
  source_file_names += ['src/os/freebsd.cpp']
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
  dependency('fmt', include_type: 'system', static: true),
  dependency('openssl', include_type: 'system', static: true),
  dependency('libcurl', include_type: 'system', static: true),
  dependency('tomlplusplus', include_type: 'system', static: true),
]

if host_machine.system() == 'darwin'
  deps += dependency(
    'appleframeworks',
    modules: ['foundation', 'mediaplayer', 'systemconfiguration'],
    static: true,
  )
elif host_machine.system() == 'windows'
  deps += [
    cpp.find_library('dwmapi'),
    cpp.find_library('windowsapp'),
  ]
elif host_machine.system() == 'linux' or host_machine.system() == 'freebsd'
  deps += [
    dependency('SQLiteCpp'),
    dependency('sdbus-c++'),
    dependency('x11'),
    dependency('xcb'),
    dependency('xau'),
    dependency('xdmcp'),
    dependency('wayland-client'),
    dependency('dbus-1'),
  ]
endif

cmake = import('cmake')

ftxui_dep = dependency(
  'ftxui',
  modules: ['ftxui::screen', 'ftxui::dom', 'ftxui::component'],
  include_type: 'system',
  static: true,
  required: false,
)

if not ftxui_dep.found()
  ftxui_dom = dependency('ftxui-dom', fallback: ['ftxui', 'dom_dep'])
  ftxui_screen = dependency('ftxui-screen', fallback: ['ftxui', 'screen_dep'])
  ftxui_component = dependency('ftxui-component', fallback: ['ftxui', 'component_dep'])
  ftxui_dep = declare_dependency(dependencies: [ftxui_dom, ftxui_screen, ftxui_component])
endif

deps += ftxui_dep

cmake_opts = cmake.subproject_options()

cmake_opts.add_cmake_defines({
  'CMAKE_CXX_FLAGS': '-Wno-everything',
})

cmake_opts.append_compile_args('cpp', '-Wno-everything')

reflectcpp_proj = cmake.subproject('reflectcpp', options: cmake_opts)
reflectcpp_dep = reflectcpp_proj.dependency('reflectcpp', include_type: 'system')
deps += reflectcpp_dep

objc_args = []
link_args = ['-static-libgcc', '-static-libstdc++', '-static']

if host_machine.system() == 'darwin'
  objc_args += ['-fobjc-arc']
endif

executable(
  'draconis++',
  sources,
  objc_args: objc_args,
  link_args: link_args,
  dependencies: deps,
)