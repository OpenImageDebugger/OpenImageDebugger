include($$PWD/../oidbridge.pri)

PKGCONFIG += python3

TARGET = oidbridge_python3
target.path = $$BRIDGE_INSTALL_FOLDER

INSTALLS += \
  target \
  $$SCRIPT_INSTALLS
