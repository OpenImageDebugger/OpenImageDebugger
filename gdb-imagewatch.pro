# BUILD_MODE variable can be either release or debug
isEmpty(BUILD_MODE) {
  BUILD_MODE = release
}

message(GDB-ImageWatch build mode: $$BUILD_MODE)

CONFIG += $$BUILD_MODE

# Prevent strip from producing spurious error messages
QMAKE_STRIP = echo "strip disabled: "

QT += \
  core \
  gui \
  opengl \
  widgets \
  gui

QMAKE_CXXFLAGS += \
  -fPIC \
  -fvisibility=hidden \
  -pthread

QMAKE_LFLAGS += \
  -Wl,--exclude-libs,ALL

SOURCES += \
  src/giw_window.cpp \
  src/debuggerinterface/buffer_request_message.cpp \
  src/debuggerinterface/managed_pointer.cpp \
  src/debuggerinterface/python_native_interface.cpp \
  src/io/buffer_exporter.cpp \
  src/math/assorted.cpp \
  src/math/linear_algebra.cpp \
  src/ui/gl_canvas.cpp \
  src/ui/symbol_completer.cpp \
  src/ui/symbol_search_input.cpp \
  src/ui/main_window/main_window.cpp \
  src/ui/main_window/initialization.cpp \
  src/ui/main_window/auto_contrast.cpp \
  src/ui/main_window/ui_events.cpp \
  src/visualization/game_object.cpp \
  src/visualization/shader.cpp \
  src/visualization/stage.cpp \
  src/visualization/components/background.cpp \
  src/visualization/components/buffer.cpp \
  src/visualization/components/buffer_values.cpp \
  src/visualization/components/camera.cpp\
  src/visualization/components/component.cpp\
  src/visualization/shaders/background_fs.cpp \
  src/visualization/shaders/background_vs.cpp \
  src/visualization/shaders/buffer_fs.cpp \
  src/visualization/shaders/buffer_vs.cpp \
  src/visualization/shaders/text_fs.cpp \
  src/visualization/shaders/text_vs.cpp \
  src/ui/gl_text_renderer.cpp

# Qt related headers
HEADERS += \
  src/debuggerinterface/preprocessor_directives.h \
  src/ui/gl_canvas.h \
  src/ui/main_window/main_window.h \
  src/ui/symbol_completer.h \
  src/ui/symbol_search_input.h \
  src/ui/gl_text_renderer.h

# Copy resource files to build folder
copydata.commands = \
  $(COPY_DIR) \"$$shell_path($$PWD\\resources\\giwscripts)\" \"$$shell_path($$OUT_PWD)\"; \
  $(COPY_DIR) \"$$shell_path($$PWD\\resources\\matlab)\" \"$$shell_path($$OUT_PWD)\"; \
  $(COPY_FILE) \"$$shell_path($$PWD\\resources\\gdb-imagewatch.py)\" \"$$shell_path($$OUT_PWD)\"

first.depends = $(first) copydata
export(first.depends)
export(copydata.commands)
QMAKE_EXTRA_TARGETS += first copydata

# Instalation instructions
isEmpty(PREFIX) {
  PREFIX = /usr/local
}

VERSION = 1.2
TARGET = giwwindow
TEMPLATE = lib
target.path = $$PREFIX/bin/gdb-imagewatch/

install_debugger_scripts.path = $$PREFIX/bin/gdb-imagewatch/
install_debugger_scripts.files = \
  resources/gdb-imagewatch.py \
  resources/giwscripts

install_fonts.path = $$PREFIX/bin/gdb-imagewatch/fonts/
install_fonts.files = resources/fonts/*

install_matlab_scripts.path = $$PREFIX/bin/gdb-imagewatch/matlab
install_matlab_scripts.files = resources/matlab/*

INSTALLS += \
  install_fonts \
  install_matlab_scripts \
  install_debugger_scripts \
  target

# Assorted configuration
INCLUDEPATH += $$PWD/src

CONFIG += \
  link_pkgconfig \
  warn_on \
  c++11 \
  no_keywords

PKGCONFIG += \
  python3

FORMS += ui/main_window.ui

OTHER_FILES += \
  resources/icons/arrow-down-b.png \
  resources/icons/arrow-shrink.png \
  resources/icons/contrast.png \
  resources/icons/link.png \
  resources/icons/rotate-ccw.png \
  resources/icons/rotate-cw.png

RESOURCES += \
  resources/resources.qrc

