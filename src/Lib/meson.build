# ------- #
#  Files  #
# ------- #

# Structured source organization
lib_sources = {
  'base' : files(),
  'packages' : files('Services/Packages.cpp'),
  'weather' : files(
    'Services/Weather/MetNoService.cpp',
    'Services/Weather/OpenMeteoService.cpp',
    'Services/Weather/OpenWeatherMapService.cpp',
    'Services/Weather/WeatherFactory.cpp',
    'Services/Weather/WeatherUtils.cpp',
  ),
}

# Platform-specific sources
platform_sources = {
  'darwin' : files('OS/macOS.cpp', 'OS/macOS/Bridge.mm'),
  'dragonfly' : files('OS/BSD.cpp'),
  'freebsd' : files('OS/BSD.cpp'),
  'haiku' : files('OS/Haiku.cpp'),
  'linux' : files('OS/Linux.cpp'),
  'netbsd' : files('OS/BSD.cpp'),
  'serenity' : files('OS/Serenity.cpp'),
  'windows' : files('OS/Windows.cpp'),
}

# Build final source list
lib_all_sources = lib_sources['base']

if get_option('packagecount').enabled()
  lib_all_sources += lib_sources['packages']
endif

if get_option('weather').enabled()
  lib_all_sources += lib_sources['weather']
endif

# Add platform sources
lib_all_sources += platform_sources.get(host_system, files())

# Link arguments
link_args = []

if get_option('use_linked_pci_ids') == true
  link_args += meson.project_source_root() / 'pci_ids.o'
endif

# ----------------- #
#  Static Library   #
# ----------------- #
libdrac = static_library(
  'drac++',
  lib_all_sources,
  dependencies : lib_deps,
  install : false,
)

draconis_dep = declare_dependency(
  link_with : libdrac,
  dependencies : [includes_dep],
  link_args : link_args,
)
