message(GDB-ImageWatch Resources)

QMAKE_STRIP = echo "strip disabled: "

# Copy resource files to build folder
copydata.commands = \
  $(COPY_DIR) \"$$shell_path($$PWD\\giwscripts)\" \"$$shell_path($$OUT_PWD)\"; \
  $(COPY_DIR) \"$$shell_path($$PWD\\matlab)\" \"$$shell_path($$OUT_PWD)\"; \
  $(COPY_FILE) \"$$shell_path($$PWD\\gdb_imagewatch.py)\" \"$$shell_path($$OUT_PWD)\"

first.depends = $(first) copydata
export(first.depends)
export(copydata.commands)
QMAKE_EXTRA_TARGETS += first copydata

install_fonts.files = fonts/*
install_matlab_scripts.files = matlab/*
install_debugger_scripts.files = \
  gdb_imagewatch.py \
  giwscripts

macx {
    install_fonts.path = $$PREFIX/gdb-imagewatch/giwwindow.app/Contents/MacOS/fonts/
    install_matlab_scripts.path = $$PREFIX/gdb-imagewatch/giwwindow.app/Contents/MacOS/matlab/
    install_debugger_scripts.path = $$PREFIX/gdb-imagewatch/giwwindow.app/Contents/MacOS/
}

linux {
    install_fonts.path = $$PREFIX/gdb-imagewatch/fonts/
    install_matlab_scripts.path = $$PREFIX/gdb-imagewatch/matlab/
    install_debugger_scripts.path = $$PREFIX/gdb-imagewatch/
}

QMAKE_BUNDLE_DATA += \
  install_fonts \
  install_matlab_scripts \
  install_debugger_scripts

SCRIPT_INSTALLS = \
  install_fonts \
  install_matlab_scripts \
  install_debugger_scripts
