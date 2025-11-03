cmake_minimum_required(VERSION 3.10)
project(melvin_visualizer VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(PkgConfig REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)

# ImGui sources (assuming installed or vendored)
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/imgui" CACHE PATH "Path to ImGui")

if(EXISTS "${IMGUI_DIR}")
    set(IMGUI_SOURCES
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
        ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    )
    
    add_executable(melvin_visualizer
        melvin_visualizer.cpp
        ${IMGUI_SOURCES}
    )
    
    target_include_directories(melvin_visualizer PRIVATE
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
    )
else()
    message(STATUS "ImGui not found, will use system installation")
    add_executable(melvin_visualizer melvin_visualizer.cpp)
endif()

# Link libraries
target_link_libraries(melvin_visualizer
    OpenGL::GL
    glfw
    ${CMAKE_DL_LIBS}
)

# Install target
install(TARGETS melvin_visualizer DESTINATION /usr/local/bin)

