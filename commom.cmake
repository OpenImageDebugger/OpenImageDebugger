cmake_minimum_required(VERSION 3.1.0)

set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
set(CMAKE_INSTALL_PREFIX /usr/local CACHE PATH "Install path" FORCE)

set(CXX_STANDARD 11)
set(CXX_STANDARD_REQUIRED True)

find_package(Threads REQUIRED)
find_package(Qt5 COMPONENTS Network REQUIRED)

set(CMAKE_CXX_FLAGS
    ${CMAKE_CXX_FLAGS}
    "-fvisibility=hidden")

# Prevent strip from producing spurious error messages
# QMAKE_STRIP = echo "strip disabled: "
