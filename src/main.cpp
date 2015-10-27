// Standard headers
#include <stdio.h>
#include <stdlib.h>
#include <limits>
#include <sstream>

// OpenGL related headers
#include <GL/glew.h>
#define GLFW_INCLUDE_GL3
#define GLFW_NO_GLU
#include <GLFW/glfw3.h>

// Other dependencies
#include <Python.h>

// Internal headers
#include "math.hpp"
#include "shader.hpp"
#include "window.hpp"

using namespace std;

extern "C" {
    void plot_binary( PyObject* pybuffer, PyObject* var_name, int buffer_width_i, int buffer_height_i, int channels, int type );
}

void plot_binary( PyObject* pybuffer, PyObject* var_name, int buffer_width_i, int buffer_height_i, int channels, int type )
{
    uint8_t* buffer = reinterpret_cast<uint8_t*>(PyMemoryView_GET_BUFFER(pybuffer)->buf);
    PyObject *var_name_bytes = PyUnicode_AsEncodedString(var_name, "ASCII", "strict");

    Window window;
    stringstream window_name;
    window_name << "[GDB-ImageWatch] "<<PyBytes_AS_STRING(var_name_bytes)<<" (";
    if(type == 0)
        window_name << "float32";
    else if(type==1)
        window_name << "uint8";

    window_name << "/";

    if (channels==1)
        window_name << "1 channel)";
    else
        window_name << channels << " channels)";

    if(!window.create(window_name.str().c_str(), 3,2, buffer, buffer_width_i,
                buffer_height_i, channels, type)) { // OpenGL version 3.2
        return;
    }
    window.printGLInfo();
    window.run();
}

