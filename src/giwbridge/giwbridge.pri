include($$PWD/../../common.pri)

CONFIG += $$BUILD_MODE console

QT += \
  core

SOURCES += \
  ../giw_bridge.cpp \
  ../../debuggerinterface/python_native_interface.cpp \
  ../../ipc/message_exchange.cpp \
  ../../ipc/raw_data_decode.cpp

# Qt related headers
HEADERS += \
  ../../debuggerinterface/preprocessor_directives.h \
  ../../debuggerinterface/python_native_interface.h \
  ../../ipc/message_exchange.h

# Prevent strip from producing spurious error messages
QMAKE_STRIP = echo "strip disabled: "

TEMPLATE = lib

linux-g++ {
    QMAKE_LFLAGS += -Wl,--exclude-libs,ALL
}

INCLUDEPATH += \
  $$PWD/../
