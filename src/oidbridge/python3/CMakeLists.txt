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
        Take into consideration that your IDE may use custom lldb which differs from system-one.
        Run:
            lldb
            (lldb) script
            Python Interactive Interpreter. To exit, type 'quit()', 'exit()' or Ctrl-D.
            >>> import sys; print(sys.path)")

    # Load variables from cache. Allow user to provide paths manually.
    set(APPLE_LLDB_PYTHON3_INCLUDE_DIR "" CACHE PATH "Path to python3 include directory.
        ${APPLE_LLDB_PYTHON3_INFO}
        Examine output and insert path to include directory here.
        Several examples:
            - /Applications/Xcode.app/Contents/Developer/Library/Frameworks/Python3.framework/Versions/3.8/include/python3.8 (system-one, used by lldb command)
            - /usr/local/Frameworks/Python.framework/Versions/3.9/include/python3.9 (instaled by user)")

    set(APPLE_LLDB_PYTHON3_LIBRARY "" CACHE FILEPATH "Path to python3 library file.
        ${APPLE_LLDB_PYTHON3_INFO}
        Examine output and insert path to library file here.
        Several examples:
            - /Applications/Xcode.app/Contents/Developer/Library/Frameworks/Python3.framework/Versions/3.8/lib/libpython3.8.dylib (system-one, used by lldb command)
            - /usr/local/Frameworks/Python.framework/Versions/3.9/lib/libpython3.9.dylib (instaled by user)
            - ~/.vscode/extensions/vadimcn.vscode-lldb-1.6.10/lldb/lib/libpython39.dylib (used by CodeLLDB)")

    # Check if user has provided correct paths.
    if(NOT EXISTS ${APPLE_LLDB_PYTHON3_INCLUDE_DIR})
        message(SEND_ERROR "Python include directory wasn't found: ${APPLE_LLDB_PYTHON3_INCLUDE_DIR}
        Verify correctness of APPLE_LLDB_PYTHON3_INCLUDE_DIR variable")
    endif()

    if(NOT EXISTS ${APPLE_LLDB_PYTHON3_LIBRARY})
        message(SEND_ERROR "Python library file wasn't found: ${APPLE_LLDB_PYTHON3_LIBRARY}
        Verify correctness of APPLE_LLDB_PYTHON3_LIBRARY variable")
    endif()

    # Set python paths in the same way as find_package does.
    set(PYTHON_INCLUDE_DIR ${APPLE_LLDB_PYTHON3_INCLUDE_DIR})
    set(PYTHON_LIBRARY ${APPLE_LLDB_PYTHON3_LIBRARY})
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/../oidbridge.cmake)
