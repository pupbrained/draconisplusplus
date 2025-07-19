# ----------------- #
#  Test Files      #
# ----------------- #
test_sources = {
  'core': files('CacheManagerTest.cpp', 'CoreTypesTest.cpp', 'LoggingUtilsTest.cpp'),
  'packages': files('PackageCountingIntegrationTest.cpp', 'PackageCountingMockTest.cpp'),
  'weather': files('WeatherServiceTest.cpp'),
}

# ----------------- #
#  Test Executable  #
# ----------------- #
gtest_dep = dependency('gtest', fallback: ['gtest', 'gtest_dep'], main: false, required: false)
gmock_dep = dependency('gmock', fallback: ['gtest', 'gmock_dep'], required: false)

if gtest_dep.found() and gmock_dep.found()
  test_common_args = ['--gtest_color=yes', '--gtest_print_time=1']
  test_common_dependencies = [draconis_dep, gtest_dep, gmock_dep] + lib_deps
  test_inc = include_directories('..')

  fs = import('fs')

  # Always build core tests
  foreach test_file : test_sources['core']
    test_name = fs.stem(test_file)

    test_exe = executable(
      test_name,
      test_file,
      dependencies: test_common_dependencies,
      include_directories: test_inc,
    )

    test(
      test_name,
      test_exe,
      args: test_common_args,
      protocol: 'gtest',
      suite: 'core',
      timeout: 30,
      is_parallel: true,
    )
  endforeach

  # Conditionally build package tests
  if get_option('packagecount').enabled()
    foreach test_file : test_sources['packages']
      test_name = fs.stem(test_file)

      test_exe = executable(
        test_name,
        test_file,
        dependencies: test_common_dependencies,
        include_directories: test_inc,
      )

      test(
        test_name,
        test_exe,
        args: test_common_args,
        protocol: 'gtest',
        suite: 'packages',
        timeout: 30,
        is_parallel: true,
      )
    endforeach
  endif

  # Conditionally build weather tests
  if get_option('weather').enabled()
    foreach test_file : test_sources['weather']
      test_name = fs.stem(test_file)

      test_exe = executable(
        test_name,
        test_file,
        dependencies: test_common_dependencies,
        include_directories: test_inc,
      )

      test(
        test_name,
        test_exe,
        args: test_common_args,
        protocol: 'gtest',
        suite: 'weather',
        timeout: 30,
        is_parallel: true,
      )
    endforeach
  endif
endif
