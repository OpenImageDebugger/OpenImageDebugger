cmake_minimum_required(VERSION 3.10.0)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../../commom.cmake)

SET(SOURCES
    ../oid_bridge.cpp
    ../../debuggerinterface/python_native_interface.cpp
    ../../ipc/message_exchange.cpp
    ../../ipc/raw_data_decode.cpp
    ../../system/process/process.cpp
    ../../system/process/process_unix.cpp
    ../../system/process/process_win32.cpp)


include_directories(${CMAKE_CURRENT_SOURCE_DIR}
                    ${CMAKE_CURRENT_SOURCE_DIR}/../..)

add_compile_options("$<$<PLATFORM_ID:UNIX>:-Wl,--exclude-libs,ALL>")

# Need to unset these vars to trigger find_package with a different required version
unset(PYTHON_INCLUDE_DIR CACHE)
unset(PYTHON_LIBRARY CACHE)

# TODO: deal with macos installation

include(${CMAKE_CURRENT_SOURCE_DIR}/../../../resources/deployscripts.cmake)
