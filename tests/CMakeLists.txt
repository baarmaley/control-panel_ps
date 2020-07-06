cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

project(external_sort_tests VERSION 0.1 LANGUAGES CXX)

find_package(Catch2 CONFIG REQUIRED)
find_package(Boost)



file(GLOB_RECURSE TARGET_SOURCES
     src/*.cpp)
    
file(GLOB_RECURSE TARGET_PRIVATE_HEADERS
    src/*.hpp
    src/*.h)
    
add_executable(protocol_tests
    ${TARGET_SOURCES}
    ${TARGET_PRIVATE_HEADERS})

target_include_directories(protocol_tests
    PRIVATE src)

target_link_libraries(protocol_tests
        PRIVATE
        tsvetkov::protocol
        Boost::boost
        Catch2::Catch2)