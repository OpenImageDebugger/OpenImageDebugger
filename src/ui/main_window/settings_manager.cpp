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
#include <QWidget>

#include "main_window.h"

namespace oid
{

SettingsManager::SettingsManager(Dependencies deps)
    : deps_{std::move(deps)}
{
}

void SettingsManager::load_settings()
{
    using BufferExpiration = QPair<QString, QDateTime>;

    // Qt 6: Register the metatype for QSettings serialization
    // Including QDataStream provides the stream operators for Qt types
    qRegisterMetaType<QList<BufferExpiration>>();

    auto settings = QSettings{QSettings::Format::IniFormat,
                              QSettings::Scope::UserScope,
                              "OpenImageDebugger"};

    settings.sync();

    // Load maximum framerate
    deps_.render_framerate =
        settings.value("Rendering/maximum_framerate", 60).value<double>();
    if (deps_.render_framerate <= 0.f) {
        deps_.render_framerate = 1.0;
    }

    // Default save suffix: Image
    settings.beginGroup("Export");
    if (settings.contains("default_export_suffix")) {
        deps_.default_export_suffix =
            settings.value("default_export_suffix").value<QString>();
    } else {
        deps_.default_export_suffix = "Image File (*.png)";
    }
    settings.endGroup();

    // Load previous session symbols
    const auto now              = QDateTime::currentDateTime();
    const auto previous_buffers = settings.value("PreviousSession/buffers")
                                      .value<QList<BufferExpiration>>();

    for (const auto& [name, time] : previous_buffers) {
        if (time >= now) {
            deps_.buffer_data.previous_session_buffers.insert(
                name.toStdString());
        }
    }

    // Load window position/size.
    // Window is loaded with a fixed size and restored in timer.
    // This is needed to give application some time to run event loop
    // and redraw all widgets without changing overall geometry.
    settings.beginGroup("MainWindow");
    deps_.main_window.setFixedSize(
        settings.value("size", deps_.main_window.size()).toSize());
    deps_.main_window.move(
        settings.value("pos", deps_.main_window.pos()).toPoint());
    settings.endGroup();

    // Load UI geometry.
    initialize_settings_ui(settings);

    // Restore possibility to resize application in timer.
    // This is needed to give application some time to run event loop
    // and redraw all widgets without changing overall geometry.
    QTimer::singleShot(100, &deps_.main_window, [this]() {
        deps_.main_window.setMinimumSize(0, 0);
        deps_.main_window.setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    });
}

void SettingsManager::persist_settings()
{
    using BufferExpiration = QPair<QString, QDateTime>;

    auto settings = QSettings{QSettings::Format::IniFormat,
                              QSettings::Scope::UserScope,
                              "OpenImageDebugger"};

    auto persisted_session_buffers = QList<BufferExpiration>{};

    // Load previous session symbols
    const auto previous_session_buffers_qlist =
        settings.value("PreviousSession/buffers")
            .value<QList<BufferExpiration>>();

    const auto now             = QDateTime::currentDateTime();
    const auto next_expiration = now.addDays(1);

    const auto lock = std::unique_lock{deps_.ui_mutex};
    // Of the buffers not currently being visualized, only keep those whose
    // timer hasn't expired yet and is not in the set of removed names
    for (const auto& prev_buff : previous_session_buffers_qlist) {
        const auto& [buff_name, timestamp] = prev_buff;
        const auto buff_name_std_str       = buff_name.toStdString();

        const auto being_viewed =
            deps_.buffer_data.held_buffers.contains(buff_name_std_str);
        const auto was_removed =
            deps_.buffer_data.removed_buffer_names.contains(buff_name_std_str);

        if (was_removed) {
            deps_.buffer_data.previous_session_buffers.erase(buff_name_std_str);
        } else if (!being_viewed && timestamp >= now) {
            persisted_session_buffers.append(prev_buff);
        }
    }

    for (const auto& buffer :
         deps_.buffer_data.held_buffers | std::views::keys) {
        persisted_session_buffers.append(
            BufferExpiration(buffer.c_str(), next_expiration));
    }

    // Write default suffix for buffer export
    settings.setValue("Export/default_export_suffix",
                      deps_.default_export_suffix);

    // Write maximum framerate
    settings.setValue("Rendering/maximum_framerate", deps_.render_framerate);

    // Write previous session symbols
    settings.setValue("PreviousSession/buffers",
                      QVariant::fromValue(persisted_session_buffers));

    // Write UI geometry.
    settings.beginGroup("UI");
    {
        const auto listSizesInt = deps_.ui_components.ui->splitter->sizes();

        auto listSizesVariant = QList<QVariant>{};
        for (const int size : listSizesInt) {
            listSizesVariant.append(size);
        }

        settings.setValue("splitter", listSizesVariant);
    }
    settings.setValue("minmax_visible",
                      deps_.ui_components.ui->acEdit->isChecked());
    settings.setValue("contrast_enabled",
                      deps_.ui_components.ui->acToggle->isChecked());
    settings.setValue("link_views_enabled",
                      deps_.ui_components.ui->linkViewsToggle->isChecked());
    settings.endGroup();

    // Write window position/size
    settings.beginGroup("MainWindow");
    settings.setValue("size", deps_.main_window.size());
    settings.setValue("pos", deps_.main_window.pos());
    settings.endGroup();

    settings.sync();

    deps_.buffer_data.removed_buffer_names.clear();
}

void SettingsManager::persist_settings_deferred()
{
    deps_.ui_components.settings_persist_timer.start(100);
}

void SettingsManager::initialize_settings_ui(QSettings& settings)
{
    settings.beginGroup("UI");

    initialize_settings_ui_list_position(settings);
    initialize_settings_ui_splitter(settings);
    initialize_settings_ui_minmax_compact(settings);
    initialize_settings_ui_colorspace(settings);
    initialize_settings_ui_minmax_visible(settings);
    initialize_settings_ui_contrast_enabled(settings);
    initialize_settings_ui_link_views_enabled(settings);

    settings.endGroup();
}

void SettingsManager::initialize_settings_ui_list_position(
    const QSettings& settings) const
{
    const auto variant = settings.value("list_position");
    if (!variant.canConvert<QString>()) {
        return;
    }

    const auto position_str = variant.toString();

    if (position_str == "top" || position_str == "bottom") {
        deps_.ui_components.ui->splitter->setOrientation(Qt::Vertical);
    }

    if (position_str == "right" || position_str == "bottom") {
        deps_.ui_components.ui->splitter->insertWidget(
            -1, deps_.ui_components.ui->frame_list);
    }

    deps_.ui_components.ui->splitter->repaint();
}

void SettingsManager::initialize_settings_ui_splitter(
    const QSettings& settings) const
{
    const auto variant = settings.value("splitter");
    if (!variant.canConvert<QVariantList>()) {
        return;
    }

    const auto listVariants = variant.toList();

    auto listSizes = QList<int>{};
    for (const QVariant& size : listVariants) {
        listSizes.append(size.toInt());
    }

    deps_.ui_components.ui->splitter->setSizes(listSizes);
}

void SettingsManager::initialize_settings_ui_minmax_compact(
    const QSettings& settings) const
{
    const auto is_minmax_compact = [&] {
        const auto variant = settings.value("minmax_compact");
        if (!variant.canConvert<bool>()) {
            return false;
        }

        return variant.toBool();
    }();

    if (!is_minmax_compact) {
        return;
    }

    const auto is_minmax_visible = [&] {
        const auto variant = settings.value("minmax_visible");
        if (!variant.canConvert<bool>()) {
            return true;
        }

        return variant.toBool();
    }();

    if (is_minmax_visible) {
        deps_.ui_components.ui->gridLayout_toolbar->addWidget(
            deps_.ui_components.ui->linkViewsToggle, 0, 0);
        deps_.ui_components.ui->gridLayout_toolbar->addWidget(
            deps_.ui_components.ui->reposition_buffer, 0, 1);
        deps_.ui_components.ui->gridLayout_toolbar->addWidget(
            deps_.ui_components.ui->go_to_pixel, 0, 2);

        deps_.ui_components.ui->gridLayout_toolbar->addWidget(
            deps_.ui_components.ui->rotate_90_ccw, 1, 0);
        deps_.ui_components.ui->gridLayout_toolbar->addWidget(
            deps_.ui_components.ui->rotate_90_cw, 1, 1);
        deps_.ui_components.ui->gridLayout_toolbar->addWidget(
            deps_.ui_components.ui->acToggle, 1, 2);
        deps_.ui_components.ui->gridLayout_toolbar->addWidget(
            deps_.ui_components.ui->decrease_float_precision, 1, 3);
        deps_.ui_components.ui->gridLayout_toolbar->addWidget(
            deps_.ui_components.ui->increase_float_precision, 1, 4);
    }

    deps_.ui_components.ui->horizontalLayout_container_toolbar->addWidget(
        deps_.ui_components.ui->minMaxEditor, 2);
    deps_.ui_components.ui->horizontalLayout_container_toolbar->setStretch(0,
                                                                           0);
    deps_.ui_components.ui->horizontalLayout_container_toolbar->setStretch(1,
                                                                           1);
    deps_.ui_components.ui->horizontalLayout_container_toolbar->setStretch(2,
                                                                           0);

    deps_.ui_components.ui->acEdit->hide();
}

QString SettingsManager::initialize_settings_ui_colorspace_channel(
    const QChar& character)
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

void SettingsManager::initialize_settings_ui_colorspace(
    const QSettings& settings)
{
    const auto variant = settings.value("colorspace");
    if (!variant.canConvert<QString>()) {
        return;
    }

    const auto colorspace_str = variant.toString();

    if (!colorspace_str.isEmpty()) {
        deps_.channel_names.name_channel_1 =
            initialize_settings_ui_colorspace_channel(colorspace_str.at(0));
    }
    if (colorspace_str.size() > 1) {
        deps_.channel_names.name_channel_2 =
            initialize_settings_ui_colorspace_channel(colorspace_str.at(1));
    }
    if (colorspace_str.size() > 2) {
        deps_.channel_names.name_channel_3 =
            initialize_settings_ui_colorspace_channel(colorspace_str.at(2));
    }
    if (colorspace_str.size() > 3) {
        deps_.channel_names.name_channel_4 =
            initialize_settings_ui_colorspace_channel(colorspace_str.at(3));
    }
}

void SettingsManager::initialize_settings_ui_minmax_visible(
    const QSettings& settings) const
{
    const auto variant = settings.value("minmax_visible");
    if (!variant.canConvert<bool>()) {
        return;
    }

    const auto is_minmax_visible = variant.toBool();
    deps_.ui_components.ui->acEdit->setChecked(is_minmax_visible);
    deps_.ui_components.ui->minMaxEditor->setVisible(is_minmax_visible);
}

void SettingsManager::initialize_settings_ui_contrast_enabled(
    const QSettings& settings)
{
    const auto variant = settings.value("contrast_enabled");
    if (!variant.canConvert<bool>()) {
        return;
    }

    deps_.state.ac_enabled = variant.toBool();
    deps_.ui_components.ui->acToggle->setChecked(deps_.state.ac_enabled);
    deps_.ui_components.ui->minMaxEditor->setEnabled(deps_.state.ac_enabled);
}

void SettingsManager::initialize_settings_ui_link_views_enabled(
    const QSettings& settings)
{
    const auto variant = settings.value("link_views_enabled");
    if (!variant.canConvert<bool>()) {
        return;
    }

    deps_.state.link_views_enabled = variant.toBool();
    deps_.ui_components.ui->linkViewsToggle->setChecked(
        deps_.state.link_views_enabled);
}

} // namespace oid
