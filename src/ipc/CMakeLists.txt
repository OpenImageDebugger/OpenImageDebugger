# The MIT License (MIT)

# Copyright (c) 2015-2026 OpenImageDebugger contributors
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

cmake_minimum_required(VERSION 3.28.3)

project(oidipc CXX)

if(NOT DEFINED OID_IPC_LIBRARY_TYPE)
  # Emscripten emulates shared libraries as static objects and drops their
  # symbols from the final link (see em++'s "-shared" warning), which leaves
  # oidwindow with undefined BufferAssembler/codec symbols. Build oidipc STATIC
  # on WASM so its symbols are linked into oidwindow.wasm; keep it SHARED on
  # native (installed as a runtime library).
  if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    set(OID_IPC_LIBRARY_TYPE STATIC)
  else()
    set(OID_IPC_LIBRARY_TYPE SHARED)
  endif()
endif()

# The codec/data core is Qt-free and platform-agnostic (compiles everywhere,
# including under Emscripten).
add_library(${PROJECT_NAME} ${OID_IPC_LIBRARY_TYPE}
            buffer_assembler.cpp
            message_exchange.cpp
            raw_data_decode.cpp)

set_target_properties(${PROJECT_NAME} PROPERTIES
                      WINDOWS_EXPORT_ALL_SYMBOLS ON)

target_include_directories(${PROJECT_NAME}
                           PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# TCP transport is native-only. WASM has no TCP -- it uses PostMessageTransport
# (src/platform) -- so standalone Asio (which does not compile under
# Emscripten: it requires Windows or POSIX threading/signal primitives) is
# neither built nor linked there.
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  target_sources(${PROJECT_NAME} PRIVATE
                 asio_transport.cpp)
  target_include_directories(${PROJECT_NAME} SYSTEM
                             PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty/asio/asio/include)
  target_compile_definitions(${PROJECT_NAME} PUBLIC ASIO_STANDALONE ASIO_NO_DEPRECATED)
  find_package(Threads REQUIRED)
  target_link_libraries(${PROJECT_NAME} PUBLIC Threads::Threads)
endif()

install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION OpenImageDebugger)

