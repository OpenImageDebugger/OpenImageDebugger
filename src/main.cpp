// Standard headers
#include <stdio.h>
#include <stdlib.h>
#include <limits>
#include <memory>

// OpenGL related headers
#include <GL/glew.h>
#define GLFW_INCLUDE_GL3
#define GLFW_NO_GLU
#include <GLFW/glfw3.h>

// Internal headers
#include "math.hpp"
#include "shader.hpp"
#include "window.hpp"

using namespace std;

extern "C" {
    void plot_binary( PyObject* pybuffer, PyObject* var_name, int buffer_width_i, int buffer_height_i, int channels, int type );
    void update_plot( PyObject* pybuffer, PyObject* var_name, int buffer_width_i, int buffer_height_i, int channels, int type );
}

map<string, shared_ptr<Window>> windows;
void update_plot( PyObject* pybuffer, PyObject* var_name, int buffer_width_i,
        int buffer_height_i, int channels, int type )
{
    PyObject *var_name_bytes = PyUnicode_AsEncodedString(var_name, "ASCII", "strict");
    string var_name_str = PyBytes_AS_STRING(var_name_bytes);
    auto wnd_it = windows.find(var_name_str);

    if(wnd_it != windows.end()) {
        uint8_t* buffer = reinterpret_cast<uint8_t*>(PyMemoryView_GET_BUFFER(pybuffer)->buf);

        Window* window = wnd_it->second.get();
        window->buffer_update(pybuffer, var_name_str, buffer_width_i, buffer_height_i, channels, type);
    }
}

void plot_binary( PyObject* pybuffer, PyObject* var_name, int buffer_width_i,
        int buffer_height_i, int channels, int type )
{
    PyObject *var_name_bytes = PyUnicode_AsEncodedString(var_name, "ASCII", "strict");
    string var_name_str = PyBytes_AS_STRING(var_name_bytes);

    shared_ptr<Window> window = make_shared<Window>();
    if(!window->create(var_name_str, 3,2, pybuffer, buffer_width_i,
                buffer_height_i, channels, type)) { // OpenGL version 3.2
        return;
    }
    window->printGLInfo();
    windows[var_name_str] = window;
    window->run();
    windows.erase(var_name_str);
}

