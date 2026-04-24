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

cmake_minimum_required(VERSION 3.28.3)

include(FetchContent)
FetchContent_Declare(googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
  URL_HASH SHA256=8ad598c73ad796e0d8280b082cebd82a630d73e73cd3c70057938a6501bba5d7
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  foreach(_t gtest gtest_main gmock gmock_main)
    target_compile_options(${_t} PRIVATE -Wno-character-conversion)
  endforeach()
endif()

enable_testing()

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

# Add math source file directly since it's not a separate library
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
    oidipc
)

if(WIN32)
  add_custom_command(
    TARGET test_raw_data_decode
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:oidipc>
            $<TARGET_FILE_DIR:test_raw_data_decode>
    VERBATIM
  )
endif()

add_test(NAME RawDataDecodeTests COMMAND test_raw_data_decode)

# Test for IPC message exchange
add_executable(test_message_exchange
    test_message_exchange.cpp
)

target_include_directories(test_message_exchange
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src/ipc
)

target_link_libraries(test_message_exchange
    PRIVATE
    Qt6::Network
    Qt6::Core
    GTest::gtest_main
    GTest::gtest
)

# Add message_exchange source file directly for testing
target_sources(test_message_exchange PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ipc/message_exchange.cpp
)

add_test(NAME MessageExchangeTests COMMAND test_message_exchange)

