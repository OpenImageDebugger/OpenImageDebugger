include($$PWD/../../common.pri)

CONFIG += $$BUILD_MODE console

QT += \
  core

SOURCES += \
  ../giw_bridge.cpp \
  ../../debuggerinterface/python_native_interface.cpp \
  ../../debuggerinterface/buffer_request_message.cpp

# Qt related headers
HEADERS += \
  ../../debuggerinterface/preprocessor_directives.h \
  ../../debuggerinterface/python_native_interface.h 
  ../../debuggerinterface/buffer_request_message.h

# Prevent strip from producing spurious error messages
QMAKE_STRIP = echo "strip disabled: "

TEMPLATE = lib

linux-g++ {
    QMAKE_LFLAGS += -Wl,--exclude-libs,ALL
}

INCLUDEPATH += \
  $$PWD/../
