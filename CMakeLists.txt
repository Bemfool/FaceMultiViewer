project(face_multiviewer LANGUAGES C CXX)
cmake_minimum_required(VERSION 3.18)

add_compile_options(-fPIC)
add_compile_options(-Wno-register) # Ignore glm register warning in C++17

set(SHADER_DIR ${CMAKE_CURRENT_LIST_DIR}/shader/)
configure_file(${CMAKE_CURRENT_LIST_DIR}/include/config.h.in ${CMAKE_CURRENT_LIST_DIR}/include/config.h @ONLY)

# OpenGL
set(GLAD_PATH /home/bemfoo/Repository/glad)
find_package(glfw3 REQUIRED)

# GLM
include_directories(/home/bemfoo/Repository/glm)

# ImGUI
set(IMGUI_PATH /home/bemfoo/Repository/imgui)
include_directories(${IMGUI_PATH})
include_directories(${IMGUI_PATH}/backends)

# stb_image.h
set(STB_INCLUDE_DIRS /home/bemfoo/Repository/stb)
include_directories(${STB_INCLUDE_DIRS})

# Tinyxml2
set(TINYXML_DIR /home/bemfoo/Repository/tinyxml2)
set(TINYXML_INCLUDE_DIRS ${TINYXML_DIR})
set(TINYXML_SRC_DIRS ${TINYXML_DIR})
include_directories(${TINYXML_INCLUDE_DIRS})

# Assimp
find_package(assimp REQUIRED)

# Boost
find_package(Boost REQUIRED system filesystem program_options)
include_directories(${Boost_INCLUDE_DIRS})

# Head files
include_directories(./include)

add_executable(face_multiviewer 
    src/face_multiviewer.cpp 
    ${GLAD_PATH}/src/glad.c
    ${IMGUI_PATH}/imgui.cpp
    ${IMGUI_PATH}/imgui_draw.cpp
    ${IMGUI_PATH}/imgui_widgets.cpp
    ${IMGUI_PATH}/imgui_tables.cpp
    ${IMGUI_PATH}/backends/imgui_impl_glfw.cpp
    ${IMGUI_PATH}/backends/imgui_impl_opengl3.cpp
    ${TINYXML_SRC_DIRS}/tinyxml2.cpp
)
target_link_libraries(
    face_multiviewer 
    GL glfw3 assimp m Xrandr Xi X11 Xxf86vm pthread dl Xinerama Xcursor
    ${Boost_LIBRARIES} boost_filesystem boost_program_options
)
set_target_properties(face_multiviewer
    PROPERTIES
        CXX_STANDARD 17
)