cmake_minimum_required(VERSION 3.1.0)

set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type")
set(CMAKE_INSTALL_PREFIX /usr/local CACHE PATH "Install path")

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS False)

find_package(Threads REQUIRED)
find_package(Qt5 COMPONENTS Network REQUIRED)

add_compile_options(-fvisibility=hidden)
