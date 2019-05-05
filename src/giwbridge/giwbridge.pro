include($$PWD/../../common.pri)

CONFIG += $$BUILD_MODE
message(GDB-ImageWatch Bridge build mode: $$BUILD_MODE)

# Prevent strip from producing spurious error messages
QMAKE_STRIP = echo "strip disabled: "

QT += \
  core

linux-g++ {
    QMAKE_LFLAGS += -Wl,--exclude-libs,ALL
}

SOURCES += \
  giw_bridge.cpp \
  ../debuggerinterface/python_native_interface.cpp \
  ../debuggerinterface/buffer_request_message.cpp

# Qt related headers
HEADERS += \
  ../debuggerinterface/preprocessor_directives.h \
  ../debuggerinterface/python_native_interface.h 
  ../debuggerinterface/buffer_request_message.h

TARGET = giwbridge
TEMPLATE = lib
target.path = $$PREFIX/bin/gdb-imagewatch/

INSTALLS += \
  target

# Assorted configuration
INCLUDEPATH += \
  $$PWD/../

CONFIG += console
