cmake_minimum_required(VERSION 3.5)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

project(draconis++ C CXX)

add_executable(${PROJECT_NAME} src/main.cpp)

find_package(PkgConfig REQUIRED)

pkg_check_modules(${PROJECT_NAME} REQUIRED IMPORTED_TARGET fmt playerctl)

target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::${PROJECT_NAME})
