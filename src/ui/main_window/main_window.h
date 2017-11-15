/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 GDB ImageWatch contributors
 * (github.com/csantosbh/gdb-imagewatch/)
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

#ifndef MAIN_WINDOW_H_
#define MAIN_WINDOW_H_

#include <deque>
#include <memory>
#include <mutex>
#include <set>
#include <string>

#include <QLabel>
#include <QListWidgetItem>
#include <QMainWindow>
#include <QTimer>

#include "debuggerinterface/buffer_request_message.h"
#include "math/linear_algebra.h"
#include "ui/go_to_widget.h"
#include "ui/symbol_completer.h"
#include "visualization/stage.h"


namespace Ui
{
class MainWindowUi;
}


class MainWindow : public QMainWindow
{
    Q_OBJECT

  public:
    ///
    // Constructor / destructor
    explicit MainWindow(QWidget* parent = 0);

    ~MainWindow();

    ///
    // Assorted methods - implemented in main_window.cpp
    void show();

    void draw();

    QSizeF get_icon_size();

    // External interface
    void set_plot_callback(int (*plot_cbk)(const char*));

    void plot_buffer(const BufferRequestMessage& buffer_metadata);

    std::deque<std::string> get_observed_symbols();

    bool is_window_ready();

    void set_available_symbols(const std::deque<std::string>& available_set);

    ///
    // Auto contrast pane - implemented in auto_contrast.cpp
    void reset_ac_min_labels();

    void reset_ac_max_labels();

    ///
    // General UI Events - implemented in ui_events.cpp
    void resize_callback(int w, int h);

    void scroll_callback(float delta);

    void mouse_drag_event(int mouse_x, int mouse_y);

    void mouse_move_event(int mouse_x, int mouse_y);

    // Window change events - only called after the event is finished
    bool eventFilter(QObject* target, QEvent* event);

    void resizeEvent(QResizeEvent*);

    void moveEvent(QMoveEvent*);

    void closeEvent(QCloseEvent*);

  public Q_SLOTS:
    ///
    // Assorted methods - slots - implemented in main_window.cpp
    void loop();

    ///
    // Auto contrast pane - slots - implemented in auto_contrast.cpp
    void ac_red_min_update();

    void ac_green_min_update();

    void ac_blue_min_update();

    void ac_alpha_min_update();

    void ac_red_max_update();

    void ac_green_max_update();

    void ac_blue_max_update();

    void ac_alpha_max_update();

    void ac_min_reset();

    void ac_max_reset();

    void ac_toggle();

    ///
    // General UI Events - slots - implemented in ui_events.cpp
    void recenter_buffer();

    void link_views_toggle();

    void rotate_90_cw();

    void rotate_90_ccw();

    void buffer_selected(QListWidgetItem* item);

    void remove_selected_buffer();

    void symbol_selected();

    void symbol_completed(QString str);

    void export_buffer();

    void show_context_menu(const QPoint& pos);

    void toggle_go_to_dialog();

    void go_to_pixel(float x, float y);

  private Q_SLOTS:
    ///
    // Assorted methods - private slots - implemented in main_window.cpp
    void persist_settings();

  private:
    bool is_window_ready_;
    bool request_render_update_;
    bool completer_updated_;
    bool ac_enabled_;
    bool link_views_enabled_;

    const int icon_width_base_;
    const int icon_height_base_;

    double render_framerate_;

    QTimer settings_persist_timer_;
    QTimer update_timer_;

    Stage* currently_selected_stage_;

    std::map<std::string, std::shared_ptr<uint8_t>> held_buffers_;
    std::map<std::string, std::shared_ptr<Stage>> stages_;

    std::set<std::string> previous_session_buffers_;
    std::set<std::string> removed_buffer_names_;

    std::deque<BufferRequestMessage> pending_updates_;

    QStringList available_vars_;

    std::mutex ui_mutex_;

    SymbolCompleter* symbol_completer_;

    Ui::MainWindowUi* ui_;

    QLabel* status_bar_;
    GoToWidget* go_to_widget_;

    int (*plot_callback_)(const char*);

    ///
    // Assorted methods - private - implemented in main_window.cpp
    void update_status_bar();

    qreal get_screen_dpi_scale();

    std::string get_type_label(Buffer::BufferType type, int channels);

    void persist_settings_deferred();

    void set_currently_selected_stage(Stage* stage);

    vec4 get_stage_coordinates(float pos_window_x, float pos_window_y);

    ///
    // Auto contrast pane - private - implemented in auto_contrast.cpp
    void set_ac_min_value(int idx, float value);

    void set_ac_max_value(int idx, float value);

    ///
    // Initialization - private - implemented in initialization.cpp
    void initialize_ui_icons();

    void initialize_timers();

    void initialize_shortcuts();

    void initialize_symbol_completer();

    void initialize_auto_contrast_form();

    void initialize_toolbar();

    void initialize_left_pane();

    void initialize_status_bar();

    void initialize_visualization_pane();

    void initialize_settings();

    void initialize_go_to_widget();
};

#endif // MAIN_WINDOW_H_
