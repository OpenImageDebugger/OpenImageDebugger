// Standard headers
#include <stdio.h>
#include <stdlib.h>
#include <limits>
#include <sstream>
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
    auto wnd_it = windows.find(PyBytes_AS_STRING(var_name_bytes));

    if(wnd_it != windows.end()) {
        uint8_t* buffer = reinterpret_cast<uint8_t*>(PyMemoryView_GET_BUFFER(pybuffer)->buf);

        Window* window = wnd_it->second.get();
        window->buffer_update(pybuffer, buffer_width_i, buffer_height_i, channels, type);
    }
}

void plot_binary( PyObject* pybuffer, PyObject* var_name, int buffer_width_i,
        int buffer_height_i, int channels, int type )
{
    PyObject *var_name_bytes = PyUnicode_AsEncodedString(var_name, "ASCII", "strict");

    shared_ptr<Window> window = make_shared<Window>();
    stringstream window_name;
    window_name << "[GDB-ImageWatch] " << PyBytes_AS_STRING(var_name_bytes) <<
        " (" << buffer_width_i << "x" << buffer_height_i << "/";

    if(type == 0)
        window_name << "float32";
    else if(type==1)
        window_name << "uint8";

    window_name << "/";

    if (channels==1)
        window_name << "1 channel)";
    else
        window_name << channels << " channels)";

    if(!window->create(window_name.str().c_str(), 3,2, pybuffer, buffer_width_i,
                buffer_height_i, channels, type)) { // OpenGL version 3.2
        return;
    }
    window->printGLInfo();
    windows[PyBytes_AS_STRING(var_name_bytes)] = window;
    window->run();
    windows.erase(PyBytes_AS_STRING(var_name_bytes));
}

