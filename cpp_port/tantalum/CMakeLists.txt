project(tantalum_port)


file(GLOB srcs "src/*.*")
source_group("srcs\\" FILES ${srcs})

file(GLOB shaders "shaders/*.glsl")
source_group("shaders\\" FILES ${shaders})

add_executable(tantalum_port ${srcs} ${shaders})

target_link_libraries(tantalum_port deps)

target_compile_options(tantalum_port PRIVATE "/wd4251;/wd4592;/wd4127")

target_compile_definitions(tantalum_port PRIVATE "PROJECT_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/\"")
