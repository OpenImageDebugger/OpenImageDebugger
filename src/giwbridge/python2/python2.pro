include($$PWD/../giwbridge.pri)

# On MacOS, run the following before qmake:
# export PKG_CONFIG_PATH=/System/Library/Frameworks/Python.framework/Versions/2.7/lib/pkgconfig/
PKGCONFIG += python2

message(GDB-ImageWatch Python2 Bridge build mode: $$BUILD_MODE)

TARGET = giwbridge_python2
target.path = $$PREFIX/bin/gdb-imagewatch/

INSTALLS += \
  target

