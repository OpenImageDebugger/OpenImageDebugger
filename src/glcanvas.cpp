#include <GL/glew.h>
#include "mainwindow.h"
#include "glcanvas.hpp"

void GLCanvas::initializeGL() {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

    GLenum err = glewInit();
    if (err != GLEW_OK) {
        std::cerr << "Error while initializing GLEW:" << glewGetErrorString(err) << std::endl;
    }

    /// Texture for generating icons
    int icon_width = 200;
    int icon_height = 100;
    glGenTextures(1, &icon_texture_);
    glBindTexture(GL_TEXTURE_2D, icon_texture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, icon_width, icon_height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    // Generate FBO
    glGenFramebuffers(1, &icon_fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, icon_fbo_);
    // Attach 2D texture to this FBO
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, icon_texture_, 0);
    // Check if the GPU won't freak out about our FBO
    GLenum status;
    status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    switch(status) {
    case GL_FRAMEBUFFER_COMPLETE:
        cout<<"Info: FBO configuration is going to work well -- chill out!" << endl;
        break;
    default:
        cerr << "Error: FBO configuration is not supported -- sorry mate!" << endl;
        break;
    }

}

void GLCanvas::render_buffer_icon(Stage* stage) {
    const int icon_width = 200;
    const int icon_height = 100;
    glBindFramebuffer(GL_FRAMEBUFFER_EXT, icon_fbo_);
    glViewport(0, 0, icon_width, icon_height);

    Camera* cam = stage->getComponent<Camera>("camera_component");

    // Adapt camera to the thumbnail dimentions
    cam->window_resized(icon_width, icon_height);
    // Flips the projected image along the horizontal axis
    cam->projection.setOrthoProjection(icon_width/2.0, -icon_height/2.0, -1.0f, 1.0f);
    cam->set_initial_zoom();

    stage->draw();
    stage->buffer_icon_.resize(3*icon_width*icon_height);
    glReadPixels(0, 0, icon_width, icon_height, GL_RGB, GL_UNSIGNED_BYTE,
                 stage->buffer_icon_.data());

    glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
    glViewport(0, 0, width(), height());

    // Reset stage camera
    cam->window_resized(width(), height());
    cam->set_initial_zoom();
}

void GLCanvas::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
    main_window_->resize_callback(w,h);
}

void GLCanvas::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    main_window_->draw();
    swapBuffers();
}

void GLCanvas::wheelEvent(QWheelEvent* ev) {
    main_window_->scroll_callback(ev->delta()/120.0f);
}
