message(GDB-ImageWatch Resources)

QMAKE_STRIP = echo "strip disabled: "

# Copy resource files to build folder
install_matlab_scripts.files = $$shell_path($$PWD\\matlab)
install_debugger_scripts.files = \
  $$shell_path($$PWD\\gdb_imagewatch.py) \
  $$shell_path($$PWD\\giwscripts)

macx {
    install_matlab_scripts.path = $$PREFIX/gdb-imagewatch/giwwindow.app/Contents/MacOS/
    install_debugger_scripts.path = $$PREFIX/gdb-imagewatch/giwwindow.app/Contents/MacOS/
}

linux {
    install_matlab_scripts.path = $$PREFIX/gdb-imagewatch/
    install_debugger_scripts.path = $$PREFIX/gdb-imagewatch/
}

install_matlab_scripts.CONFIG += no_check_exist
install_debugger_scripts.CONFIG += no_check_exist

SCRIPT_INSTALLS = \
  install_matlab_scripts \
  install_debugger_scripts
