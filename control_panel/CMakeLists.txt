cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

project(control_panel VERSION 0.1 LANGUAGES CXX)

find_package(Boost)
find_package(cxxopts CONFIG REQUIRED)
find_package(Threads REQUIRED)


file(GLOB_RECURSE TARGET_SOURCES
        src/*.cpp)

list(REMOVE_ITEM TARGET_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)

file(GLOB_RECURSE TARGET_PUBLIC_HEADERS
        src/*.hpp
        src/*.h)

message("TARGET_SOURCES = ${TARGET_SOURCES} TARGET_PUBLIC_HEADERS = ${TARGET_PUBLIC_HEADERS}")

add_library(control_panel_library ${TARGET_PUBLIC_HEADERS} ${TARGET_SOURCES})
set_target_properties(control_panel_library PROPERTIES LINKER_LANGUAGE CXX)

add_library(tsvetkov::control_panel_library ALIAS control_panel_library)

add_executable(control_panel src/main.cpp)

target_include_directories(control_panel
        PRIVATE src)

target_include_directories(control_panel_library
        PUBLIC src)

target_link_libraries(control_panel_library
        PUBLIC
        tsvetkov::protocol
        Boost::boost
        Threads::Threads)

target_link_libraries(control_panel
        PRIVATE
        tsvetkov::control_panel_library
        cxxopts::cxxopts)