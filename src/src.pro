include($$PWD/../common.pri)

message(OpenImageDebugger UI build mode: $$BUILD_MODE)

# Prevent strip from producing spurious error messages
QMAKE_STRIP = echo "strip disabled: "

QT += \
  core \
  gui \
  opengl \
  widgets \
  gui

QMAKE_LFLAGS += \
  # If you have an error "cannot find -lGL", uncomment the following line and
  # replace the folder by the location of your libGL.so
  #-L/path/to/your/opengl/folder

# Windows needs to be explicitly linked to opengl
win32:LIBS += -lopengl32

linux-g++:QMAKE_LFLAGS += -Wl,--exclude-libs,ALL

SOURCES += \
  oid_window.cpp \
  io/buffer_exporter.cpp \
  ipc/message_exchange.cpp \
  ipc/raw_data_decode.cpp \
  math/assorted.cpp \
  math/linear_algebra.cpp \
  ui/decorated_line_edit.cpp \
  ui/gl_canvas.cpp \
  ui/gl_text_renderer.cpp \
  ui/go_to_widget.cpp \
  ui/main_window/auto_contrast.cpp \
  ui/main_window/initialization.cpp \
  ui/main_window/main_window.cpp \
  ui/main_window/message_processing.cpp \
  ui/main_window/ui_events.cpp \
  ui/symbol_completer.cpp \
  ui/symbol_search_input.cpp \
  visualization/components/background.cpp \
  visualization/components/buffer.cpp \
  visualization/components/buffer_values.cpp \
  visualization/components/camera.cpp\
  visualization/components/component.cpp\
  visualization/events.cpp \
  visualization/game_object.cpp \
  visualization/shader.cpp \
  visualization/shaders/background_fs.cpp \
  visualization/shaders/background_vs.cpp \
  visualization/shaders/buffer_fs.cpp \
  visualization/shaders/buffer_vs.cpp \
  visualization/shaders/text_fs.cpp \
  visualization/shaders/text_vs.cpp \
  visualization/stage.cpp \

# Qt related headers
HEADERS += \
  debuggerinterface/preprocessor_directives.h \
  ui/decorated_line_edit.h \
  ui/gl_canvas.h \
  ui/gl_text_renderer.h \
  ui/go_to_widget.h \
  ui/main_window/main_window.h \
  ui/symbol_completer.h \
  ui/symbol_search_input.h \

TARGET = oidwindow
TEMPLATE = app
target.path = $$PREFIX/OpenImageDebugger/

INSTALLS += \
  target

# Assorted configuration
INCLUDEPATH += \
  $$PWD \
  $$PWD/thirdparty/Eigen/ \
  $$PWD/thirdparty/Khronos/

FORMS += ui/main_window/main_window.ui

RESOURCES += resources/resources.qrc

