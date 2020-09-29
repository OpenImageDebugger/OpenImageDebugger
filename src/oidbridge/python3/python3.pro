include($$PWD/../oidbridge.pri)

PKGCONFIG += python3

TARGET = oidbridge_python3
# Add the 'lib' prefix to the library when compiling with mingw-g++ on Windows
win32-g++:TARGET = lib$$TARGET

target.path = $$BRIDGE_INSTALL_FOLDER

INSTALLS += \
  target \
  $$SCRIPT_INSTALLS
