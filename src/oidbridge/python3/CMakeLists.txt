cmake_minimum_required(VERSION 3.10.0)

project(oidbridge_python3)

# Find python automaticaly in such cases:
# - OS is linux or windows
# - OS is Mac and cmake configuration has been called from CI 
if(IS_CI OR NOT APPLE)
    find_package(PythonLibs 3 EXACT)
else()
    set(APPLE_LLDB_PYTHON3_INFO
        "In order to properly compile the plugin, you must make sure it is linked against the same python lib binary used by lldb.
        Run:
            lldb
            (lldb) script import sys; print(sys.path)
        This should print a list of directories that lldb uses to find libraries.
        Take into consideration that IDE may use built-in lldb which differs from system one (e.g. VS Code and CLion do that).
        In such case, you need to run the command above from your IDE's lldb debug console.
        Note that some lldb assemblies use the python library while others use the python framework.
        ")

    # Load variables from cache. Allow user to provide paths manually.
    set(APPLE_LLDB_PYTHON3_INCLUDE_DIR "" CACHE PATH "Path to python3 include directory.
        ${APPLE_LLDB_PYTHON3_INFO}
        Examine output and insert path to include directory here. This directory must contain 'Python.h' file.
        Several examples:
            - /Applications/Xcode.app/Contents/Developer/Library/Frameworks/Python3.framework/Versions/3.8/include/python3.8 (system-one, used by lldb command)
            - /usr/local/Frameworks/Python.framework/Versions/3.9/include/python3.9 (instaled by user)")

    set(APPLE_LLDB_PYTHON3_LIBRARY "" CACHE FILEPATH "Path to python3 library file.
        ${APPLE_LLDB_PYTHON3_INFO}
        Examine output and insert path to 'libpython*.dylib' library file here.
        In case you have set the library file, you don't need to set the framework directory and vice versa.
        Several examples:
            - /Applications/Xcode.app/Contents/Developer/Library/Frameworks/Python3.framework/Versions/3.8/lib/libpython3.8.dylib (system-one, used by lldb command)
            - /usr/local/Frameworks/Python.framework/Versions/3.9/lib/libpython3.9.dylib (instaled by user)
            - ~/.vscode/extensions/vadimcn.vscode-lldb-1.6.10/lldb/lib/libpython39.dylib (used by CodeLLDB - plugin for VS Code)")

    set(APPLE_LLDB_PYTHON3_FRAMEWORK_DIR "" CACHE FILEPATH "Path to python3 framework directory.
        ${APPLE_LLDB_PYTHON3_INFO}
        Examine output and insert path to framework directory here. This directory must contain 'Python.framework' subdirectory.
        In case you have set the framework directory, you don't need to set the library file and vice versa.
        Example:
            - /Applications/CLion.app/Contents/bin/lldb/mac (used by CLion)")

    # Check if user has provided correct paths.
    if(NOT EXISTS ${APPLE_LLDB_PYTHON3_INCLUDE_DIR})
        message(SEND_ERROR "Python include directory wasn't found.
        Verify correctness of APPLE_LLDB_PYTHON3_INCLUDE_DIR variable")

        message("Python include directory: \"${APPLE_LLDB_PYTHON3_INCLUDE_DIR}\"")
    endif()

    if((NOT EXISTS ${APPLE_LLDB_PYTHON3_LIBRARY}) AND (NOT EXISTS ${APPLE_LLDB_PYTHON3_FRAMEWORK_DIR}))
        message(SEND_ERROR "Neither python library file nor framework directory weren't found.
        Verify correctness of APPLE_LLDB_PYTHON3_LIBRARY variable")

        message("Python library file: \"${APPLE_LLDB_PYTHON3_LIBRARY}\"")
        message("Python framework directory: \"${APPLE_LLDB_PYTHON3_FRAMEWORK_DIR}\"")
    endif()

    # Set python paths in the same way as find_package does.
    set(PYTHON_INCLUDE_DIR ${APPLE_LLDB_PYTHON3_INCLUDE_DIR})
    set(PYTHON_LIBRARY ${APPLE_LLDB_PYTHON3_LIBRARY})
    set(PYTHON_FRAMEWORK ${APPLE_LLDB_PYTHON3_FRAMEWORK_DIR})
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/../oidbridge.cmake)
