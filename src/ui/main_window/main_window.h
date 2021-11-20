/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 OpenImageDebugger contributors
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

#ifndef MAIN_WINDOW_H_
#define MAIN_WINDOW_H_

#include <deque>
#include <memory>
#include <mutex>
#include <set>
#include <string>

#include <QAbstractButton>
#include <QLabel>
#include <QListWidgetItem>
#include <QMainWindow>
#include <QTimer>
#include <QTcpSocket>

#include "math/linear_algebra.h"
#include "ui/go_to_widget.h"
#include "ui/symbol_completer.h"
#include "visualization/stage.h"


namespace Ui
{
class MainWindowUi;
}

struct ConnectionSettings {
    std::string url;
    uint16_t port;
};


class MainWindow : public QMainWindow
{
    Q_OBJECT

  public:
    ///
    // Constructor / destructor
    explicit MainWindow(const ConnectionSettings &host_settings,
                        QWidget* parent = nullptr);

    ~MainWindow();

    ///
    // Assorted methods - implemented in main_window.cpp
    void show();

    void draw();

    GLCanvas* gl_canvas();

    QSizeF get_icon_size();

    // External interface
    bool is_window_ready();

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

    void request_render_update();

    ///
    // Auto contrast pane - slots - implemented in auto_contrast.cpp
    void ac_c1_min_update();

    void ac_c2_min_update();

    void ac_c3_min_update();

    void ac_c4_min_update();

    void ac_c1_max_update();

    void ac_c2_max_update();

    void ac_c3_max_update();

    void ac_c4_max_update();

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

    QString default_export_suffix_;

    Stage* currently_selected_stage_;

    std::map<std::string, std::vector<uint8_t>> held_buffers_;
    std::map<std::string, std::shared_ptr<Stage>> stages_;

    std::set<std::string> previous_session_buffers_;
    std::set<std::string> removed_buffer_names_;

    QStringList available_vars_;

    std::mutex ui_mutex_;

    SymbolCompleter* symbol_completer_;

    Ui::MainWindowUi* ui_;

    QLabel* status_bar_;
    GoToWidget* go_to_widget_;

    ConnectionSettings host_settings_;
    QTcpSocket socket_;

    QString name_channel_1_;
    QString name_channel_2_;
    QString name_channel_3_;
    QString name_channel_4_;

    ///
    // Assorted methods - private - implemented in main_window.cpp
    void update_status_bar();

    qreal get_screen_dpi_scale();

    std::string get_type_label(BufferType type, int channels);

    void persist_settings_deferred();

    void set_currently_selected_stage(Stage* stage);

    vec4 get_stage_coordinates(float pos_window_x, float pos_window_y);

    ///
    // Communication with debugger bridge
    void decode_set_available_symbols();

    void respond_get_observed_symbols();

    void decode_plot_buffer_contents();

    void decode_incoming_messages();

    void request_plot_buffer(const char* buffer_name);

    ///
    // Auto contrast pane - private - implemented in auto_contrast.cpp
    void set_ac_min_value(int idx, float value);

    void set_ac_max_value(int idx, float value);

    ///
    // Initialization - private - implemented in initialization.cpp
    void setFontIcon(QAbstractButton* ui_element, QString unicode_id);
    void setVectorIcon(QLabel* ui_element, QString icon_file_name, int width, int height);
    void initialize_ui_icons();

    void initialize_ui_settings();

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

    void initialize_networking();
};

#endif // MAIN_WINDOW_H_
