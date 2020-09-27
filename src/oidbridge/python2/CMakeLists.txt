cmake_minimum_required(VERSION 3.10.0)

include(${CMAKE_CURRENT_SOURCE_DIR}/../oidbridge.cmake)

project(oidbridge_python2)

find_package(PythonLibs REQUIRED)
# # For some reason, on Focal, pkg-config does not find python2 libs by default
# linux {
#     PKG_CONFIG_PATH += /usr/lib/x86_64-linux-gnu/pkgconfig/
# }

# macx {
#     PKG_CONFIG_PATH=/System/Library/Frameworks/Python.framework/Versions/2.7/lib/pkgconfig/
# }

# PKGCONFIG += python-2.7

# message(OpenImageDebugger Python2 Bridge build mode: $$BUILD_MODE)

# TARGET = oidbridge_python2
# target.path = $$BRIDGE_INSTALL_FOLDER

# INSTALLS += \
#   target \
#   $$SCRIPT_INSTALLS
add_library(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
                            ${CMAKE_CURRENT_SOURCE_DIR}
                            ${PYTHON_INCLUDE_DIR})

target_link_libraries(${PROJECT_NAME} PRIVATE Qt5::Core Qt5::Network Threads::Threads ${PYTHON_LIBRARY})