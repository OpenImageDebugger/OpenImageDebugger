#pragma once

#include <string>
#include <iostream>
#include <GLFW/glfw3.h>
#include <sstream>
#include <deque>
#include <mutex>

#include <Python.h>

#include "stage.hpp"

class Window {
public:
    ~Window() {
        Py_DECREF(owned_buffer);
        glfwDestroyWindow(window_);
    }

    struct BufferUpdateMessage {
        std::string var_name_str;
        PyObject* py_buffer;
        int width_i;
        int height_i;
        int channels;
        int type;
        int step;
    };
    std::deque<BufferUpdateMessage> pending_updates;

    void buffer_update(PyObject* pybuffer, const std::string& var_name_str,
            int buffer_width_i, int buffer_height_i, int channels, int type, int step) {
        Py_INCREF(pybuffer);


        BufferUpdateMessage new_buffer;
        new_buffer.var_name_str = var_name_str;
        new_buffer.py_buffer = pybuffer;
        new_buffer.width_i = buffer_width_i;
        new_buffer.height_i = buffer_height_i;
        new_buffer.channels = channels;
        new_buffer.type = type;
        new_buffer.step = step;
        {
        std::unique_lock<std::mutex> lock(mtx_);
        pending_updates.push_back(new_buffer);
        }
    }

    bool create(const std::string& var_name_str, int gl_major_version,
            int gl_minor_version, PyObject* pybuffer, int buffer_width_i,
            int buffer_height_i, int channels, int type, int step, int win_width = 800,
            int win_height = 600, GLFWmonitor* monitor = NULL) {
        owned_buffer = pybuffer;
        var_name_ = var_name_str;

        Py_INCREF(owned_buffer);
        uint8_t* buffer = reinterpret_cast<uint8_t*>(PyMemoryView_GET_BUFFER(owned_buffer)->buf);

        // Initialize GLFW
        if( !glfwInit() ) {
            std::cerr << "Failed to initialize GLFW\n" << std::endl;
            return false;
        }

        glfwWindowHint(GLFW_SAMPLES, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, gl_major_version);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, gl_minor_version);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);

        window_ = glfwCreateWindow(win_width, win_height, "", monitor, NULL);   
        setWindowTitle(var_name_str, buffer_width_i, buffer_height_i, channels, type);

        if(window_ == nullptr) {
            std::cerr << "Failed to open GLFW window." << std::endl;
            glfwTerminate();
            return false;
        }

        // Ensure we can capture the escape key being pressed below
        glfwSetInputMode(window_, GLFW_STICKY_KEYS, GL_TRUE );

        set_callbacks();

        glfwMakeContextCurrent(window_);

        GLenum err = glewInit();
        if (err != GLEW_OK) {
            std::cerr << "Error while initializing GLEW:" << glewGetErrorString(err) << std::endl;
            return false;
        }

        glfwGetFramebufferSize(window_, &window_width_i_, &window_height_i_);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

        // Setup stage
        stage_.window = this;
        bool init_status = stage_.initialize(buffer, buffer_width_i, buffer_height_i,
                channels, type, step);

        setWindowTitle(var_name_str, buffer_width_i, buffer_height_i, channels, type);

        return init_status;
    }

    GLFWwindow* glfw_window(){
        return window_;
    }

    void printGLInfo() {
        int gl_major_version = glfwGetWindowAttrib(window_, GLFW_CONTEXT_VERSION_MAJOR);
        int gl_minor_version = glfwGetWindowAttrib(window_, GLFW_CONTEXT_VERSION_MINOR);
        int rev = glfwGetWindowAttrib(window_, GLFW_CONTEXT_REVISION);

        fprintf(stderr, "Opened window has recieved OpenGL version: %d.%d.%d\n", gl_major_version, gl_minor_version, rev);
    } 

    static void error_callback(int error, const char* description) {
        std::cerr << "[GLFW] Error " << error << ": " << description << std::endl;
    }

    bool isMouseDown() {
        return mouseDown_;
    }

    void run() {
        do {
            ////
            // Update logic
            glfwGetCursorPos (window_, &mouseX_, &mouseY_);
            mouseDown_ = (glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_LEFT)
                    == GLFW_PRESS);

            stage_.update();
            stage_.draw();

            glfwSwapBuffers(window_);
            glfwPollEvents();

            // Check for buffer updates
            {
                if(pending_updates.size() > 0) {
                    BufferUpdateMessage message;
                    {
                        std::unique_lock<std::mutex> lock(mtx_);
                        message = pending_updates.front();
                        pending_updates.pop_front();
                    }
                    Py_DECREF(owned_buffer);
                    owned_buffer = message.py_buffer;
                    uint8_t* buffer = reinterpret_cast<uint8_t*>(
                            PyMemoryView_GET_BUFFER(owned_buffer)->buf);
                    setWindowTitle(message.var_name_str, message.width_i,
                            message.height_i, message.channels, message.type);
                    stage_.buffer_update(buffer, message.width_i,
                            message.height_i, message.channels, message.type,
                            message.step);
                }
            }

            // Check for pending callbacks
            {
                std::unique_lock<std::mutex> lock(mtx_);
                for(auto& cbk: pending_callbacks) {
                    cbk();
                }
                pending_callbacks.clear();
            }
        }
        while( !glfwWindowShouldClose(window_) );
    }

    double mouseX() {
        return mouseX_;
    }

    double mouseY() {
        return mouseY_;
    }

    // Window width
    int window_width() {
        return window_width_i_;
    }

    // Window height
    int window_height() {
        return window_height_i_;
    }

    Stage* stage() {
        return &stage_;
    }

