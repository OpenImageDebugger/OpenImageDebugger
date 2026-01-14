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

#ifndef SETTINGS_APPLIER_H_
#define SETTINGS_APPLIER_H_

#include <mutex>
#include <set>
#include <string>

#include <QList>
#include <QObject>
#include <QPoint>
#include <QSize>
#include <QString>
#include <QStringList>

namespace oid
{

struct BufferData;
struct ChannelNames;
struct UIComponents;
struct WindowState;
class MainWindow;

class SettingsApplier : public QObject
{
    Q_OBJECT

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

    explicit SettingsApplier(Dependencies deps, QObject* parent = nullptr);

  public slots:
    void apply_rendering_settings(double framerate);
    void apply_export_settings(QString defaultSuffix);
    void apply_window_geometry(QSize size, QPoint pos);
    void restore_window_resize();
    void apply_ui_list_position(QString position);
    void apply_ui_splitter_sizes(QList<int> sizes);
    void apply_ui_minmax_compact(bool compact, bool visible);
    void
    apply_ui_colorspace(QString ch1, QString ch2, QString ch3, QString ch4);
    void apply_ui_minmax_visible(bool visible);
    void apply_ui_contrast_enabled(bool enabled);
    void apply_ui_link_views_enabled(bool enabled);
    void apply_previous_session_buffers(QStringList buffers);

  private:
    Dependencies deps_;
};

} // namespace oid

#endif // SETTINGS_APPLIER_H_
