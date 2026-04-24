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

#ifndef MAIN_WINDOW_INITIALIZER_H_
#define MAIN_WINDOW_INITIALIZER_H_

#include <memory>

#include <QString>

class QAbstractButton;
class QChar;
class QLabel;
class QTcpSocket;

namespace oid
{

struct BufferData;
struct ChannelNames;
struct ConnectionSettings;
struct UIComponents;
struct WindowState;
class AutoContrastController;
class UIEventHandler;
class MainWindow;
class SettingsManager;

class MainWindowInitializer
{
  public:
    struct Dependencies
    {
        WindowState& state;
        UIComponents& ui_components;
        BufferData& buffer_data;
        ChannelNames& channel_names;
        ConnectionSettings& host_settings;
        QTcpSocket& socket;
        MainWindow& main_window;
        AutoContrastController& ac_controller;
        UIEventHandler& event_handler;
        SettingsManager& settings_manager;
    };

    explicit MainWindowInitializer(Dependencies deps);

    void initialize();

  private:
    Dependencies deps_;

    void initialize_ui_icons() const;

    void initialize_ui_signals();

    void initialize_timers();

    void initialize_shortcuts();

    void initialize_symbol_completer();

    void initialize_auto_contrast_form() const;

    void initialize_toolbar() const;

    void initialize_left_pane() const;

    void initialize_status_bar();

    void initialize_visualization_pane();

    void initialize_go_to_widget();

    void initialize_networking();

    static void setFontIcon(QAbstractButton* ui_element,
                            const wchar_t unicode_id[]);

    static void setVectorIcon(QLabel* ui_element,
                              const QString& icon_file_name,
                              int width,
                              int height);
};

} // namespace oid

#endif // MAIN_WINDOW_INITIALIZER_H_
