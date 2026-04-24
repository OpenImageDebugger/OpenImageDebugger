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

#include <functional>
#include <set>
#include <string>

#include <QChar>
#include <QList>
#include <QObject>
#include <QPoint>
#include <QSettings>
#include <QSize>
#include <QString>
#include <QStringList>

namespace oid
{

namespace SettingsConstants
{
constexpr int WINDOW_RESIZE_RESTORE_DELAY_MS = 100;
constexpr int SETTINGS_PERSIST_DELAY_MS      = 100;
constexpr double DEFAULT_FRAMERATE           = 60.0;
constexpr double MIN_FRAMERATE               = 1.0;
constexpr int BUFFER_EXPIRATION_DAYS         = 1;

constexpr const char* ORGANIZATION_NAME             = "OpenImageDebugger";
constexpr const char* DEFAULT_EXPORT_SUFFIX         = "Image File (*.png)";
constexpr const char* SETTINGS_GROUP_EXPORT         = "Export";
constexpr const char* SETTINGS_GROUP_RENDERING      = "Rendering";
constexpr const char* SETTINGS_GROUP_UI             = "UI";
constexpr const char* SETTINGS_GROUP_MAINWINDOW     = "MainWindow";
constexpr const char* SETTINGS_KEY_FRAMERATE        = "maximum_framerate";
constexpr const char* SETTINGS_KEY_EXPORT_SUFFIX    = "default_export_suffix";
constexpr const char* SETTINGS_KEY_PREVIOUS_BUFFERS = "PreviousSession/buffers";
} // namespace SettingsConstants

class SettingsManager : public QObject
{
    Q_OBJECT

  public:
    // Data provider callbacks for persistence
    struct DataCallbacks
    {
        // Window geometry
        std::function<QSize()> getWindowSize;
        std::function<QPoint()> getWindowPos;

        // UI state
        std::function<QList<int>()> getSplitterSizes;
        std::function<bool()> getMinMaxVisible;
        std::function<bool()> getContrastEnabled;
        std::function<bool()> getLinkViewsEnabled;

        // Application state
        std::function<double()> getRenderFramerate;
        std::function<QString()> getDefaultExportSuffix;

        // Buffer data
        std::function<std::set<std::string, std::less<>>()>
            getCurrentBufferNames;
        std::function<std::set<std::string, std::less<>>()>
            getPreviousSessionBuffers;
        std::function<std::set<std::string, std::less<>>()>
            getRemovedBufferNames;
        std::function<void()> clearRemovedBufferNames;
    };

    explicit SettingsManager(QObject* parent = nullptr);

    void load_settings();

    void persist_settings(const DataCallbacks& callbacks) const;

  signals:
    // Rendering settings
    void renderingSettingsLoaded(double framerate);

    // Export settings
    void exportSettingsLoaded(QString defaultSuffix);

    // Window geometry
    void windowGeometryLoaded(QSize size, QPoint pos);
    void windowResizeRestoreRequested();

    // UI geometry and state
    void uiListPositionLoaded(QString position);
    void uiSplitterSizesLoaded(QList<int> sizes);
    void uiMinMaxCompactLoaded(bool compact, bool visible);
    void uiColorspaceLoaded(QString ch1, QString ch2, QString ch3, QString ch4);
    void uiMinMaxVisibleLoaded(bool visible);
    void uiContrastEnabledLoaded(bool enabled);
    void uiLinkViewsEnabledLoaded(bool enabled);

    // Previous session buffers
    void previousSessionBuffersLoaded(QStringList buffers);

  private:
    static QString colorspace_channel_from_char(const QChar& character);

    void load_rendering_settings(const QSettings& settings);

    void load_export_settings(QSettings& settings);

    void load_window_geometry(QSettings& settings);

    void load_ui_settings(const QSettings& settings);

    void load_previous_session_buffers(const QSettings& settings);
};

} // namespace oid

#endif // SETTINGS_MANAGER_H_
