include($$PWD/../../common.pri)

CONFIG += $$BUILD_MODE console

QT += \
  core

QT -= \
  gui

SOURCES += \
  ../oid_bridge.cpp \
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

# Deployment settings
macx {
    BRIDGE_INSTALL_FOLDER = $$PREFIX/OpenImageDebugger/oidwindow.app/Contents/MacOS
}
linux {
    BRIDGE_INSTALL_FOLDER = $$PREFIX/OpenImageDebugger/
}

include($$PWD/../../resources/deployscripts.pri)

PKG_CONFIG_PATH += /usr/lib/x86_64-linux-gnu/pkgconfig/
