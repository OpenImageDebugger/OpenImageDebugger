# TODO config as release by default (so the user will know how to change to debug)
QT += \
  core \
  gui \
  opengl \
  widgets \
  gui

TARGET = giwwindow
TEMPLATE = lib

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
  src/visualization/shaders/text_vs.cpp

# Qt related headers
HEADERS += \
  src/debuggerinterface/preprocessor_directives.h \
  src/ui/gl_canvas.h \
  src/ui/main_window/main_window.h \
  src/ui/symbol_completer.h \
  src/ui/symbol_search_input.h

# TODO gerar .os em outro lugar (ou limpar eles no caso do make install)

# Copy resource files to build folder
copydata.commands = \
  $(COPY_DIR) \"$$shell_path($$PWD\\resources\\giwscripts)\" \"$$shell_path($$OUT_PWD)\"; \
  $(COPY_DIR) \"$$shell_path($$PWD\\resources\\fonts)\" \"$$shell_path($$OUT_PWD)\"; \
  $(COPY_DIR) \"$$shell_path($$PWD\\resources\\matlab)\" \"$$shell_path($$OUT_PWD)\"; \
  $(COPY_FILE) \"$$shell_path($$PWD\\resources\\gdb-imagewatch.py)\" \"$$shell_path($$OUT_PWD)\"

# TODO check about these commands, do I need them all? https://dragly.org/2013/11/05/copying-data-files-to-the-build-directory-when-working-with-qmake/
first.depends = $(first) copydata
export(first.depends)
export(copydata.commands)
QMAKE_EXTRA_TARGETS += first copydata

INCLUDEPATH += $$PWD/src

CONFIG += \
  link_pkgconfig \
  c++11 \
  no_keywords

# TODO ou usa isso ou nao
#QMAKE_LFLAGS += -Xlinker -Bstatic

DEFINES += "FONT_PATH=\\\"$$OUT_PWD/fonts/serif.ttf\\\""

PKGCONFIG += \
  freetype2 \
  python3 \
  glew

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
