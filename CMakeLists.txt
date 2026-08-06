cmake_minimum_required(VERSION 3.20)
project(Ariadnis C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Keep the editor together for every configuration.
set(ARIADNIS_BINARY_DIR "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${ARIADNIS_BINARY_DIR}/$<CONFIG>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${ARIADNIS_BINARY_DIR}/$<CONFIG>")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/$<CONFIG>")

include(FetchContent)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
)

FetchContent_Declare(
    nativefiledialog
    GIT_REPOSITORY https://github.com/btzy/nativefiledialog-extended
    GIT_TAG v1.2.1
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL
    SYSTEM
)

FetchContent_MakeAvailable(glfw glm nativefiledialog)

set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/imgui")

add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/imgui_impl_opengl3.cpp
)

target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(imgui PUBLIC
    glfw
    glm::glm
    opengl32
)

add_executable(Ariadnis
    src/main.cpp
    src/glad.c

    src/core/camera/camera.cpp
    src/core/camera/camera.h

    src/core/terrain/terrain.cpp
    src/core/terrain/terrain.h

    src/core/render/render-modes.h

    src/core/history/command.h
    src/core/history/history_manager.h

    src/ui/navigation/top_bar.h
    src/ui/navigation/tool_bar.h

    src/tools/brush.h
    src/tools/raise_lower_brush/raise_lower_brush.cpp
    src/tools/raise_lower_brush/raise_lower_brush.h
    src/tools/smooth_brush/smooth_brush.cpp
    src/tools/smooth_brush/smooth_brush.h
    src/tools/paint_brush/paint_brush.h

    src/core/paint/paint_layer.h

    src/core/io/export/obj_exporter.h
    src/core/io/export/obj_exporter.cpp)

target_include_directories(Ariadnis PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(Ariadnis PRIVATE
    glfw
    glm::glm
    opengl32
    comdlg32
    imgui
    nfd
)

install(TARGETS Ariadnis RUNTIME DESTINATION .)