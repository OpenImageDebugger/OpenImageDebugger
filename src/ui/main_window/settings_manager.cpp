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

#include "settings_manager.h"

#include <ranges>
#include <utility>

#include <QDataStream>
#include <QDateTime>
#include <QList>
#include <QMetaType>
#include <QPair>
#include <QTimer>
#include <QVariant>

namespace oid
{

using BufferExpiration = QPair<QString, QDateTime>;

SettingsManager::SettingsManager(QObject* parent)
    : QObject{parent}
{
}

void SettingsManager::load_settings()
{
    // Qt 6: Register the metatype for QSettings serialization
    qRegisterMetaType<QList<BufferExpiration>>();

    auto settings = QSettings{QSettings::Format::IniFormat,
                              QSettings::Scope::UserScope,
                              SettingsConstants::ORGANIZATION_NAME};

    settings.sync();

    load_rendering_settings(settings);
    load_export_settings(settings);
    load_window_geometry(settings);
    load_ui_settings(settings);
    load_previous_session_buffers(settings);
}

void SettingsManager::load_rendering_settings(const QSettings& settings)
{
    const auto framerate_key =
        QString::fromUtf8(SettingsConstants::SETTINGS_GROUP_RENDERING) + "/" +
        QString::fromUtf8(SettingsConstants::SETTINGS_KEY_FRAMERATE);
    auto framerate =
        settings.value(framerate_key, SettingsConstants::DEFAULT_FRAMERATE)
            .value<double>();
    if (framerate <= 0.0) {
        framerate = SettingsConstants::MIN_FRAMERATE;
    }
    emit renderingSettingsLoaded(framerate);
}

void SettingsManager::load_export_settings(QSettings& settings)
{
    settings.beginGroup(SettingsConstants::SETTINGS_GROUP_EXPORT);
    QString default_suffix =
        QString::fromUtf8(SettingsConstants::DEFAULT_EXPORT_SUFFIX);
    if (const auto suffix_key =
            QString::fromUtf8(SettingsConstants::SETTINGS_KEY_EXPORT_SUFFIX);
        settings.contains(suffix_key)) {
        if (const auto suffix_value = settings.value(suffix_key);
            suffix_value.canConvert<QString>()) {
            default_suffix = suffix_value.value<QString>();
        }
    }
    settings.endGroup();
    emit exportSettingsLoaded(default_suffix);
}

void SettingsManager::load_window_geometry(QSettings& settings)
{
    settings.beginGroup(SettingsConstants::SETTINGS_GROUP_MAINWINDOW);
    const auto size = settings.value("size").toSize();
    const auto pos  = settings.value("pos").toPoint();
    settings.endGroup();

    emit windowGeometryLoaded(size, pos);
    // Schedule window resize restore after event loop processes
    QTimer::singleShot(SettingsConstants::WINDOW_RESIZE_RESTORE_DELAY_MS,
                       this,
                       &SettingsManager::windowResizeRestoreRequested);
}

void SettingsManager::load_ui_settings(const QSettings& settings)
{
    const QString ui_group_prefix =
        QString::fromUtf8(SettingsConstants::SETTINGS_GROUP_UI) + "/";

    // List position
    if (const auto list_pos_variant =
            settings.value(ui_group_prefix + "list_position");
        list_pos_variant.canConvert<QString>()) {
        emit uiListPositionLoaded(list_pos_variant.toString());
    }

    // Splitter sizes
    if (const auto splitter_variant =
            settings.value(ui_group_prefix + "splitter");
        splitter_variant.canConvert<QVariantList>()) {
        const auto listVariants = splitter_variant.toList();
        QList<int> listSizes;
        for (const QVariant& size : listVariants) {
            listSizes.append(size.toInt());
        }
        emit uiSplitterSizesLoaded(listSizes);
    }

    // MinMax compact
    const auto compact_variant =
        settings.value(ui_group_prefix + "minmax_compact");
    const auto visible_variant =
        settings.value(ui_group_prefix + "minmax_visible");
    const bool is_compact =
        compact_variant.canConvert<bool>() && compact_variant.toBool();
    const bool is_visible =
        visible_variant.canConvert<bool>() ? visible_variant.toBool() : true;
    emit uiMinMaxCompactLoaded(is_compact, is_visible);

    // Colorspace
    if (const auto colorspace_variant =
            settings.value(ui_group_prefix + "colorspace");
        colorspace_variant.canConvert<QString>()) {
        const auto colorspace_str = colorspace_variant.toString();
        QString ch1;
        QString ch2;
        QString ch3;
        QString ch4;
        if (!colorspace_str.isEmpty()) {
            ch1 = colorspace_channel_from_char(colorspace_str.at(0));
        }
        if (colorspace_str.size() > 1) {
            ch2 = colorspace_channel_from_char(colorspace_str.at(1));
        }
        if (colorspace_str.size() > 2) {
            ch3 = colorspace_channel_from_char(colorspace_str.at(2));
        }
        if (colorspace_str.size() > 3) {
            ch4 = colorspace_channel_from_char(colorspace_str.at(3));
        }
        emit uiColorspaceLoaded(ch1, ch2, ch3, ch4);
    }

    // MinMax visible
    if (visible_variant.canConvert<bool>()) {
        emit uiMinMaxVisibleLoaded(visible_variant.toBool());
    }

    // Contrast enabled
    if (const auto contrast_variant =
            settings.value(ui_group_prefix + "contrast_enabled");
        contrast_variant.canConvert<bool>()) {
        emit uiContrastEnabledLoaded(contrast_variant.toBool());
    }

    // Link views enabled
    if (const auto link_views_variant =
            settings.value(ui_group_prefix + "link_views_enabled");
        link_views_variant.canConvert<bool>()) {
        emit uiLinkViewsEnabledLoaded(link_views_variant.toBool());
    }
}

void SettingsManager::load_previous_session_buffers(const QSettings& settings)
{
    const auto now = QDateTime::currentDateTime();
    const auto previous_buffers =
        settings.value(SettingsConstants::SETTINGS_KEY_PREVIOUS_BUFFERS)
            .value<QList<BufferExpiration>>();

    QStringList valid_buffers;
    valid_buffers.reserve(previous_buffers.size());
    for (const auto& [name, time] : previous_buffers) {
        if (time >= now) {
            valid_buffers.append(name);
        }
    }
    emit previousSessionBuffersLoaded(valid_buffers);
}

void SettingsManager::persist_settings(const DataCallbacks& callbacks) const
{
    auto settings = QSettings{QSettings::Format::IniFormat,
                              QSettings::Scope::UserScope,
                              SettingsConstants::ORGANIZATION_NAME};

    auto persisted_session_buffers = QList<BufferExpiration>{};

    // Load previous session symbols
    const auto previous_session_buffers_qlist =
        settings.value(SettingsConstants::SETTINGS_KEY_PREVIOUS_BUFFERS)
            .value<QList<BufferExpiration>>();

    const auto now = QDateTime::currentDateTime();
    const auto next_expiration =
        now.addDays(SettingsConstants::BUFFER_EXPIRATION_DAYS);

    // Get current state through callbacks
    const auto current_buffers  = callbacks.getCurrentBufferNames();
    const auto previous_buffers = callbacks.getPreviousSessionBuffers();
    const auto removed_buffers  = callbacks.getRemovedBufferNames();

    // Of the buffers not currently being visualized, only keep those whose
    // timer hasn't expired yet and is not in the set of removed names
    for (const auto& prev_buff : previous_session_buffers_qlist) {
        const auto& [buff_name, timestamp] = prev_buff;
        const auto buff_name_std_str       = buff_name.toStdString();

        const auto being_viewed = current_buffers.contains(buff_name_std_str);
        const auto was_removed  = removed_buffers.contains(buff_name_std_str);

        if (!was_removed && !being_viewed && timestamp >= now) {
            persisted_session_buffers.append(prev_buff);
        }
    }

    // Add currently viewed buffers
    for (const auto& buffer : current_buffers) {
        persisted_session_buffers.append(
            BufferExpiration(buffer.c_str(), next_expiration));
    }

    // Write default suffix for buffer export
    const auto export_suffix_key =
        QString::fromUtf8(SettingsConstants::SETTINGS_GROUP_EXPORT) + "/" +
        QString::fromUtf8(SettingsConstants::SETTINGS_KEY_EXPORT_SUFFIX);
    settings.setValue(export_suffix_key, callbacks.getDefaultExportSuffix());

    // Write maximum framerate
    const auto framerate_key =
        QString::fromUtf8(SettingsConstants::SETTINGS_GROUP_RENDERING) + "/" +
        QString::fromUtf8(SettingsConstants::SETTINGS_KEY_FRAMERATE);
    settings.setValue(framerate_key, callbacks.getRenderFramerate());

    // Write previous session symbols
    settings.setValue(SettingsConstants::SETTINGS_KEY_PREVIOUS_BUFFERS,
                      QVariant::fromValue(persisted_session_buffers));

    // Write UI geometry
    settings.beginGroup(SettingsConstants::SETTINGS_GROUP_UI);
    {
        const auto splitterSizes = callbacks.getSplitterSizes();
        auto listSizesVariant    = QList<QVariant>{};
        for (const int size : splitterSizes) {
            listSizesVariant.append(size);
        }
        settings.setValue("splitter", listSizesVariant);
    }
    settings.setValue("minmax_visible", callbacks.getMinMaxVisible());
    settings.setValue("contrast_enabled", callbacks.getContrastEnabled());
    settings.setValue("link_views_enabled", callbacks.getLinkViewsEnabled());
    settings.endGroup();

    // Write window position/size
    settings.beginGroup(SettingsConstants::SETTINGS_GROUP_MAINWINDOW);
    settings.setValue("size", callbacks.getWindowSize());
    settings.setValue("pos", callbacks.getWindowPos());
    settings.endGroup();

    settings.sync();

    // Clear removed buffer names through callback
    if (callbacks.clearRemovedBufferNames) {
        callbacks.clearRemovedBufferNames();
    }
}


QString SettingsManager::colorspace_channel_from_char(const QChar& character)
{
    switch (character.toLatin1()) {
    case 'b':
        return "blue";
    case 'g':
        return "green";
    case 'r':
        return "red";
    case 'a':
        return "alpha";
    default:
        return {};
    }
}

} // namespace oid
