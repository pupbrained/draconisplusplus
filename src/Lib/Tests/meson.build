# ----------------- #
#  Test Files      #
# ----------------- #
test_sources = files('CoreTypesTest.cpp', 'LoggingUtilsTest.cpp', 'CacheManagerTest.cpp')

if get_option('enable_packagecount')
  test_sources += files('PackageCountingIntegrationTest.cpp', 'PackageCountingMockTest.cpp')
endif

if get_option('enable_weather')
  test_sources += files('WeatherServiceTest.cpp')
endif

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
  foreach test_file : test_sources
    test_name = fs.stem(test_file)

    test_exe = executable(
      test_name,
      test_file,
      dependencies: test_common_dependencies,
      include_directories: test_inc,
    )

    test(test_name, test_exe, args: test_common_args, protocol: 'gtest')
  endforeach
endif