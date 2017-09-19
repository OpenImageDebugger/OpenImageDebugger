/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 GDB ImageWatch (github.com/csantosbh/gdb-imagewatch/)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "giw_window.h"

#include <csignal>

#include <string>

#include <QApplication>

#include "debuggerinterface/python_native_interface.h"
#include "pythoninterface/preprocessor_directives.h"
#include "ui/main_window.h"


static char giw_app_argv0[] = "GDB-ImageWatch";
static char* giw_app_argv[] = {giw_app_argv0};
static int giw_app_argc = 1;


void dummy_sgn_handler(int signum)
{
#ifndef NDEBUG
    cout << "[gdb-imagewatch] SIGNAL (" << signum << ") received.\n";
#endif
}


AppHandler giw_initialize()
{
    sighandler_t gdb_sigchld_handler = std::signal(SIGCHLD, dummy_sgn_handler);

    QApplication* app = new QApplication(giw_app_argc, giw_app_argv);

    // Restore GDB SIGCHLD handler
    std::signal(SIGCHLD, gdb_sigchld_handler);

    return static_cast<AppHandler>(app);
}


void giw_cleanup(AppHandler handler)
{
    QApplication* app = static_cast<QApplication*>(handler);

    if (app == nullptr) {
        RAISE_PY_EXCEPTION(PyExc_RuntimeError,
                           "giw_terminate received null application handler");
        return;
    }

    QApplication::exit();
    delete app;
}


void giw_terminate()
{
    QApplication::exit();
}


void giw_exec(AppHandler handler)
{
    QApplication* app = static_cast<QApplication*>(handler);

    if (app == nullptr) {
        RAISE_PY_EXCEPTION(PyExc_RuntimeError,
                           "giw_exec received null application handler");
        return;
    }

    app->exec();
}


WindowHandler giw_create_window(int (*plot_callback)(const char*))
{
    MainWindow* window = new MainWindow();
    window->show();
    window->set_plot_callback(plot_callback);

    return window;
}


int giw_is_window_ready(WindowHandler handler)
{
    MainWindow* window = static_cast<MainWindow*>(handler);

    return window != nullptr && (window->is_window_ready());
}


void giw_destroy_window(WindowHandler handler)
{
    MainWindow* window = static_cast<MainWindow*>(handler);

    if (window == nullptr) {
        RAISE_PY_EXCEPTION(PyExc_RuntimeError,
                           "giw_destroy_window received null window handler");
        return;
    }

    window->close();
    delete window;
}


PyObject* giw_get_observed_buffers(WindowHandler handler)
{
    MainWindow* window = static_cast<MainWindow*>(handler);

    if (window == nullptr) {
        RAISE_PY_EXCEPTION(PyExc_Exception,
                           "giw_get_observed_buffers received null window "
                           "handler");
        return nullptr;
    }

    auto observed_symbols = window->get_observed_symbols();
    PyObject* py_observed_symbols = PyList_New(observed_symbols.size());

    int observed_symbols_sentinel = static_cast<int>(observed_symbols.size());
    for (int i = 0; i < observed_symbols_sentinel; ++i) {
        string symbol_name = observed_symbols[i];
        PyObject* py_symbol_name = PyBytes_FromString(symbol_name.c_str());

        if (py_symbol_name == nullptr) {
            Py_DECREF(py_observed_symbols);
            return nullptr;
        }

        PyList_SetItem(py_observed_symbols, i, py_symbol_name);
    }

    return py_observed_symbols;
}


void giw_set_available_symbols(WindowHandler handler, PyObject* available_set)
{
    assert(PyList_Check(available_set));

    MainWindow* window = static_cast<MainWindow*>(handler);

    if (window == nullptr) {
        RAISE_PY_EXCEPTION(PyExc_RuntimeError,
                           "giw_set_available_symbols received null window "
                           "handler");
        return;
    }

    window->set_available_symbols(available_set);
}


void giw_plot_buffer(WindowHandler handler, PyObject* buffer_metadata)
{
    MainWindow* window = static_cast<MainWindow*>(handler);

    if (window == nullptr) {
        RAISE_PY_EXCEPTION(PyExc_RuntimeError,
                           "giw_plot_buffer received null window handler");
        return;
    }

    window->plot_buffer(buffer_metadata);
}
