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

#include <QList>
#include <QObject>
#include <QSize>
#include <QString>

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
    void apply_rendering_settings(double framerate) const;
    void apply_export_settings(const QString& defaultSuffix) const;
    void apply_window_geometry(QSize size, QPoint pos) const;
    void restore_window_resize() const;
    void apply_ui_list_position(const QString& position) const;
    void apply_ui_splitter_sizes(const QList<int>& sizes) const;
    void apply_ui_minmax_compact(bool compact, bool visible) const;
    void apply_ui_colorspace(const QString& ch1,
                             const QString& ch2,
                             const QString& ch3,
                             const QString& ch4) const;
    void apply_ui_minmax_visible(bool visible) const;
    void apply_ui_contrast_enabled(bool enabled) const;
    void apply_ui_link_views_enabled(bool enabled) const;
    void apply_previous_session_buffers(QStringList buffers) const;

  private:
    Dependencies deps_;
};

} // namespace oid

#endif // SETTINGS_APPLIER_H_
