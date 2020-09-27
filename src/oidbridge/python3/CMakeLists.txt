cmake_minimum_required(VERSION 3.10.0)

include(${CMAKE_CURRENT_SOURCE_DIR}/../oidbridge.cmake)

project(oidbridge_python3)

find_package(PythonLibs 3 EXACT)

# TARGET = oidbridge_python2
# target.path = $$BRIDGE_INSTALL_FOLDER

# INSTALLS += \
#   target \
#   $$SCRIPT_INSTALLS
add_library(${PROJECT_NAME} SHARED ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
                            ${CMAKE_CURRENT_SOURCE_DIR}
                            ${PYTHON_INCLUDE_DIR})

target_link_libraries(${PROJECT_NAME} PRIVATE Qt5::Core Qt5::Network Threads::Threads ${PYTHON_LIBRARY})

# Need to unset these vars to trigger find_package with a different required version
unset(PYTHON_INCLUDE_DIR CACHE)
unset(PYTHON_LIBRARY CACHE)