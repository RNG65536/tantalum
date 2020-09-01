cmake_minimum_required(VERSION 3.0)
project(imgui)

add_library(${PROJECT_NAME} STATIC
    "imconfig.h"
    "imgui.cpp"
    "imgui.h"
    "imgui_demo.cpp"
    "imgui_draw.cpp"
    "imgui_internal.h"
    "stb_rect_pack.h"
    "stb_textedit.h"
    "stb_truetype.h"
)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})