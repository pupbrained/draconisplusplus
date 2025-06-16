# ------- #
#  Files  #
# ------- #
main_app_sources = files('Config/Config.cpp', 'UI/UI.cpp', 'main.cpp')

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
  dependencies: [draconis_dep] + lib_deps,
  link_args: link_args,
  objc_args: objc_args,
  install: true,
)
