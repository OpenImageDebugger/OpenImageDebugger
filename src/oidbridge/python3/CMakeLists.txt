cmake_minimum_required(VERSION 3.10.0)

include(${CMAKE_CURRENT_SOURCE_DIR}/../oidbridge.cmake)

project(oidbridge_python3)

find_package(PythonLibs 3 EXACT)

add_library(${PROJECT_NAME} SHARED ${SOURCES})

target_include_directories(${PROJECT_NAME}
                           PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_include_directories(${PROJECT_NAME} SYSTEM
                           PRIVATE ${PYTHON_INCLUDE_DIR})

target_link_libraries(${PROJECT_NAME} PRIVATE
                      Qt5::Core
                      Qt5::Network
                      Threads::Threads
                      ${PYTHON_LIBRARY})

install(TARGETS ${PROJECT_NAME} DESTINATION OpenImageDebugger)
