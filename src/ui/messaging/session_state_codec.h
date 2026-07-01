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

#ifndef SESSION_STATE_CODEC_H_
#define SESSION_STATE_CODEC_H_

#include <functional>
#include <optional>

#include <QByteArray>
#include <QList>
#include <QString>

#include "ui/main_window/settings_manager.h"

namespace oid {

class SettingsApplier;

struct SessionStateFields {
    std::optional<double> framerate;
    std::optional<QString> default_export_suffix;
    std::optional<QList<int>> splitter_sizes;
    std::optional<QString> list_position;
    std::optional<bool> minmax_visible;
    std::optional<bool> minmax_compact;
    std::optional<bool> contrast_enabled;
    std::optional<bool> link_views_enabled;
    std::optional<QString> colorspace;
};

struct SessionStateExtraInputs {
    std::function<QString()> getListPosition;
    std::function<QString()> getColorspace;
};

bool parse_session_state_json(const QByteArray& json, SessionStateFields& out);

void apply_session_state_fields(const SessionStateFields& fields,
                                SettingsApplier& applier);

QByteArray serialize_session_state_delta(
    const SettingsManager::DataCallbacks& callbacks,
    const SessionStateExtraInputs& extra);

} // namespace oid

#endif // SESSION_STATE_CODEC_H_
