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

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

namespace oid {

namespace {

std::optional<bool> read_bool(const QJsonObject& obj, const char* key) {
    if (!obj.contains(key)) {
        return std::nullopt;
    }
    return obj.value(key).toBool();
}

std::optional<double> read_double(const QJsonObject& obj, const char* key) {
    if (!obj.contains(key)) {
        return std::nullopt;
    }
    return obj.value(key).toDouble();
}

std::optional<QString> read_string(const QJsonObject& obj, const char* key) {
    if (!obj.contains(key)) {
        return std::nullopt;
    }
    return obj.value(key).toString();
}

} // namespace

bool parse_session_state_json(const QByteArray& json, SessionStateFields& out) {
    const auto doc = QJsonDocument::fromJson(json);
    if (!doc.isObject()) {
        return false;
    }

    const auto root = doc.object();

    if (const auto rendering = root.value(QStringLiteral("rendering")).toObject();
        !rendering.isEmpty()) {
        out.framerate = read_double(rendering, "framerate");
    }

    if (const auto export_group = root.value(QStringLiteral("export")).toObject();
        !export_group.isEmpty()) {
        out.default_export_suffix = read_string(export_group, "defaultSuffix");
    }

    if (const auto ui = root.value(QStringLiteral("ui")).toObject(); !ui.isEmpty()) {
        out.list_position = read_string(ui, "listPosition");
        out.minmax_visible = read_bool(ui, "minmaxVisible");
        out.minmax_compact = read_bool(ui, "minmaxCompact");
        out.contrast_enabled = read_bool(ui, "contrastEnabled");
        out.link_views_enabled = read_bool(ui, "linkViewsEnabled");
        out.colorspace = read_string(ui, "colorspace");

        if (ui.contains(QStringLiteral("splitterSizes"))) {
            const auto sizes_array =
                ui.value(QStringLiteral("splitterSizes")).toArray();
            auto sizes = QList<int>{};
            for (const auto& value : sizes_array) {
                sizes.append(value.toInt());
            }
            out.splitter_sizes = sizes;
        }
    }

    return true;
}

QByteArray serialize_session_state_delta(
    const SettingsManager::DataCallbacks& callbacks,
    const SessionStateExtraInputs& extra) {
    auto rendering = QJsonObject{};
    rendering.insert(QStringLiteral("framerate"), callbacks.getRenderFramerate());

    auto exp = QJsonObject{};
    exp.insert(QStringLiteral("defaultSuffix"), callbacks.getDefaultExportSuffix());

    auto ui = QJsonObject{};
    const auto splitter_sizes = callbacks.getSplitterSizes();
    auto sizes_array = QJsonArray{};
    for (const int size : splitter_sizes) {
        sizes_array.append(size);
    }
    ui.insert(QStringLiteral("splitterSizes"), sizes_array);
    ui.insert(QStringLiteral("minmaxVisible"), callbacks.getMinMaxVisible());
    ui.insert(QStringLiteral("contrastEnabled"), callbacks.getContrastEnabled());
    ui.insert(QStringLiteral("linkViewsEnabled"), callbacks.getLinkViewsEnabled());

    if (extra.getListPosition) {
        const auto list_position = extra.getListPosition();
        if (!list_position.isEmpty()) {
            ui.insert(QStringLiteral("listPosition"), list_position);
        }
    }
    if (extra.getColorspace) {
        const auto colorspace = extra.getColorspace();
        if (!colorspace.isEmpty()) {
            ui.insert(QStringLiteral("colorspace"), colorspace);
        }
    }

    auto root = QJsonObject{};
    root.insert(QStringLiteral("rendering"), rendering);
    root.insert(QStringLiteral("export"), exp);
    root.insert(QStringLiteral("ui"), ui);
    return QJsonDocument{root}.toJson(QJsonDocument::Compact);
}

} // namespace oid
