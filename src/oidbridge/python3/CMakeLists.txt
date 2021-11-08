cmake_minimum_required(VERSION 3.10.0)

project(oidbridge_python3)

if(NOT APPLE)
    find_package(PythonLibs 3 EXACT)
else()
    set(LLDB_PYTHON3_VERSION "" CACHE FILEPATH "Version of python3 library, used by lldb.
    In order to properly compile the plugin, you must make sure it is linked against the same python lib binary used by lldb.
    Run:
        lldb
        (lldb) script
        Python Interactive Interpreter. To exit, type 'quit()', 'exit()' or Ctrl-D.
        >>> import sys; print(sys.path)
    Examine output and insert python version here. For example: 3.8")

    if (LLDB_PYTHON3_VERSION STREQUAL "")
        message(SEND_ERROR "Version of python3 library, used by lldb, isn't set. Verify correctness of LLDB_PYTHON3_VERSION variable")
    endif()

    set(PYTHON_INCLUDE_DIR "/Applications/Xcode.app/Contents/Developer/Library/Frameworks/Python3.framework/Versions/${LLDB_PYTHON3_VERSION}/include/python${LLDB_PYTHON3_VERSION}")
    set(PYTHON_LIBRARY "/Applications/Xcode.app/Contents/Developer/Library/Frameworks/Python3.framework/Versions/${LLDB_PYTHON3_VERSION}/lib/libpython${LLDB_PYTHON3_VERSION}.dylib")

    if(NOT EXISTS ${PYTHON_INCLUDE_DIR})
        message(SEND_ERROR "Python include directory wasn't found: ${PYTHON_INCLUDE_DIR}
        Verify correctness of LLDB_PYTHON3_VERSION variable")
    endif()
    if(NOT EXISTS ${PYTHON_LIBRARY})
        message(SEND_ERROR "Python library file wasn't found: ${PYTHON_LIBRARY}
        Verify correctness of LLDB_PYTHON3_VERSION variable")
    endif()
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/../oidbridge.cmake)