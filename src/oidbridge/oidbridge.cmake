cmake_minimum_required(VERSION 3.10.0)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../../commom.cmake)

SET(SOURCES
    ../oid_bridge.cpp
    ../../debuggerinterface/python_native_interface.cpp
    ../../ipc/message_exchange.cpp
    ../../ipc/raw_data_decode.cpp)


include_directories(${CMAKE_CURRENT_SOURCE_DIR}
                    ${CMAKE_CURRENT_SOURCE_DIR}/../..)

# # Prevent strip from producing spurious error messages
# QMAKE_STRIP = echo "strip disabled: "

# TEMPLATE = lib
add_compile_options("$<$<PLATFORM_ID:UNIX>:-Wl,--exclude-libs,ALL>")

# Need to unset these vars to trigger find_package with a different required version
unset(PYTHON_INCLUDE_DIR CACHE)
unset(PYTHON_LIBRARY CACHE)
# # Deployment settings
# macx {
#     BRIDGE_INSTALL_FOLDER = $$PREFIX/OpenImageDebugger/oidwindow.app/Contents/MacOS
# }
# linux {
#     BRIDGE_INSTALL_FOLDER = $$PREFIX/OpenImageDebugger/
# }

# include($$PWD/../../resources/deployscripts.pri)
