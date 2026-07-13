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

// No-op on native: there is no inbound host message hook to wire here --
// that only exists for the non-native (postMessage) embedding, which must
// install it before its transport starts polling.
void install_platform_hooks() {
    /* no-op on native: no inbound host message hook to wire here; see comment
     * above */
}

struct SettingsBackend::Impl {
    oid::host::SettingsStore store{oid::host::config_file_path()};
};

SettingsBackend::SettingsBackend() : impl_{std::make_unique<Impl>()} {}
SettingsBackend::~SettingsBackend() = default;

oid::host::AppSettings SettingsBackend::load() const {
    // load() never throws: a missing/corrupt settings file just yields
    // AppSettings{} defaults, so this never blocks startup.
    return impl_->store.load();
}

std::function<void(const oid::host::AppSettings&)>
SettingsBackend::make_save_sink(oid::host::IpcClient& /*ipc*/) const {
    return [this](const oid::host::AppSettings& a) { impl_->store.save(a); };
}

SessionBridge::SessionBridge(
    oid::host::IpcClient& /*ipc*/,
    const std::function<void(const oid::host::AppSettings&)>& /*apply*/,
    const std::function<void()>& /*open_export*/) {}

bool confirm_export(oid::host::ExportDialogState& dialog) {
    // A pending open request (set by open_export_dialog from the buffer-list
    // context menu) resolves this frame via the blocking OS save dialog, so
    // the one-shot flag is consumed up front. Runs from handle_export_requests
    // -- the same top-level ImGui point after panels draw -- as the
    // request_open_files call for File > Open.
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

    dialog.format = oid::host::classify_export_format(*chosen);
    oid::host::set_export_path(
        dialog, oid::host::ensure_export_extension(*chosen, dialog.format));

    return true;
}

bool perform_export(const oid::Buffer& buffer,
                    oid::host::ExportDialogState& dialog,
                    oid::host::IpcClient& /*ipc*/,
                    std::string& status_message,
                    std::string& last_export_dir) {
    const bool ok = oid::host::export_buffer_imgui(
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
