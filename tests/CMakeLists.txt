# The MIT License (MIT)
#
# Copyright (c) 2015-2026 OpenImageDebugger contributors
# (https://github.com/OpenImageDebugger/OpenImageDebugger)
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to
# deal in the Software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
# sell copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.

cmake_minimum_required(VERSION 3.22.1)

include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# enable_testing() is already called in the root CMakeLists.txt when BUILD_TESTS is ON

# Test for linear algebra
add_executable(test_linear_algebra
    test_linear_algebra.cpp
)

target_include_directories(test_linear_algebra
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src/ipc
)

target_include_directories(test_linear_algebra SYSTEM
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src/thirdparty/Eigen
)

target_link_libraries(test_linear_algebra
    PRIVATE
    GTest::gtest_main
    GTest::gtest
)

target_sources(test_linear_algebra PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/math/linear_algebra.cpp
)

add_test(NAME LinearAlgebraTests COMMAND test_linear_algebra)

# Test for raw data decode
add_executable(test_raw_data_decode
    test_raw_data_decode.cpp
)

target_include_directories(test_raw_data_decode
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src/ipc
)

target_link_libraries(test_raw_data_decode
    PRIVATE
    GTest::gtest_main
    GTest::gtest
)

target_sources(test_raw_data_decode PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ipc/raw_data_decode.cpp
)

add_test(NAME RawDataDecodeTests COMMAND test_raw_data_decode)

# Test for IPC message exchange
add_executable(test_message_exchange
    test_message_exchange.cpp
)

target_include_directories(test_message_exchange
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src/ipc
)

find_package(protobuf "5.27.0" CONFIG)
if(NOT Protobuf_FOUND)
  find_package(Protobuf "5.27.0" REQUIRED)
endif()

find_package(Qt6 "6.2.4" REQUIRED COMPONENTS Network Core)

target_sources(test_message_exchange PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ipc/message_exchange.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ipc/buffer.proto
)

set(CMAKE_INCLUDE_CURRENT_DIR TRUE)
protobuf_generate(TARGET test_message_exchange
                  PROTOC_OUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../src/ipc)

target_link_libraries(test_message_exchange
    PRIVATE
    Qt6::Network
    Qt6::Core
    protobuf::libprotobuf
    GTest::gtest_main
    GTest::gtest
)

add_test(NAME MessageExchangeTests COMMAND test_message_exchange)
set_tests_properties(MessageExchangeTests PROPERTIES TIMEOUT 5)

