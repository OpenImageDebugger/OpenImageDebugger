# The MIT License (MIT)

# Copyright (c) 2015-2025 OpenImageDebugger contributors
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

project(oidbridge CXX)

find_package(Python 3.10.12 REQUIRED COMPONENTS Development)

add_library(${PROJECT_NAME} MODULE
            oid_bridge.cpp
            ../debuggerinterface/python_native_interface.cpp
            ../system/process/process.cpp
            $<$<BOOL:${UNIX}>:../system/process/process_unix.cpp>
            $<$<BOOL:${WIN32}>:../system/process/process_win32.cpp>)

add_dependencies(${PROJECT_NAME} oidipc)

# Things get a bit when loading a shared lib from another shared lib or module
# We need to set the RPATH to ensure that one shared lib can find the other
set_target_properties(${PROJECT_NAME} PROPERTIES
                      INSTALL_RPATH
                      "${CMAKE_INSTALL_PREFIX}/lib;${CMAKE_SOURCE_DIR}/lib/ipc")

target_compile_options(${PROJECT_NAME} PUBLIC "$<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-fvisibility=hidden>")

target_link_options(${PROJECT_NAME}
                     PUBLIC "$<$<PLATFORM_ID:Linux>:-Wl,--exclude-libs,ALL;>")

target_include_directories(${PROJECT_NAME}
                           PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../
                           PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../ipc)

target_include_directories(${PROJECT_NAME} SYSTEM
                           PRIVATE ${Python_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME} PRIVATE
                      oidipc
                      ${Python_LIBRARIES})

install(TARGETS ${PROJECT_NAME} DESTINATION OpenImageDebugger)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../resources/deployscripts.cmake)
