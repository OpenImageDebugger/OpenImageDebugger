#include <GL/glew.h>
#define GLFW_INCLUDE_GL3
#define GLFW_NO_GLU
#include <GLFW/glfw3.h>

#include "buffer.hpp"
#include "stage.hpp"

using namespace std;

void Buffer::update() {
    float zoom = stage->camera_component.zoom;
    if(zoom > 40) {
        buff_prog.uniform1i("enable_borders", 1);
    } else {
        buff_prog.uniform1i("enable_borders", 0);
    }
}
