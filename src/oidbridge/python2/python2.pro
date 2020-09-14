include($$PWD/../oidbridge.pri)

# For some reason, on Focal, pkg-config does not find python2 libs by default
linux {
    PKG_CONFIG_PATH += /usr/lib/x86_64-linux-gnu/pkgconfig/
}

macx {
    PKG_CONFIG_PATH=/System/Library/Frameworks/Python.framework/Versions/2.7/lib/pkgconfig/
}

PKGCONFIG += python-2.7

message(OpenImageDebugger Python2 Bridge build mode: $$BUILD_MODE)

TARGET = oidbridge_python2
target.path = $$BRIDGE_INSTALL_FOLDER

INSTALLS += \
  target \
  $$SCRIPT_INSTALLS
