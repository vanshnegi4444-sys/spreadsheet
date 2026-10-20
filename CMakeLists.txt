cmake_minimum_required(VERSION 3.20)
project(xspreadsheet LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find GLFW and OpenGL
find_package(glfw3 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(nlohmann_json REQUIRED)

set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui")

set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

set(CORE_SOURCES
    cpp/src/core/alphabet.cpp
    cpp/src/core/cell_range.cpp
    cpp/src/core/format.cpp
    cpp/src/core/formula.cpp
    cpp/src/core/merge.cpp
    cpp/src/core/row_col.cpp
    cpp/src/core/auto_filter.cpp
    cpp/src/core/validation.cpp
    cpp/src/core/sheet_data.cpp
    cpp/src/core/spreadsheet_book.cpp
    cpp/src/locale/locale.cpp
)

set(UI_SOURCES
    cpp/src/ui/dialog_validation.cpp
    cpp/src/ui/dialog_sort_filter.cpp
    cpp/src/ui/dialog_print.cpp
    cpp/src/ui/spreadsheet_ui.cpp
)

add_executable(xspreadsheet
    cpp/src/main.cpp
    ${CORE_SOURCES}
    ${UI_SOURCES}
    ${IMGUI_SOURCES}
)

target_include_directories(xspreadsheet PRIVATE
    cpp/src
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

target_link_libraries(xspreadsheet PRIVATE
    glfw
    OpenGL::GL
    nlohmann_json::nlohmann_json
    imm32
    gdi32
)

# Test target
add_executable(xspreadsheet_tests
    cpp/tests/test_main.cpp
    ${CORE_SOURCES}
)

target_include_directories(xspreadsheet_tests PRIVATE
    cpp/src
)

target_link_libraries(xspreadsheet_tests PRIVATE
    nlohmann_json::nlohmann_json
)
