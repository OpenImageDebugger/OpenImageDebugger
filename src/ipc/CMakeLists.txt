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

project(oidipc CXX)

find_package(Qt6 "6.2.4" REQUIRED COMPONENTS Network)

# Find required protobuf package
find_package(protobuf "5.27.0" CONFIG)
if(NOT Protobuf_FOUND)
  find_package(Protobuf "5.27.0" REQUIRED)
endif()

find_package(gRPC "1.72" REQUIRED)

set(CMAKE_INCLUDE_CURRENT_DIR TRUE)

set(PROTO_FILES
    buffer.proto
)

add_library(${PROJECT_NAME} SHARED
            message_exchange.cpp
            raw_data_decode.cpp
            ${PROTO_FILES})

set_target_properties(${PROJECT_NAME} PROPERTIES
                      WINDOWS_EXPORT_ALL_SYMBOLS ON)

target_include_directories(${PROJECT_NAME}
                           PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(${PROJECT_NAME}
                      PUBLIC Qt6::Network
                      PRIVATE protobuf::libprotobuf
                      PRIVATE grpc::grpc)

protobuf_generate(TARGET ${PROJECT_NAME}
                  PROTOC_OUT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION OpenImageDebugger)

