// Standard headers
#include <stdio.h>
#include <stdlib.h>
#include <limits>
#include <memory>
#include <unistd.h>

// Qt headers
#include <QApplication>

// OpenGL related headers
#include <GL/glew.h>

// Internal headers
#include "math.hpp"
#include "shader.hpp"
#include "mainwindow.h"


using namespace std;

extern "C" {
    void initialize_window();
    void terminate();
    void plot_binary( PyObject* pybuffer, PyObject* var_name, int buffer_width_i,
            int buffer_height_i, int channels, int type, int step);
    void update_plot( PyObject* pybuffer, PyObject* var_name, int buffer_width_i,
            int buffer_height_i, int channels, int type, int step);
}

MainWindow* wnd;

void update_plot( PyObject* pybuffer, PyObject* var_name, int buffer_width_i,
        int buffer_height_i, int channels, int type, int step )
{
    /*
    PyObject *var_name_bytes = PyUnicode_AsEncodedString(var_name, "ASCII", "strict");
    string var_name_str = PyBytes_AS_STRING(var_name_bytes);
    auto wnd_it = windows.find(var_name_str);

    if(wnd_it != windows.end()) {
        Window* window = wnd_it->second.get();
        window->buffer_update(pybuffer, var_name_str, buffer_width_i, buffer_height_i,
                channels, type, step);
    }
    */
}

void plot_binary( PyObject* pybuffer, PyObject* var_name, int buffer_width_i,
        int buffer_height_i, int channels, int type, int step)
{
    PyObject *var_name_bytes = PyUnicode_AsEncodedString(var_name, "ASCII", "strict");
    string var_name_str = PyBytes_AS_STRING(var_name_bytes);

    BufferRequestMessage request;
    request.var_name_str = var_name_str;
    request.py_buffer = pybuffer;
    request.width_i = buffer_width_i;
    request.height_i = buffer_height_i;
    request.channels = channels;
    request.type = type;
    request.step = step;

    while(wnd == nullptr)
        usleep(1/30*1e6);

    wnd->plot_buffer(request);
}

void initialize_window() {
    char* argv[] = { "GDB ImageWatch", NULL };
    int argc = 1;

    QApplication app(argc, &argv[0]);
    MainWindow window;
    window.show();
    wnd = &window;
    app.exec();
}

void terminate() {
    QApplication::exit();
}
