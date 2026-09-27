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

#include "platform/app_services.h"

#include <filesystem>
#include <optional>
#include <string>

#include "host/io/imgui_buffer_exporter.h"
#include "host/ipc/ipc_client.h"
#include "host/settings/config_path.h"
#include "host/settings/settings_store.h"
#include "host/ui/export_dialog.h"
#include "visualization/components/buffer.h"

namespace oid::platform {

void install_platform_hooks() {
    // No-op on native: the inbound host message hook exists only for the
    // non-native (postMessage) embedding, which installs it before polling.
}

void register_agent_targets(oid::host::IpcBufferModel& /*model*/,
                            oid::host::StageManager& /*stages*/,
                            oid::host::UiState& /*ui*/,
                            std::shared_ptr<RenderCanvas> /*canvas*/) {
    // No-op on native: main.cpp assembles the agent endpoint directly; only
    // a non-native port needs these handed across the platform seam.
}

struct SettingsBackend::Impl {
    host::SettingsStore store{host::config_file_path()};
};

SettingsBackend::SettingsBackend() : impl_{std::make_unique<Impl>()} {}
SettingsBackend::~SettingsBackend() = default;

host::AppSettings SettingsBackend::load() const {
    // load() never throws: a missing/corrupt settings file just yields
    // AppSettings{} defaults, so this never blocks startup.
    return impl_->store.load();
}

host::SettingsScope SettingsBackend::scope() const {
    return host::SettingsScope::FULL;
}

std::function<void(const host::AppSettings&)>
SettingsBackend::make_save_sink(host::IpcClient& /*ipc*/) const {
    return [this](const host::AppSettings& a) { impl_->store.save(a); };
}

SessionBridge::SessionBridge(
    host::IpcClient& /*ipc*/,
    const std::function<void(const host::AppSettings&)>& /*apply*/,
    const std::function<void()>& /*open_export*/) {}

bool confirm_export(host::ExportDialogState& dialog) {
    // The blocking OS save dialog resolves this frame; consume the flag now.
    if (!dialog.open) {
        return false;
    }
    dialog.open = false;

    // open_export_dialog seeded path_buf with "<dir>/<name>.<ext>"; split it
    // back into what the nfd save dialog wants as defaults.
    const std::filesystem::path seeded{dialog.path_buf.data()};

    const std::optional<std::string> chosen = request_save_path(
        seeded.parent_path().string(), seeded.filename().string());
    if (!chosen.has_value()) {
        return false;
    }

    dialog.format = host::classify_export_format(*chosen);
    host::set_export_path(
        dialog, host::ensure_export_extension(*chosen, dialog.format));

    return true;
}

bool perform_export(const Buffer& buffer,
                    host::ExportDialogState& dialog,
                    host::IpcClient& /*ipc*/,
                    std::string& status_message,
                    std::string& last_export_dir) {
    const bool ok = host::export_buffer_imgui(
        buffer, dialog.path_buf.data(), dialog.format);
    const std::string path{dialog.path_buf.data()};
    status_message =
        (ok ? std::string{"Exported "} : std::string{"Export failed: "}) + path;
    if (ok) {
        last_export_dir = std::filesystem::path{path}.parent_path().string();
    }
    return ok;
}

} // namespace oid::platform
