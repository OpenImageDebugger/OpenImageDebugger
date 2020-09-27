cmake_minimum_required(VERSION 3.10.0)

message(Open Image Debugger Resources)

# Copy resource files to build folder
set(MATLAB_SCRIPTS ${CMAKE_CURRENT_SOURCE_DIR}/../../../resources/matlab)
set(DEBUGGER_SCRIPTS ${CMAKE_CURRENT_SOURCE_DIR}/../../../resources/oidscripts)
set(OID_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/../../../resources/oid.py)

install(DIRECTORY ${MATLAB_SCRIPTS} ${DEBUGGER_SCRIPTS}
        DESTINATION OpenImageDebugger)

install(PROGRAMS ${OID_SCRIPT}
        DESTINATION OpenImageDebugger)

# TODO: deal with macos
