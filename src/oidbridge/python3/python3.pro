include($$PWD/../oidbridge.pri)

PKGCONFIG += python3

message(OpenImageDebugger Python3 Bridge build mode: $$BUILD_MODE)

TARGET = oidbridge_python3
target.path = $$BRIDGE_INSTALL_FOLDER

INSTALLS += \
  target \
  $$SCRIPT_INSTALLS
