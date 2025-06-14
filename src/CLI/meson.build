# ------- #
#  Files  #
# ------- #
main_app_sources = files('UI/UI.cpp', 'main.cpp')

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
  dependencies: [draconis_dep] + lib_deps, # Add lib_deps here
  link_args: link_args,
  objc_args: objc_args,
  install: true,
)

# ----------------- #
#  Testing Support  #
# ----------------- #
gtest_dep = dependency('gtest', fallback: ['gtest', 'gtest_dep'], main: false, required: false)

if gtest_dep.found()
  test_common_args = ['--gtest_color=yes', '--gtest_print_time=1']
  test_common_dependencies = [draconis_dep, gtest_dep] + lib_deps # Add lib_deps here

  individual_test_files_paths = [
    'tests/ConfigTest.cpp',
    'tests/CoreTypesTest.cpp',
    'tests/LoggingUtilsTest.cpp',
  ]

  if get_option('enable_weather')
    individual_test_files_paths += ['tests/WeatherServiceTest.cpp']
  endif

  fs = import('fs')
  foreach test_fpath : individual_test_files_paths
    test_name = fs.stem(test_fpath)

    current_test_exe = executable(
      test_name,
      files(meson.project_source_root() / test_fpath),
      dependencies: test_common_dependencies,
      link_args: link_args,
      objc_args: objc_args,
    )

    test(
      test_name,
      current_test_exe,
      args: test_common_args,
      protocol: 'gtest',
    )
  endforeach
endif