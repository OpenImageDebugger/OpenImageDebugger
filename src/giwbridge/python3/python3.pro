include($$PWD/../giwbridge.pri)

PKGCONFIG += python3

message(GDB-ImageWatch Python3 Bridge build mode: $$BUILD_MODE)

TARGET = giwbridge_python3
target.path = $$PREFIX/bin/gdb-imagewatch/

INSTALLS += \
  target
