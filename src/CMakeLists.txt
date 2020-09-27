cmake_minimum_required(VERSION 3.10.0)

project(oidwindow)

include(${CMAKE_CURRENT_SOURCE_DIR}/../commom.cmake)

find_package(OpenGL REQUIRED)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 COMPONENTS Core Gui OpenGL Widgets REQUIRED)

set(SOURCES
    oid_window.cpp 
    io/buffer_exporter.cpp 
    ipc/message_exchange.cpp 
    ipc/raw_data_decode.cpp 
    math/assorted.cpp 
    math/linear_algebra.cpp 
    ui/decorated_line_edit.cpp 
    ui/gl_canvas.cpp 
    ui/gl_text_renderer.cpp 
    ui/go_to_widget.cpp
    ui/main_window/main_window.ui
    ui/main_window/auto_contrast.cpp 
    ui/main_window/initialization.cpp 
    ui/main_window/main_window.cpp 
    ui/main_window/message_processing.cpp 
    ui/main_window/ui_events.cpp 
    ui/symbol_completer.cpp 
    ui/symbol_search_input.cpp
    resources/resources.qrc
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

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/Khronos)

target_compile_options(${PROJECT_NAME} PRIVATE "$<$<PLATFORM_ID:UNIX>:-Wl,--exclude-libs,ALL>")

target_link_libraries(${PROJECT_NAME} PRIVATE Qt5::Core Qt5::Gui Qt5::OpenGL Qt5::Network Qt5::Widgets Threads::Threads ${OPENGL_gl_LIBRARY})


# target.path = $$PREFIX/OpenImageDebugger/

# INSTALLS += 
#   target

