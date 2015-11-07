cmake_minimum_required (VERSION 2.6)
project (gdb-imagewatch)
SET(FONT_PATH "${CMAKE_CURRENT_LIST_DIR}/build/serif.ttf")
SET(CMAKE_CXX_FLAGS "-std=c++11 -Wall -g")
add_definitions(-DFONT_PATH=\"${FONT_PATH}\")
add_library(gdb-imagewatch SHARED src/buffer.cpp
                                  src/camera.cpp
                                  src/buffer_values.cpp
                                  src/main.cpp
                                  src/shaders/buff_frag_shader.cpp
                                  src/shaders/buff_vert_shader.cpp
                                  src/shaders/text_frag_shader.cpp
                                  src/shaders/text_vert_shader.cpp
                                  )
target_link_libraries(gdb-imagewatch glfw3 GLU GL Xrandr Xext X11 GLEW rt dl pthread Xi Xxf86vm m Xcursor Xinerama freetype)
target_include_directories(gdb-imagewatch PUBLIC /usr/include/python3.4 /usr/include/freetype2)
