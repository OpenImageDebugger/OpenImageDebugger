/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2026 OpenImageDebugger contributors
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

#ifndef EVENT_HANDLER_H_
#define EVENT_HANDLER_H_

#include <functional>
#include <memory>
#include <mutex>

class QKeyEvent;
class QListWidgetItem;
class QPoint;
class QString;
class QWidget;

namespace oid
{

struct BufferData;
struct ChannelNames;
struct UIComponents;
struct WindowState;
class Stage;
enum class EventProcessCode;

class UIEventHandler
{
  public:
    struct Dependencies
    {
        std::recursive_mutex& ui_mutex;
        BufferData& buffer_data;
        WindowState& state;
        UIComponents& ui_components;
        ChannelNames& channel_names;
        QString& default_export_suffix;
        std::shared_ptr<class GLCanvas> gl_canvas;
        QWidget* parent_widget;
        std::function<void()> update_status_bar;
        std::function<void(const std::shared_ptr<Stage>&)>
            set_currently_selected_stage;
        std::function<void()> set_currently_selected_stage_null;
        std::function<void(std::string_view)> request_plot_buffer;
        std::function<void()> reset_ac_min_labels;
        std::function<void()> reset_ac_max_labels;
        std::function<void()> update_shift_precision;
        std::function<void()> persist_settings_deferred;
    };

    explicit UIEventHandler(Dependencies deps);

    void resize_callback(int w, int h) const;
    void scroll_callback(float delta);
    void mouse_drag_event(int mouse_x, int mouse_y);
    void mouse_move_event(int mouse_x, int mouse_y) const;

    void recenter_buffer();
    void link_views_toggle();
    void decrease_float_precision();
    void increase_float_precision();
    void update_shift_precision() const;
    void rotate_90_cw();
    void rotate_90_ccw();

    void buffer_selected(QListWidgetItem* item);
    void remove_selected_buffer();
    void symbol_selected();
    void symbol_completed(const QString& str);
    void export_buffer(const QString& buffer_name);
    void show_context_menu(const QPoint& pos);
    void toggle_go_to_dialog() const;
    void go_to_pixel(float x, float y);

    void propagate_key_press_event(const QKeyEvent* key_event,
                                   EventProcessCode& event_intercepted) const;

  private:
    Dependencies deps_;
};

} // namespace oid

#endif // EVENT_HANDLER_H_
