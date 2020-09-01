project(lodepng)

set(srcs
lodepng.h
lodepng.cpp
)

add_library(lodepng STATIC ${srcs})

target_include_directories(lodepng PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})