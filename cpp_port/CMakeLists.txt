cmake_minimum_required(VERSION 3.12)

project(tantalum_port)

set(CMAKE_CXX_STANDARD 17)

add_definitions("/Zi")
add_link_options("/DEBUG")

add_subdirectory(deps)

add_subdirectory(tantalum)
