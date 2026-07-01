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

#include "session_state_codec.h"

#include "ui/main_window/settings_applier.h"

namespace oid {

namespace {

QString colorspace_channel_from_char(const QChar& character) {
    switch (character.toLatin1()) {
    case 'b':
        return QStringLiteral("blue");
    case 'g':
        return QStringLiteral("green");
    case 'r':
        return QStringLiteral("red");
    case 'a':
        return QStringLiteral("alpha");
    default:
        return {};
    }
}

} // namespace

void apply_session_state_fields(const SessionStateFields& fields,
                                SettingsApplier& applier) {
    if (fields.framerate.has_value()) {
        applier.apply_rendering_settings(*fields.framerate);
    }
    if (fields.default_export_suffix.has_value()) {
        applier.apply_export_settings(*fields.default_export_suffix);
    }
    if (fields.list_position.has_value()) {
        applier.apply_ui_list_position(*fields.list_position);
    }
    if (fields.splitter_sizes.has_value()) {
        applier.apply_ui_splitter_sizes(*fields.splitter_sizes);
    }
    if (fields.minmax_compact.has_value() && fields.minmax_visible.has_value()) {
        applier.apply_ui_minmax_compact(*fields.minmax_compact,
                                        *fields.minmax_visible);
    } else if (fields.minmax_visible.has_value()) {
        applier.apply_ui_minmax_visible(*fields.minmax_visible);
    }
    if (fields.colorspace.has_value()) {
        const auto& colorspace_str = *fields.colorspace;
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
        applier.apply_ui_colorspace(ch1, ch2, ch3, ch4);
    }
    if (fields.contrast_enabled.has_value()) {
        applier.apply_ui_contrast_enabled(*fields.contrast_enabled);
    }
    if (fields.link_views_enabled.has_value()) {
        applier.apply_ui_link_views_enabled(*fields.link_views_enabled);
    }
}

} // namespace oid
