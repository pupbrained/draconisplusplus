cmake_minimum_required(VERSION 3.28.1)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
set(CMAKE_CXX_SCAN_FOR_MODULES 1)

project(draconis++)

set(sources)

list(APPEND sources src/main.cpp)

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(MACOSX TRUE)
endif ()

if (MACOSX)
    list(APPEND sources src/os/macos.cpp)
elseif (LINUX)
    list(APPEND sources src/os/linux.cpp)
endif ()

add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PUBLIC src/main.cpp)
target_sources(${PROJECT_NAME} PUBLIC FILE_SET all_my_modules TYPE CXX_MODULES FILES src/os/linux.cpp)

find_package(PkgConfig REQUIRED)
find_package(cpr REQUIRED)
find_package(Boost REQUIRED)

if (MACOSX)
    pkg_check_modules(${PROJECT_NAME} REQUIRED IMPORTED_TARGET fmt tomlplusplus)
else ()
    pkg_check_modules(${PROJECT_NAME} REQUIRED IMPORTED_TARGET fmt playerctl tomlplusplus)
endif ()

target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::${PROJECT_NAME} cpr::cpr Boost::boost)

