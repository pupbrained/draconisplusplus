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
    modules : ['foundation', 'mediaplayer', 'systemconfiguration'],
    static: true,
  )
elif host_machine.system() == 'linux' or host_machine.system() == 'freebsd'
  deps += dependency('SQLiteCpp')
  deps += dependency('sdbus-c++')
  deps += dependency('x11')
  deps += dependency('xcb')
  deps += dependency('xau')
  deps += dependency('xdmcp')
  deps += dependency('wayland-client')
endif

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

cmake = import('cmake')

reflectcpp_proj = cmake.subproject('reflectcpp')
reflectcpp_dep = reflectcpp_proj.dependency('reflectcpp')
deps += reflectcpp_dep

objc_args = []
link_args = []

if host_machine.system() == 'darwin'
  objc_args += ['-fobjc-arc']
elif host_machine.system() == 'linux'
  link_args += ['-static-libgcc', '-static-libstdc++']
elif host_machine.system() == 'windows'
  windows_sdk_lib_dir = 'C:/Program Files (x86)/Windows Kits/10/Lib/10.0.22621.0/um/x64'
  link_args += ['-L' + windows_sdk_lib_dir, '-lwindowsapp', '-ldwmapi', '-static']
endif

executable(
  'draconis++',
  sources,
  objc_args: objc_args,
  link_args: link_args,
  dependencies: deps,
)
