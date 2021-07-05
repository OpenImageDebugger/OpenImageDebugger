cmake_minimum_required(VERSION 3.10.0)

project(oidbridge_python3)

find_package(PythonLibs 3 EXACT)

include(${CMAKE_CURRENT_SOURCE_DIR}/../oidbridge.cmake)