private:
    GLFWwindow* window_;
    Stage stage_;
    std::string var_name_;
    int window_width_i_, window_height_i_;
    double mouseX_, mouseY_;
    bool mouseDown_;
    PyObject* owned_buffer;
    std::mutex mtx_;
    std::vector<std::function<void()>> pending_callbacks;

    void setWindowTitle(const std::string& var_name_str, int buffer_width_i,
            int buffer_height_i, int channels, int type) {
        std::stringstream window_name;
        window_name << "[GDB-ImageWatch] " << var_name_str <<
            " (" << buffer_width_i << "x" << buffer_height_i << "; ";

        if(type == 0)
            window_name << "float32";
        else if(type==1)
            window_name << "uint8";

        window_name << "; ";

        if (channels==1)
            window_name << "1 channel)";
        else
            window_name << channels << " channels)";

        float zoom = 1.0;
        Camera* cam = stage_.getComponent<Camera>("camera_component");
        if(cam != nullptr)
            zoom = cam->zoom;
        char zoom_str[40];
        sprintf(zoom_str, "%.2f", zoom*100.0);
        window_name << " - " << zoom_str << "%";

        glfwSetWindowTitle(window_, window_name.str().c_str());
    }


    static Window* getThisPtr(GLFWwindow *window) {
        return reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
    }

    static void scroll_callback(GLFWwindow *window, double x, double y) {
        Window* this_ = getThisPtr(window);

        std::unique_lock<std::mutex> lock(this_->mtx_);
        this_->pending_callbacks.push_back([x, y, this_]() {
            this_->stage_.scroll_callback(y);

            // Update window title
            Buffer* buff = this_->stage_.getComponent<Buffer>("buffer_component");
            int bw = buff->buffer_width_f;
            int bh = buff->buffer_height_f;
            this_->setWindowTitle(this_->var_name_, bw, bh, buff->channels, buff->type);
        });
    }

    static void resize_callback(GLFWwindow *window, int w, int h) {
        Window* this_ = getThisPtr(window);

        std::unique_lock<std::mutex> lock(this_->mtx_);
        this_->pending_callbacks.push_back([w, h, this_]() {
            this_->window_width_i_ = w;
            this_->window_height_i_ = h;

            this_->stage_.window_resized(this_->window_width_i_, this_->window_height_i_);
        });
    }

    void set_callbacks() {
        glfwSetWindowUserPointer(window_, this);

        glfwSetErrorCallback(Window::error_callback);
        // These callbacks are merely responsible for enqueueing events into
        // the responsible window objects event vector. This is because one
        // window thread might be used to execute the callback function
        // generated by the other thread, and the code executed by the stage
        // object is not thread safe (especially anything opengl-related).
        glfwSetScrollCallback(window_, &Window::scroll_callback);
        glfwSetFramebufferSizeCallback(window_, &Window::resize_callback);
    }
};

