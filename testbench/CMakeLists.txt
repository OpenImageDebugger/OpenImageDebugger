cmake_minimum_required(VERSION 3.22.1)

project(testbench)

include(../common.cmake)

set(SOURCES main.cpp)

add_executable(${PROJECT_NAME} ${SOURCES})

# Remove -Werror for testbench (test code doesn't need strict error handling)
# We need to override the -Werror from common.cmake
target_compile_options(${PROJECT_NAME} PRIVATE
    "$<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wno-error>")

target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)
