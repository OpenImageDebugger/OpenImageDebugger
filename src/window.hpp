#pragma once

#include <string>
#include <iostream>
#include <GLFW/glfw3.h>

#include "stage.hpp"

class Window {
public:
    bool create(std::string title, int gl_major_version, int gl_minor_version,
            uint8_t* buffer, int buffer_width_i, int buffer_height_i,
            int channels, int type, int width = 800, int height = 600,
            GLFWmonitor* monitor = NULL) {
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

        window_ = glfwCreateWindow(width, height, title.c_str(), monitor, NULL);   

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

        // Setup stage
        return stage_.initialize(this, width, height, buffer, buffer_width_i,
                buffer_height_i, channels, type);
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
        glfwGetFramebufferSize(window_, &window_width_i_, &window_height_i_);
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
        }
        while( !glfwWindowShouldClose(window_) );

        glfwDestroyWindow(window_);
        glfwTerminate();
    }

    double mouseX() {
        return mouseX_;
    }

    double mouseY() {
        return mouseY_;
    }

private:
    GLFWwindow* window_;
    Stage stage_;
    int window_width_i_, window_height_i_;
    double mouseX_, mouseY_;
    bool mouseDown_;

    static Window* getThisPtr(GLFWwindow *window) {
        return reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
    }

    static void scroll_callback(GLFWwindow *window, double x, double y) {
        Window* this_ = getThisPtr(window);

        this_->stage_.scroll_callback(y);
    }

    static void resize_callback(GLFWwindow *window, int w, int h) {
        Window* this_ = getThisPtr(window);

        glfwGetFramebufferSize(window, &this_->window_width_i_,
                &this_->window_height_i_);

        this_->stage_.window_resized(w, h);
    }

    void set_callbacks() {
        glfwSetWindowUserPointer(window_, this);

        glfwSetErrorCallback(Window::error_callback);
        glfwSetScrollCallback(window_, &Window::scroll_callback);
        glfwSetWindowSizeCallback(window_, &Window::resize_callback);
    }
};

