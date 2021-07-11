# !!! qmake build is deprecated and will be removed soon !!!
TEMPLATE = subdirs
SUBDIRS = src src/oidbridge/python2 src/oidbridge/python3

# Currently we do not support python3 on Windows
win32:SUBDIRS -= src/oidbridge/python3
