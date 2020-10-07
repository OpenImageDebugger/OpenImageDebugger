include($$PWD/../../common.pri)

# 'plugin' is used to generate only the .so/.dll file
CONFIG += $$BUILD_MODE console plugin

QT += \
  core

QT -= \
  gui

SOURCES += \
  ../oid_bridge.cpp \
  ../../debuggerinterface/python_native_interface.cpp \
  ../../ipc/message_exchange.cpp \
  ../../ipc/raw_data_decode.cpp \
  ../../system/process/process.cpp \
  ../../system/process/process_unix.cpp \
  ../../system/process/process_win32.cpp \

# Qt related headers
HEADERS += \
  ../../debuggerinterface/preprocessor_directives.h \
  ../../debuggerinterface/python_native_interface.h \
  ../../ipc/message_exchange.h \
  ../../system/process/process.h \
  ../../system/process/process_impl.h

# Prevent strip from producing spurious error messages
QMAKE_STRIP = echo "strip disabled: "

TEMPLATE = lib

linux-g++:QMAKE_LFLAGS += -Wl,--exclude-libs,ALL

INCLUDEPATH += \
  $$PWD/../

# Deployment settings
macx:BRIDGE_INSTALL_FOLDER = $$PREFIX/OpenImageDebugger/oidwindow.app/Contents/MacOS

linux|win32:BRIDGE_INSTALL_FOLDER = $$PREFIX/OpenImageDebugger/

include($$PWD/../../resources/deployscripts.pri)
