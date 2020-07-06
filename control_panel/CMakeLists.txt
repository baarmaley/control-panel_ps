cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

project(control_panel VERSION 0.1 LANGUAGES CXX)

find_package(Boost)


file(GLOB_RECURSE TARGET_SOURCES
        src/*.cpp)

file(GLOB_RECURSE TARGET_PRIVATE_HEADERS
        src/*.hpp
        src/*.h)

add_executable(control_panel
        ${TARGET_SOURCES}
        ${TARGET_PRIVATE_HEADERS})

target_include_directories(control_panel
        PRIVATE src)

target_link_libraries(control_panel
        PRIVATE
        tsvetkov::protocol
        Boost::boost)