cmake_minimum_required(VERSION 3.22.1)

project(testbench)

include(../common.cmake)

set(SOURCES main.cpp)

add_executable(${PROJECT_NAME} ${SOURCES})
target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)
