# ------- #
#  Files  #
# ------- #
main_app_sources = files('Config/Config.cpp', 'Core/SystemInfo.cpp', 'UI/UI.cpp', 'main.cpp')

# FTXUI (TUI library)
ftxui_components = ['ftxui::screen', 'ftxui::dom', 'ftxui::component']
ftxui_dep = dependency(
  'ftxui',
  modules: ftxui_components,
  include_type: 'system',
  static: true,
  required: false,
)

if not ftxui_dep.found()
  ftxui_dep = declare_dependency(
    dependencies: [
      dependency('ftxui-dom', fallback: ['ftxui', 'dom_dep']),
      dependency('ftxui-screen', fallback: ['ftxui', 'screen_dep']),
      dependency('ftxui-component', fallback: ['ftxui', 'component_dep']),
    ],
  )
endif

# ------------------------- #
#  Link/ObjC Configuration  #
# ------------------------- #
link_args = []
objc_args = []

if host_system == 'darwin'
  objc_args += ['-fobjc-arc']
elif host_system == 'linux'
  if get_option('build_for_musl') == true
    link_args += ['-static']
  else
    link_args += ['-static-libgcc', '-static-libstdc++']
  endif
elif host_system == 'haiku'
  link_args += ['-lpackage', '-lbe']
endif

# ------------------- #
#  Executable Target  #
# ------------------- #
executable(
  'draconis++',
  main_app_sources,
  dependencies: [draconis_dep, ftxui_dep] + lib_deps,
  link_args: link_args,
  objc_args: objc_args,
  install: true,
)