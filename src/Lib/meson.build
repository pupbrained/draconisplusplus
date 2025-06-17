# ------- #
#  Files  #
# ------- #
lib_base_sources = []

if get_option('enable_packagecount') == true
  lib_base_sources += files('Services/PackageCounting.cpp')
endif

if get_option('enable_weather') == true
  lib_base_sources += files(
    'Services/Weather/MetNoService.cpp',
    'Services/Weather/OpenMeteoService.cpp',
    'Services/Weather/OpenWeatherMapService.cpp',
    'Services/Weather/WeatherFactory.cpp',
    'Services/Weather/WeatherUtils.cpp',
  )
endif

link_args = []

if get_option('use_linked_pci_ids') == true
  link_args += meson.project_source_root() / 'pci_ids.o'
endif

platform_sources = {
  'darwin': ['OS/macOS.cpp', 'OS/macOS/Bridge.mm'],
  'dragonfly': ['OS/BSD.cpp'],
  'freebsd': ['OS/BSD.cpp'],
  'haiku': ['OS/Haiku.cpp'],
  'linux': ['OS/Linux.cpp'],
  'netbsd': ['OS/BSD.cpp'],
  'serenity': ['OS/Serenity.cpp'],
  'windows': ['OS/Windows.cpp'],
}

lib_platform_sources = files(platform_sources.get(host_system, []))
lib_all_sources = lib_base_sources + lib_platform_sources

# ----------------- #
#  Static Library   #
# ----------------- #
libdraconis = static_library(
  'draconis++',
  lib_all_sources,
  dependencies: lib_deps,
  install: false,
)

draconis_dep = declare_dependency(link_with: libdraconis, dependencies: [includes_dep], link_args: link_args)
