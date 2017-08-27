# TODO config as release by default (so the user will know how to change to debug)
QT += core gui opengl widgets gui

TARGET = gdb-imagewatch
TEMPLATE = lib

QMAKE_CXXFLAGS += -fPIC -pthread

# TODO sort alphabetically
SOURCES += \
  src/camera.cpp\
  src/main.cpp \
  src/buffer_values.cpp \
  src/buffer.cpp \
  src/shaders/buff_frag_shader.cpp \
  src/shaders/buff_vert_shader.cpp \
  src/shaders/text_frag_shader.cpp \
  src/shaders/text_vert_shader.cpp \
  src/shader.cpp \
  src/mainwindow.cpp \
  src/glcanvas.cpp \
  src/stage.cpp \
  src/buffer_exporter.cpp \
  src/math.cpp \
  src/game_object.cpp \
  src/managed_pointer.cpp \
  src/background.cpp \
  src/shaders/background_frag_shader.cpp \
  src/shaders/background_vert_shader.cpp \
  src/symbol_search_input.cpp \
  src/symbol_completer.cpp \
  src/bufferrequestmessage.cpp

# TODO gerar .os em outro lugar (ou limpar eles no caso do make install)

# Prevent qmake from stripping the required resources, which gives a spurious
# error message
QMAKE_STRIP = echo

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

HEADERS  += \
  src/background.hpp \
  src/buffer.hpp \
  src/buffer_exporter.hpp \
  src/buffer_values.hpp \
  src/bufferrequestmessage.h \
  src/camera.hpp \
  src/component.hpp \
  src/game_object.h \
  src/glcanvas.hpp \
  src/mainwindow.h \
  src/managed_pointer.h \
  src/math.hpp \
  src/shader.hpp \
  src/stage.hpp \
  src/symbol_completer.h \
  src/symbol_search_input.h \
  src/shaders/imagewatch_shaders.hpp

FORMS += ui/mainwindow.ui

OTHER_FILES += \
  resources/icons/arrow-down-b.png \
  resources/icons/arrow-shrink.png \
  resources/icons/contrast.png \
  resources/icons/link.png \
  resources/icons/rotate-ccw.png \
  resources/icons/rotate-cw.png

RESOURCES += \
  resources/resources.qrc
