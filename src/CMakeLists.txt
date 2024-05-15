# The MIT License (MIT)

# Copyright (c) 2015-2024 OpenImageDebugger contributors
# (https://github.com/OpenImageDebugger/OpenImageDebugger)

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to
# deal in the Software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
# sell copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.

cmake_minimum_required(VERSION 3.22.1)

project(oidwindow)

set(OpenGL_GL_PREFERENCE LEGACY)
find_package(OpenGL 2.1 REQUIRED)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 5.15.1 REQUIRED COMPONENTS Core Gui OpenGL Widgets)

set(SOURCES
    oid_window.cpp
    io/buffer_exporter.cpp
    ipc/message_exchange.cpp
    ipc/raw_data_decode.cpp
    math/linear_algebra.cpp
    ui/decorated_line_edit.cpp
    ui/gl_canvas.cpp
    ui/gl_text_renderer.cpp
    ui/go_to_widget.cpp
    ui/main_window/auto_contrast.cpp
    ui/main_window/initialization.cpp
    ui/main_window/main_window.cpp
    ui/main_window/message_processing.cpp
    ui/main_window/ui_events.cpp
    ui/symbol_completer.cpp
    ui/symbol_search_input.cpp
    visualization/components/background.cpp
    visualization/components/buffer.cpp
    visualization/components/buffer_values.cpp
    visualization/components/camera.cpp
    visualization/components/component.cpp
    visualization/events.cpp
    visualization/game_object.cpp
    visualization/shader.cpp
    visualization/shaders/background_fs.cpp
    visualization/shaders/background_vs.cpp
    visualization/shaders/buffer_fs.cpp
    visualization/shaders/buffer_vs.cpp
    visualization/shaders/text_fs.cpp
    visualization/shaders/text_vs.cpp
    visualization/stage.cpp
)

set(QT_FORMS ui/main_window/main_window.ui)
set(QT_RESOURCES resources/resources.qrc)

add_executable(${PROJECT_NAME} ${SOURCES} ${QT_FORMS} ${QT_RESOURCES})

target_include_directories(${PROJECT_NAME}
                           PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Include external dependencies as "system dependencies" so compilation errors from them are ignored
target_include_directories(${PROJECT_NAME} SYSTEM
                           PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/Eigen/Eigen
                           PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/Khronos)

target_compile_options(${PROJECT_NAME} PRIVATE "$<$<PLATFORM_ID:UNIX>:-Wl,--exclude-libs,ALL>")

target_link_libraries(${PROJECT_NAME} PRIVATE
                      Qt5::Core
                      Qt5::Gui
                      Qt5::OpenGL
                      Qt5::Network
                      Qt5::Widgets
                      Threads::Threads
                      ${OPENGL_gl_LIBRARY})

install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION OpenImageDebugger)

add_subdirectory(oidbridge)
