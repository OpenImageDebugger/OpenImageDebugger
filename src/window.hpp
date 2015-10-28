#pragma once

#include <string>
#include <iostream>
#include <GLFW/glfw3.h>
#include <deque>

#include <Python.h>

#include <mutex>

#include "stage.hpp"

class Window {
public:
    ~Window() {
        Py_DECREF(owned_buffer);
    }

    struct BufferUpdateMessage {
        PyObject* py_buffer;
        int width_i;
        int height_i;
        int channels;
        int type;
    };
    std::deque<BufferUpdateMessage> pending_updates;

    void buffer_update(PyObject* pybuffer, int buffer_width_i, int buffer_height_i,
            int channels, int type) {
        Py_INCREF(pybuffer);


        BufferUpdateMessage new_buffer;
        new_buffer.py_buffer = pybuffer;
        new_buffer.width_i = buffer_width_i;
        new_buffer.height_i = buffer_height_i;
        new_buffer.channels = channels;
        new_buffer.type = type;
        {
        std::unique_lock<std::mutex> lock(mtx_);
        pending_updates.push_back(new_buffer);
        }
    }

    bool create(std::string title, int gl_major_version, int gl_minor_version,
            PyObject* pybuffer, int buffer_width_i, int buffer_height_i,
            int channels, int type, int win_width = 800, int win_height = 600,
            GLFWmonitor* monitor = NULL) {
        owned_buffer = pybuffer;
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

        window_ = glfwCreateWindow(win_width, win_height, title.c_str(), monitor, NULL);   

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
        return stage_.initialize(buffer, buffer_width_i, buffer_height_i,
                channels, type);
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
                    stage_.buffer_update(buffer, message.width_i,
                            message.height_i, message.channels, message.type);
                }
            }
        }
        while( !glfwWindowShouldClose(window_) );

        glfwDestroyWindow(window_);
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
    int window_width_i_, window_height_i_;
    double mouseX_, mouseY_;
    bool mouseDown_;
    PyObject* owned_buffer;
    std::mutex mtx_;

    static Window* getThisPtr(GLFWwindow *window) {
        return reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
    }

    static void scroll_callback(GLFWwindow *window, double x, double y) {
        Window* this_ = getThisPtr(window);

        this_->stage_.scroll_callback(y);
    }

    static void resize_callback(GLFWwindow *window, int, int) {
        Window* this_ = getThisPtr(window);

        glfwGetFramebufferSize(window, &this_->window_width_i_,
                &this_->window_height_i_);

        this_->stage_.window_resized(this_->window_width_i_, this_->window_height_i_);
    }

    void set_callbacks() {
        glfwSetWindowUserPointer(window_, this);

        glfwSetErrorCallback(Window::error_callback);
        glfwSetScrollCallback(window_, &Window::scroll_callback);
        glfwSetWindowSizeCallback(window_, &Window::resize_callback);
    }
};

