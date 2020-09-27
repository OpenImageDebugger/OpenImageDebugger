cmake_minimum_required(VERSION 3.10.0)

message(Open Image Debugger Resources)

# Copy resource files to build folder
set(MATLAB_SCRIPTS ${CMAKE_CURRENT_SOURCE_DIR}/matlab)
set(DEBUGGER_SCRIPTS ${CMAKE_CURRENT_SOURCE_DIR}/oidscripts)
set(OID_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/oid.py)

install(DIRECTORY ${MATLAB_SCRIPTS} ${DEBUGGER_SCRIPTS}
        DESTINATION ${CMAKE_INSTALL_PREFIX}/OpenImageDebugger)

# TODO: deal with macos
# macx {
#     install_matlab_scripts.path = $$PREFIX/OpenImageDebugger/oidwindow.app/Contents/MacOS/
#     install_debugger_scripts.path = $$PREFIX/OpenImageDebugger/oidwindow.app/Contents/MacOS/
# }
