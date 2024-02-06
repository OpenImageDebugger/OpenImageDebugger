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

cmake_minimum_required(VERSION 3.24.0)

project(oidbridge_python3)

if (NOT Python3_ROOT_DIR)
    execute_process(COMMAND "lldb" "--print-script-interpreter-info"
        OUTPUT_VARIABLE lldb_output ERROR_VARIABLE lldb_err RESULT_VARIABLE lldb_result)
    if (lldb_result EQUAL 0)
        string(JSON lldb_python_exe GET ${lldb_output} "executable")
        string(JSON lldb_python_root GET ${lldb_output} "prefix")

        execute_process(COMMAND "${lldb_python_exe}" "--version" OUTPUT_VARIABLE python_full_version RESULT_VARIABLE python_result)
        if (python_result EQUAL 0)
            string(REGEX MATCH "Python +([0-9]+\.[0-9]+).*" python_text_version "${python_full_version}")
            if (python_text_version)
                set(lldb_python_version ${CMAKE_MATCH_1})
                message(STATUS "LLDB was configured with Python ${lldb_python_version} at ${lldb_python_exe} (root is ${lldb_python_root})")
            else()
                set(lldb_python_exe FALSE)
                set(lldb_python_root FALSE)
                set(lldb_python_version FALSE)
            endif()
        else()
            set(lldb_python_exe FALSE)
            set(lldb_python_root FALSE)
            set(lldb_python_version FALSE)
        endif()
    endif()

    if (UNIX)
        # Make GDB to print script interpreter info, similar to LLDB
        file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/gdbcomm" "py
import sys
import gdb
print(f'{{\"version\": \"{sys.version_info.major}.{sys.version_info.minor}\", \"executable\": \"{sys.executable}\", \"prefix\": \"{sys.prefix}\"}}')
gdb.execute(\"quit\")
")

        execute_process(COMMAND "gdb" "-q" "--command=${CMAKE_CURRENT_BINARY_DIR}/gdbcomm"
            OUTPUT_VARIABLE gdb_output RESULT_VARIABLE gdb_result)

        if (gdb_result EQUAL 0)
            string(JSON gdb_python_version GET ${gdb_output} "version")
            string(JSON gdb_python_root GET ${gdb_output} "prefix")
            string(JSON gdb_python_exe GET ${gdb_output} "executable")

            message(STATUS "GDB was configured with Python ${gdb_python_version} at ${gdb_python_exe} (root is ${gdb_python_root})")
        endif()

        if (gdb_python_root AND lldb_python_root)
            if(NOT gdb_python_root PATH_EQUAL lldb_python_root)
                message(FATAL_ERROR
                    "CMake determined that you have installed both GDB and LLDB and they are configured to use python at different locations (${gdb_python_root} and ${lldb_python_root} respectively)
                    Please rerun configuration explicitly specifying python root path
                    cmake .... -DPython3_ROOT_DIR=/path
                    ")
            endif()
        endif()

        if (gdb_python_version AND lldb_python_version)
            if(NOT gdb_python_version VERSION_EQUAL lldb_python_version)
                message(FATAL_ERROR
                        "CMake determined that you have installed both GDB and LLDB and they are configured to use different python versions (${gdb_python_version} and ${lldb_python_version} respectively).
                        Please rerun configuration explicitly specifying python root path
                        cmake .... -DPython3_ROOT_DIR=/path
                        ")
            endif()
        endif()
    endif()

    if (lldb_python_root)
        set(Python3_ROOT_DIR ${lldb_python_root})
        set(python_version ${lldb_python_version})
    elseif(gdb_python_root)
        set(Python3_ROOT_DIR ${gdb_python_root})
        set(python_version ${gdb_python_version})
    endif()


    find_package(Python3 ${python_version} EXACT COMPONENTS Development)
else()
    # rely on user input
    # assume that user runs cmake .. -DPython3_ROOT_DIR=/path/to/python
    find_package(Python3 3.8 COMPONENTS Development)
endif()

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
                           PRIVATE ${Python3_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME} PRIVATE
                      Qt5::Core
                      Qt5::Network
                      Threads::Threads
                      ${Python3_LIBRARIES})

install(TARGETS ${PROJECT_NAME} DESTINATION OpenImageDebugger)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../resources/deployscripts.cmake)
