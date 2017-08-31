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

// TODO move to PNI.h
#define FALSE 0
#define TRUE (!FALSE)

extern "C" {
    void initialize_window(int(*plot_callback)(const char*));
    void terminate();
    int is_running();
    void update_available_variables(PyObject* available_set);
    void plot_binary(PyObject* bufffer_metadata);
}

MainWindow* wnd = nullptr;
int is_running_ = FALSE;

int is_running() {
    return is_running_;
}

void update_available_variables(PyObject* available_set) {
    wnd->update_available_variables(available_set);
}

void plot_binary(PyObject* buffer_metadata)
{
    int attempt_counter = 30;
    while(wnd == nullptr &&
          attempt_counter-- > 0 ) {
        usleep(1e6 / 30);
    }

    wnd->plot_buffer(buffer_metadata);
}

void signalHandler(int signum )
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
    is_running_ = TRUE;

    // Restore GDB SIGCHLD handler
    std::signal(SIGCHLD, gdb_sigchld_handler);

    app.exec();

    wnd = nullptr;
    is_running_ = FALSE;
}

void terminate() {
    QApplication::exit();
}
