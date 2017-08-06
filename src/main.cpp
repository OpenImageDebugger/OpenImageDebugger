// Standard headers
#include <stdio.h>
#include <stdlib.h>
#include <limits>
#include <memory>
#include <unistd.h>
#include <csignal>

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
    void initialize_window(int(*plot_callback)(const char*));
    void terminate();
    bool is_running();
    void update_available_variables(PyObject* available_set);
    void plot_binary(PyObject* pybuffer,
                     PyObject* var_name,
                     int buffer_width_i,
                     int buffer_height_i,
                     int channels,
                     int type,
                     int step,
                     PyObject* pixel_layout);
}

MainWindow* wnd = nullptr;
bool is_running_ = false;

bool is_running() {
    return is_running_;
}

void update_available_variables(PyObject* available_set) {
    wnd->update_available_variables(available_set);
}

void plot_binary(PyObject* pybuffer,
                 PyObject* var_name,
                 int buffer_width_i,
                 int buffer_height_i,
                 int channels,
                 int type,
                 int step,
                 PyObject* pixel_layout)
{
    PyObject *var_name_bytes = PyUnicode_AsEncodedString(var_name,
                                                         "ASCII",
                                                         "strict");
    PyObject *pixel_layout_bytes = PyUnicode_AsEncodedString(pixel_layout,
                                                             "ASCII",
                                                             "strict");

    BufferRequestMessage request;
    request.var_name_str = PyBytes_AS_STRING(var_name_bytes);
    request.py_buffer = pybuffer;
    request.width_i = buffer_width_i;
    request.height_i = buffer_height_i;
    request.channels = channels;
    request.type = static_cast<Buffer::BufferType>(type);
    request.step = step;
    request.pixel_layout = PyBytes_AS_STRING(pixel_layout_bytes);

    while(wnd == nullptr) {
        usleep(1e6 / 30);
    }

    wnd->plot_buffer(request);
}

void signalHandler( int signum )
{
#ifndef NDEBUG
    cout << "[gdb-imagewatch] SIGNAL (" << signum << ") received.\n";
#endif
}

void initialize_window(int(*plot_callback)(const char*)) {

    const char* argv[] = { "GDB ImageWatch", NULL };
    int argc = 1;
    sighandler_t gdb_sigchld_handler = std::signal(SIGCHLD, signalHandler);

    QApplication app(argc, const_cast<char**>(&argv[0]));
    MainWindow window;
    window.show();
    window.set_plot_callback(plot_callback);
    wnd = &window;
    is_running_ = true;

    // Restore GDB SIGCHLD handler
    std::signal(SIGCHLD, gdb_sigchld_handler);

    app.exec();

    wnd = nullptr;
    is_running_ = false;
}

void terminate() {
    QApplication::exit();
}
