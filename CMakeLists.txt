cmake_minimum_required(VERSION 3.5)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

project(draconis++ C CXX)

set(sources )

list(APPEND sources src/main.cpp)

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set(MACOSX TRUE)
endif ()

if (MACOSX)
  list(APPEND sources src/os/macos.cpp)
elseif (LINUX)
  list(APPEND sources src/os/linux.cpp)
endif ()

add_executable(${PROJECT_NAME} ${sources})

find_package(PkgConfig REQUIRED)
find_package(cpr REQUIRED)
find_package(Boost REQUIRED)

if (MACOSX)
  pkg_check_modules(${PROJECT_NAME} REQUIRED IMPORTED_TARGET fmt tomlplusplus)
else ()
  pkg_check_modules(${PROJECT_NAME} REQUIRED IMPORTED_TARGET fmt playerctl tomlplusplus)
endif ()

target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::${PROJECT_NAME} cpr::cpr Boost::boost)
