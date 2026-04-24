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

# Non-Windows: before `project()`, set compiler cache entries when available:
# Homebrew llvm on macOS, otherwise gcc-14 / g++-14 on Linux.
if(NOT WIN32)
  if(APPLE)
    execute_process(
      COMMAND brew --prefix llvm
      OUTPUT_VARIABLE _oid_brew_llvm
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
      RESULT_VARIABLE _oid_brew_llvm_rv
    )
    if(_oid_brew_llvm_rv EQUAL 0
       AND IS_DIRECTORY "${_oid_brew_llvm}/bin")
      set(CMAKE_C_COMPILER
          "${_oid_brew_llvm}/bin/clang"
          CACHE FILEPATH "C compiler" FORCE)
      set(CMAKE_CXX_COMPILER
          "${_oid_brew_llvm}/bin/clang++"
          CACHE FILEPATH "C++ compiler" FORCE)
    endif()
  else()
    find_program(_oid_cc gcc-14)
    find_program(_oid_cxx g++-14)
    if(_oid_cc AND _oid_cxx)
      set(CMAKE_C_COMPILER "${_oid_cc}" CACHE FILEPATH "C compiler" FORCE)
      set(CMAKE_CXX_COMPILER "${_oid_cxx}" CACHE FILEPATH "C++ compiler" FORCE)
    endif()
  endif()
endif()

# For clangd and other tools; must be set before the first top-level project().
set(CMAKE_EXPORT_COMPILE_COMMANDS
    ON
    CACHE BOOL
    "Emit compile_commands.json for IDEs and static analysis")

project(OpenImageDebugger)

include(${CMAKE_CURRENT_SOURCE_DIR}/common.cmake)

# oidipc always builds the RawDataDecode C++20 module. CMake and the toolchain
# must support module scanning (Ninja/VS) and a compiler with import support
# (not Apple/Xcode Clang; use Homebrew llvm on macOS when brew is available).
if(NOT CMAKE_GENERATOR MATCHES "Ninja" AND NOT CMAKE_GENERATOR
                                           MATCHES "Visual Studio")
  message(
    FATAL_ERROR
    "C++20 modules require -G Ninja, \"Ninja Multi-Config\", or Visual Studio 17.4+. "
      "This generator is not supported: ${CMAKE_GENERATOR}"
  )
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
  message(
    FATAL_ERROR
    "C++20 modules are not available with Apple/Xcode Clang. Use Homebrew "
      "LLVM (brew install llvm) or set CMAKE_CXX_COMPILER to its clang++."
  )
endif()

add_subdirectory(src)
add_subdirectory(src/ipc)
add_subdirectory(src/oidbridge)

# Add tests if enabled
option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
