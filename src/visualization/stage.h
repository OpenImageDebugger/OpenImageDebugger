/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 OpenImageDebugger contributors
 * (https://github.com/OpenImageDebugger/OpenImageDebugger)
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

#ifndef STAGE_H_
#define STAGE_H_

#include <map>
#include <memory>

#include "visualization/components/buffer.h"


class GameObject;


class Stage
{
  public:
    bool contrast_enabled;
    std::vector<uint8_t> buffer_icon;
    MainWindow* main_window;

    Stage(MainWindow* main_window);

    bool initialize(const uint8_t* buffer,
                    int buffer_width_i,
                    int buffer_height_i,
                    int channels,
                    BufferType type,
                    int step,
                    const std::string& pixel_layout,
                    bool transpose_buffer);

    bool buffer_update(const uint8_t* buffer,
                       int buffer_width_i,
                       int buffer_height_i,
                       int channels,
                       BufferType type,
                       int step,
                       const std::string& pixel_layout,
                       bool transpose_buffer);

    GameObject* get_game_object(std::string tag);

    void update();

    void draw();

    void scroll_callback(float delta);

    void resize_callback(int w, int h);

    void mouse_drag_event(int mouse_x, int mouse_y);

    void mouse_move_event(int mouse_x, int mouse_y);

    EventProcessCode key_press_event(int key_code);

    void go_to_pixel(float x, float y);

  private:
    std::map<std::string, std::shared_ptr<GameObject>> all_game_objects;
};

#endif // STAGE_H_
