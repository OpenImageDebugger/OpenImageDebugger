message(Open Image Debugger Resources)

QMAKE_STRIP = echo "strip disabled: "

# Copy resource files to build folder
install_matlab_scripts.files = $$system_path($$PWD/matlab)
install_debugger_scripts.files = \
  $$system_path($$PWD/oid.py) \
  $$system_path($$PWD/oidscripts)

macx {
    install_matlab_scripts.path = $$PREFIX/OpenImageDebugger/oidwindow.app/Contents/MacOS/
    install_debugger_scripts.path = $$PREFIX/OpenImageDebugger/oidwindow.app/Contents/MacOS/
}

linux|win32 {
    install_matlab_scripts.path = $$PREFIX/OpenImageDebugger/
    install_debugger_scripts.path = $$PREFIX/OpenImageDebugger/
}

install_matlab_scripts.CONFIG += no_check_exist
install_debugger_scripts.CONFIG += no_check_exist

SCRIPT_INSTALLS = \
  install_matlab_scripts \
  install_debugger_scripts
