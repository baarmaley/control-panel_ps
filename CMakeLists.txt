cmake_minimum_required(VERSION 3.16)
project(SmartPower)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(control_panel)
add_subdirectory(my_smart_power)
add_subdirectory(tests)
add_subdirectory(portable_concurrency)