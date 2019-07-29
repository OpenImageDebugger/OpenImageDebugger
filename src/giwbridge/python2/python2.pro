include($$PWD/../giwbridge.pri)

PKGCONFIG += python2

message(GDB-ImageWatch Python2 Bridge build mode: $$BUILD_MODE)

TARGET = giwbridge_python2
target.path = $$PREFIX/bin/gdb-imagewatch/

INSTALLS += \
  target

