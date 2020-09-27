cmake_minimum_required(VERSION 3.1.0)

project(OpenImageDebugger)

add_subdirectory(src)
add_subdirectory(src/oidbridge/python2)
if(NOT WIN32)
    add_subdirectory(src/oidbridge/python3)
else()
    message(WARNING "Python3 is currently not supported on Windows")
endif()
