// Standard headers
#include <stdio.h>
#include <stdlib.h>
#include <limits>

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
    void showBin( PyObject* pybuffer, int buffer_width_i, int buffer_height_i, int channels, int type );
}

void showBin( PyObject* pybuffer, int buffer_width_i, int buffer_height_i, int channels, int type )
{
    uint8_t* buffer = reinterpret_cast<uint8_t*>(PyMemoryView_GET_BUFFER(pybuffer)->buf);

    Window window;
    if(!window.create("GDB-ImageWatch", 3,2, buffer, buffer_width_i,
                buffer_height_i, channels, type)) { // OpenGL version 3.2
        return;
    }
    window.printGLInfo();
    window.run();
}

