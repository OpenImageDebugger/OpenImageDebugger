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

#include "main_window.h"

// All UI event handling logic has been moved to UIEventHandler.
// This file contains Qt override methods and delegation methods for compatibility.

namespace oid
{

void MainWindow::resize_callback(const int w, const int h) const
{
    event_handler_->resize_callback(w, h);
}


void MainWindow::scroll_callback(const float delta)
{
    event_handler_->scroll_callback(delta);
}


void MainWindow::mouse_drag_event(const int mouse_x, const int mouse_y)
{
    event_handler_->mouse_drag_event(mouse_x, mouse_y);
}


void MainWindow::mouse_move_event(int mouse_x, int mouse_y) const
{
    event_handler_->mouse_move_event(mouse_x, mouse_y);
}


void MainWindow::resizeEvent(QResizeEvent*)
{
    persist_settings_deferred();
}


void MainWindow::moveEvent(QMoveEvent*)
{
    persist_settings_deferred();
}


void MainWindow::closeEvent(QCloseEvent*)
{
    const auto lock        = std::unique_lock{ui_mutex_};
    state_.is_window_ready = false;
    persist_settings_deferred();
}


void MainWindow::propagate_key_press_event(
    const QKeyEvent* key_event,
    EventProcessCode& event_intercepted) const
{
    event_handler_->propagate_key_press_event(key_event, event_intercepted);
}


bool MainWindow::eventFilter(QObject* target, QEvent* event)
{
    KeyboardState::update_keyboard_state(event);

    if (event->type() == QEvent::KeyPress) {
        const auto* key_event = dynamic_cast<QKeyEvent*>(event);

        auto event_intercepted = EventProcessCode::IGNORED;

        const auto lock = std::unique_lock{ui_mutex_};
        if (state_.link_views_enabled) {
            propagate_key_press_event(key_event, event_intercepted);
        } else if (const auto stage =
                       buffer_data_.currently_selected_stage.lock()) {
            event_intercepted = stage->key_press_event(key_event->key());
        }

        if (event_intercepted == EventProcessCode::INTERCEPTED) {
            state_.request_render_update = true;
            update_status_bar();

            event->accept();
            return true;
        }

        return QObject::eventFilter(target, event);
    }

    return false;
}


void MainWindow::recenter_buffer()
{
    event_handler_->recenter_buffer();
}


void MainWindow::link_views_toggle()
{
    event_handler_->link_views_toggle();
}

void MainWindow::decrease_float_precision()
{
    event_handler_->decrease_float_precision();
}

void MainWindow::increase_float_precision()
{
    event_handler_->increase_float_precision();
}

void MainWindow::update_shift_precision() const
{
    event_handler_->update_shift_precision();
}

void MainWindow::rotate_90_cw()
{
    event_handler_->rotate_90_cw();
}


void MainWindow::rotate_90_ccw()
{
    event_handler_->rotate_90_ccw();
}


// NOSONAR: Parameter must be non-const to match Qt signal signature
// currentItemChanged(QListWidgetItem*, QListWidgetItem*) which emits
// non-const pointers.
void MainWindow::buffer_selected(QListWidgetItem* item) // NOSONAR
{
    event_handler_->buffer_selected(item);
}


void MainWindow::remove_selected_buffer()
{
    event_handler_->remove_selected_buffer();
}


void MainWindow::symbol_selected()
{
    event_handler_->symbol_selected();
}


void MainWindow::symbol_completed(const QString& str)
{
    event_handler_->symbol_completed(str);
}


void MainWindow::export_buffer()
{
    const auto sender_action(dynamic_cast<QAction*>(sender()));
    const auto buffer_name = sender_action->data().toString();
    event_handler_->export_buffer(buffer_name);
}


void MainWindow::show_context_menu(const QPoint& pos)
{
    event_handler_->show_context_menu(pos);
}


void MainWindow::toggle_go_to_dialog() const
{
    event_handler_->toggle_go_to_dialog();
}


void MainWindow::go_to_pixel(const float x, const float y)
{
    event_handler_->go_to_pixel(x, y);
}

} // namespace oid
