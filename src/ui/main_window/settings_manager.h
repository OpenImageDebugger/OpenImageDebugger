/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2026 OpenImageDebugger contributors
 * (https://github.com/OpenImageDebugger/OpenImageDebugger)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated files (the "Software"), to deal in the
 * Software without restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
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

#ifndef SETTINGS_MANAGER_H_
#define SETTINGS_MANAGER_H_

#include <mutex>

#include <QChar>
#include <QPoint>
#include <QSettings>
#include <QSize>
#include <QString>

namespace oid
{

struct BufferData;
struct ChannelNames;
struct UIComponents;
struct WindowState;
class MainWindow;

class SettingsManager
{
  public:
    struct Dependencies
    {
        std::recursive_mutex& ui_mutex;
        WindowState& state;
        UIComponents& ui_components;
        BufferData& buffer_data;
        ChannelNames& channel_names;
        double& render_framerate;
        QString& default_export_suffix;
        MainWindow& main_window;
    };

    explicit SettingsManager(Dependencies deps);

    void load_settings();

    void persist_settings();

    void persist_settings_deferred();

  private:
    Dependencies deps_;

    void initialize_settings_ui(QSettings& settings);

    void initialize_settings_ui_list_position(const QSettings& settings) const;

    void initialize_settings_ui_splitter(const QSettings& settings) const;

    void initialize_settings_ui_minmax_compact(const QSettings& settings) const;

    static QString
    initialize_settings_ui_colorspace_channel(const QChar& character);

    void initialize_settings_ui_colorspace(const QSettings& settings);

    void initialize_settings_ui_minmax_visible(const QSettings& settings) const;

    void initialize_settings_ui_contrast_enabled(const QSettings& settings);

    void initialize_settings_ui_link_views_enabled(const QSettings& settings);
};

} // namespace oid

#endif // SETTINGS_MANAGER_H_
