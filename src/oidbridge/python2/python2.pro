include($$PWD/../oidbridge.pri)

# For some reason, on Focal, pkg-config does not find python2 libs by default
linux:PKG_CONFIG_PATH += /usr/lib/x86_64-linux-gnu/pkgconfig/

macx:PKG_CONFIG_PATH = /System/Library/Frameworks/Python.framework/Versions/2.7/lib/pkgconfig/

linux|macx:PKGCONFIG += python-2.7

win32 {
    # Poor man's findpackage(PythonLibs)

    # Use Windows' where instead of issue because of path issues
    PYTHON_BIN = $$system(where python)
    PYTHON_DIR = $$system(dirname $$PYTHON_BIN)

    PYTHON_LIB = $$system_path($$PYTHON_DIR/libpython2.7.dll)
    PYTHON_INCLUDE = $$clean_path($$PYTHON_DIR/../include/python2.7)

    INCLUDEPATH += $$PYTHON_INCLUDE
    LIBS += $$PYTHON_LIB
}

TARGET = oidbridge_python2
target.path = $$BRIDGE_INSTALL_FOLDER

INSTALLS += \
  target \
  $$SCRIPT_INSTALLS
