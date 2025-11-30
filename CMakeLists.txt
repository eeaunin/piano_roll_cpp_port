cmake_minimum_required(VERSION 3.16)
project(piano_roll_cpp_port LANGUAGES CXX)

option(PIANO_ROLL_USE_IMGUI "Enable Dear ImGui rendering in piano roll" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(piano_roll STATIC
    src/draggable_rectangle.cpp
    src/coordinate_system.cpp
    src/cc_lane_renderer.cpp
    src/custom_scrollbar.cpp
    src/demo.cpp
    src/grid_snap.cpp
    src/interaction.cpp
    src/keyboard.cpp
    src/note_manager.cpp
    src/loop_marker_rectangle.cpp
    src/overlay.cpp
    src/serialization.cpp
    src/renderer.cpp
    src/widget.cpp
)

target_include_directories(piano_roll
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

if(PIANO_ROLL_USE_IMGUI)
    target_compile_definitions(piano_roll PUBLIC PIANO_ROLL_USE_IMGUI)
    # ImGui include directories and link libraries are intentionally left
    # for the parent project to supply, since integration varies widely.
endif()
