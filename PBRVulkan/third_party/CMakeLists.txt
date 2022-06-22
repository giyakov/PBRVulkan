add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/glfw)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/glm)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/tinyobjloader)
add_library(tinyobjloader::tinyobjloader ALIAS tinyobjloader)

add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${CMAKE_CURRENT_LIST_DIR}/stb)

add_library(imgui STATIC
    ${CMAKE_CURRENT_LIST_DIR}/imgui/imgui_demo.cpp
    ${CMAKE_CURRENT_LIST_DIR}/imgui/imgui_draw.cpp
    ${CMAKE_CURRENT_LIST_DIR}/imgui/imgui_tables.cpp
    ${CMAKE_CURRENT_LIST_DIR}/imgui/imgui_widgets.cpp
    ${CMAKE_CURRENT_LIST_DIR}/imgui/imgui.cpp
)
target_include_directories(imgui PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/imgui/
    ${CMAKE_CURRENT_LIST_DIR}/imgui/backends/
)
add_library(imgui::imgui ALIAS imgui)