# The MIT License (MIT)

# Copyright (c) 2015-2021 OpenImageDebugger contributors
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

cmake_minimum_required(VERSION 3.10.0)

project(oidbridge_python3)

find_package(PythonLibs 3.8.10 REQUIRED)

add_library(${PROJECT_NAME} SHARED
            oid_bridge.cpp
            ../debuggerinterface/python_native_interface.cpp
            ../ipc/message_exchange.cpp
            ../ipc/raw_data_decode.cpp
            ../system/process/process.cpp
            $<$<BOOL:${UNIX}>:../system/process/process_unix.cpp>
            $<$<BOOL:${WIN32}>:../system/process/process_win32.cpp>)

target_compile_options(${PROJECT_NAME}
                       PUBLIC "$<$<PLATFORM_ID:UNIX>:-Wl,--exclude-libs,ALL>")

target_include_directories(${PROJECT_NAME}
                           PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../)

target_include_directories(${PROJECT_NAME} SYSTEM
                           PRIVATE ${PYTHON_INCLUDE_DIR})

target_link_libraries(${PROJECT_NAME} PRIVATE
                      Qt5::Core
                      Qt5::Network
                      Threads::Threads
                      ${PYTHON_LIBRARY})

install(TARGETS ${PROJECT_NAME} DESTINATION OpenImageDebugger)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../resources/deployscripts.cmake)
