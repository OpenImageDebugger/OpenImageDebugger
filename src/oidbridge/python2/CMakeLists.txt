cmake_minimum_required(VERSION 3.10.0)

project(oidbridge_python2)

find_package(PythonLibs 2 EXACT)

include(${CMAKE_CURRENT_SOURCE_DIR}/../oidbridge.cmake)
