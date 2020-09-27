cmake_minimum_required(VERSION 3.1.0)

project(testbench)

set(CXX_STANDARD 11)
set(CXX_STANDARD_REQUIRED True)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

set(SOURCES main.cpp)

add_executable(${PROJECT_NAME} ${SOURCES})
target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)
