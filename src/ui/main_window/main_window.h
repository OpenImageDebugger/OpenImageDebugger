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

#ifndef MAIN_WINDOW_H_
#define MAIN_WINDOW_H_

#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <string>

#include <QLabel>
#include <QSettings>
#include <QTcpSocket>
#include <QTimer>

#include "math/linear_algebra.h"
#include "ui/controllers/auto_contrast_controller.h"
#include "ui/events/event_handler.h"
#include "ui/go_to_widget.h"
#include "ui/messaging/message_handler.h"
#include "ui/symbol_completer.h"
#include "ui_main_window.h"
#include "visualization/stage.h"

namespace oid
{

class MainWindowInitializer;
class SettingsManager;

namespace UIConstants
{
constexpr int ICON_WIDTH_BASE  = 100;
constexpr int ICON_HEIGHT_BASE = 75;
} // namespace UIConstants

struct ConnectionSettings
{
    std::string url{};
    uint16_t port{};
};

struct WindowState
{
    bool is_window_ready{true};
    bool request_render_update{true};
    bool request_icons_update{true};
    bool completer_updated{false};
    bool ac_enabled{false};
    bool link_views_enabled{false};
};

struct UIComponents
{
    std::unique_ptr<SymbolCompleter> symbol_completer{};
    std::unique_ptr<Ui::MainWindowUi> ui{std::make_unique<Ui::MainWindowUi>()};
    std::unique_ptr<QLabel> status_bar{};
    std::unique_ptr<GoToWidget> go_to_widget{};
    QTimer settings_persist_timer{};
    QTimer update_timer{};
};

struct BufferData
{
    std::map<std::string, std::vector<std::byte>, std::less<>> held_buffers{};
    std::map<std::string, std::shared_ptr<Stage>, std::less<>> stages{};
    std::set<std::string, std::less<>> previous_session_buffers{};
    std::set<std::string, std::less<>> removed_buffer_names{};
    QStringList available_vars{};
    std::weak_ptr<Stage>
        currently_selected_stage{}; // Non-owning reference to selected stage
};

struct ChannelNames
{
    QString name_channel_1{"red"};
    QString name_channel_2{"green"};
    QString name_channel_3{"blue"};
    QString name_channel_4{"alpha"};
};


class MainWindow final : public QMainWindow
{
    Q_OBJECT

  public:
    ///
    // Constructor / destructor
    explicit MainWindow(ConnectionSettings host_settings,
                        QWidget* parent = nullptr);

    ~MainWindow() override;

    ///
    // Assorted methods - implemented in main_window.cpp
    void showWindow();

    void draw() const;

    [[nodiscard]] std::shared_ptr<GLCanvas> gl_canvas() const;

    [[nodiscard]] QSizeF get_icon_size() const;

    // External interface
    [[nodiscard]] bool is_window_ready() const;

    ///
    // General UI Events - implemented in ui_events.cpp
    void resize_callback(int w, int h) const;

    void scroll_callback(float delta);

    void mouse_drag_event(int mouse_x, int mouse_y);

    void mouse_move_event(int mouse_x, int mouse_y) const;

    // Window change events - only called after the event is finished
    bool eventFilter(QObject* target, QEvent* event) override;

    void resizeEvent(QResizeEvent*) override;

    void moveEvent(QMoveEvent*) override;

    void closeEvent(QCloseEvent*) override;

    ///
    // Assorted methods - implemented in main_window.cpp
    void loop();

    void request_render_update();

    void request_icons_update();


  private:
    WindowState state_{};
    UIComponents ui_components_{};
    BufferData buffer_data_{};
    ChannelNames channel_names_{};
    std::shared_ptr<GLCanvas> gl_canvas_ptr_{};
    double render_framerate_{};
    QString default_export_suffix_{};
    std::unique_ptr<AutoContrastController> ac_controller_{};
    std::unique_ptr<MessageHandler> message_handler_{};
    std::unique_ptr<UIEventHandler> event_handler_{};
    std::unique_ptr<SettingsManager> settings_manager_{};
    std::unique_ptr<MainWindowInitializer> initializer_{};
    // Thread safety: All access to buffer_data_ and state_ must be protected by
    // ui_mutex_. This mutex is recursive to allow nested locking when helper
    // methods are called from already-locked contexts (e.g.,
    // repaint_image_list_icon called from loop). All UI operations must run on
    // the main thread (Qt requirement), and this mutex protects against
    // concurrent access from message decoding, event handlers, and timer
    // callbacks. Mutable to allow locking in const member functions - locking
    // doesn't change logical state.
    mutable std::recursive_mutex ui_mutex_{};
    ConnectionSettings host_settings_{};
    QTcpSocket socket_{};

    ///
    // Assorted methods - private - implemented in main_window.cpp
    void update_status_bar() const;

    static qreal get_screen_dpi_scale();

    static std::string get_type_label(BufferType type, int channels);

    void persist_settings();

    void persist_settings_deferred();

    void set_currently_selected_stage(const std::shared_ptr<Stage>& stage);
    void set_currently_selected_stage(std::nullptr_t); // Overload for clearing

    [[nodiscard]] vec4 get_stage_coordinates(float pos_window_x,
                                             float pos_window_y) const;
};

} // namespace oid

#endif // MAIN_WINDOW_H_
