cmake_minimum_required(VERSION 3.1.0)

project(testbench)

include(../common.cmake)

set(SOURCES main.cpp)

add_executable(${PROJECT_NAME} ${SOURCES})
target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)
