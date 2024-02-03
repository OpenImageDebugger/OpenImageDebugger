cmake_minimum_required(VERSION 3.10.0)

# Copy resource files to build folder
set(MATLAB_SCRIPTS ${CMAKE_CURRENT_SOURCE_DIR}/../../resources/matlab)
set(DEBUGGER_SCRIPTS ${CMAKE_CURRENT_SOURCE_DIR}/../../resources/oidscripts)
set(OID_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/../../resources/oid.py)

install(DIRECTORY ${MATLAB_SCRIPTS} ${DEBUGGER_SCRIPTS}
        DESTINATION OpenImageDebugger
        FILES_MATCHING PATTERN "*pycache*" EXCLUDE
                       PATTERN "*.m"
                       PATTERN "*.py")

install(PROGRAMS ${OID_SCRIPT}
        DESTINATION OpenImageDebugger)

