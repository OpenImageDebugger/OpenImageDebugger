/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2026 OpenImageDebugger contributors
 * (https://github.com/OpenImageDebugger/OpenImageDebugger)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated files (the "Software"), to
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

#include "settings_applier.h"

#include <utility>

#include <QWidget>

#include "main_window.h"

namespace oid
{

SettingsApplier::SettingsApplier(Dependencies deps, QObject* parent)
    : QObject{parent}
    , deps_{std::move(deps)}
{
}

void SettingsApplier::apply_rendering_settings(double framerate)
{
    deps_.render_framerate = framerate;
}

void SettingsApplier::apply_export_settings(QString defaultSuffix)
{
    deps_.default_export_suffix = defaultSuffix;
}

void SettingsApplier::apply_window_geometry(QSize size, QPoint pos)
{
    // Window is loaded with a fixed size and restored in timer.
    // This is needed to give application some time to run event loop
    // and redraw all widgets without changing overall geometry.
    if (size.isValid()) {
        deps_.main_window.setFixedSize(size);
    }
    if (!pos.isNull()) {
        deps_.main_window.move(pos);
    }
}

void SettingsApplier::restore_window_resize()
{
    // Restore possibility to resize application in timer.
    // This is needed to give application some time to run event loop
    // and redraw all widgets without changing overall geometry.
    deps_.main_window.setMinimumSize(0, 0);
    deps_.main_window.setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
}

void SettingsApplier::apply_ui_list_position(QString position)
{
    if (position == "top" || position == "bottom") {
        deps_.ui_components.ui->splitter->setOrientation(Qt::Vertical);
    }

    if (position == "right" || position == "bottom") {
        deps_.ui_components.ui->splitter->insertWidget(
            -1, deps_.ui_components.ui->frame_list);
    }

    deps_.ui_components.ui->splitter->repaint();
}

void SettingsApplier::apply_ui_splitter_sizes(QList<int> sizes)
{
    if (!sizes.isEmpty()) {
        deps_.ui_components.ui->splitter->setSizes(sizes);
    }
}

void SettingsApplier::apply_ui_minmax_compact(bool compact, bool visible)
{
    if (!compact) {
        return;
    }

    if (visible) {
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

void SettingsApplier::apply_ui_colorspace(QString ch1,
                                          QString ch2,
                                          QString ch3,
                                          QString ch4)
{
    if (!ch1.isEmpty()) {
        deps_.channel_names.name_channel_1 = ch1;
    }
    if (!ch2.isEmpty()) {
        deps_.channel_names.name_channel_2 = ch2;
    }
    if (!ch3.isEmpty()) {
        deps_.channel_names.name_channel_3 = ch3;
    }
    if (!ch4.isEmpty()) {
        deps_.channel_names.name_channel_4 = ch4;
    }
}

void SettingsApplier::apply_ui_minmax_visible(bool visible)
{
    deps_.ui_components.ui->acEdit->setChecked(visible);
    deps_.ui_components.ui->minMaxEditor->setVisible(visible);
}

void SettingsApplier::apply_ui_contrast_enabled(bool enabled)
{
    deps_.state.ac_enabled = enabled;
    deps_.ui_components.ui->acToggle->setChecked(enabled);
    deps_.ui_components.ui->minMaxEditor->setEnabled(enabled);
}

void SettingsApplier::apply_ui_link_views_enabled(bool enabled)
{
    deps_.state.link_views_enabled = enabled;
    deps_.ui_components.ui->linkViewsToggle->setChecked(enabled);
}

void SettingsApplier::apply_previous_session_buffers(QStringList buffers)
{
    const auto lock = std::unique_lock{deps_.ui_mutex};
    deps_.buffer_data.previous_session_buffers.clear();
    for (const auto& buffer : buffers) {
        deps_.buffer_data.previous_session_buffers.insert(buffer.toStdString());
    }
}

} // namespace oid
