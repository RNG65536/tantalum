# glfw
# add_subdirectory(glfw3)
find_package(glfw3 REQUIRED)

# glad
# add_library(glad STATIC
#     glfw3/deps/glad/gl.h
#     glfw3/deps/glad_gl.c
# )

# target_include_directories(glad PUBLIC glfw3/deps)

# glbinding
find_package(glbinding REQUIRED)

# globjects
find_package(globjects REQUIRED)

# lodepng
add_subdirectory(lodepng)

# imgui
add_subdirectory(imgui)

# deps
add_library(deps INTERFACE)

# find_package(OpenGL REQUIRED)

target_link_libraries(deps
INTERFACE
    glfw
    # OpenGL::GL
    # OpenGL::GLU
    # glad
    glbinding::glbinding
    glbinding::glbinding-aux
    globjects::globjects
    lodepng
    imgui
)

target_compile_definitions(deps INTERFACE "GLFW_INCLUDE_NONE=1")
