QT += core gui opengl widgets gui

TARGET = gdb-imagewatch
TEMPLATE = lib

QMAKE_CXXFLAGS += -fPIC -pthread

SOURCES += src/camera.cpp\
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
    src/game_object.cpp

required_resources.path = $$OUT_PWD
required_resources.files = resources/serif.ttf \
                           resources/gdb-imagewatch.py \
                           resources/gdbiwtype.py \
                           resources/__init__.py \
                           resources/giw_load.m

INSTALLS += required_resources

CONFIG += link_pkgconfig \
          c++11 \
          no_keywords

#QMAKE_LFLAGS += -Xlinker -Bstatic

DEFINES += "FONT_PATH=\\\"$$OUT_PWD/serif.ttf\\\""

PKGCONFIG += freetype2 \
             python3 \
             glew

HEADERS  += src/buffer.hpp \
            src/buffer_values.hpp \
            src/camera.hpp \
            src/component.hpp \
            src/math.hpp \
            src/shader.hpp \
            src/glcanvas.hpp \
            src/shaders/imagewatch_shaders.hpp \
            src/mainwindow.h \
            src/stage.hpp \
    src/buffer_exporter.hpp \
    src/game_object.h

FORMS    += ui/mainwindow.ui
