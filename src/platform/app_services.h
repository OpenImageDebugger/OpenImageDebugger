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

#ifndef PLATFORM_APP_SERVICES_H_
#define PLATFORM_APP_SERVICES_H_

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "host/settings/app_settings.h"
#include "visualization/render_canvas.h"

struct GLFWwindow;

namespace oid {
class Buffer;
} // namespace oid

namespace oid::host {
class IpcClient;
class IpcBufferModel;
class StageManager;
class UiState;
struct ExportDialogState;
} // namespace oid::host

namespace oid::platform {

// Platform seam: application bootstrap policy that differs between the native
// app and the non-native embedding host.

struct Endpoint {
    std::string host{"127.0.0.1"};
    unsigned short port{9588};
};

// Non-native: wire the inbound message hook (must run before transport
// polling starts). Native: no-op.
void install_platform_hooks();

// Non-native: sink for host-pushed file bytes; call once before the main loop.
void register_file_open_sink(host::IpcBufferModel& model);

// Non-native: hands the port the model/stages/ui/canvas; call once pre-loop.
void register_agent_targets(oid::host::IpcBufferModel& model,
                            oid::host::StageManager& stages,
                            oid::host::UiState& ui,
                            std::shared_ptr<RenderCanvas> canvas);

// Settings persistence backend. Native: on-disk JSON store at the platform
// config path. Non-native: no local file -- load() yields defaults (real
// state arrives as a session-state update from the host) and the save sink
// pushes a session-state-changed message over IPC.
class SettingsBackend {
  public:
    SettingsBackend();
    ~SettingsBackend();
    SettingsBackend(const SettingsBackend&) = delete;
    SettingsBackend& operator=(const SettingsBackend&) = delete;

    [[nodiscard]] host::AppSettings load() const;

    // A wrong scope silently feeds the host back state it already owns.
    [[nodiscard]] host::SettingsScope scope() const;

    [[nodiscard]] std::function<void(const host::AppSettings&)>
    make_save_sink(host::IpcClient& ipc) const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Session-state bridge: wires platform-specific IPC callbacks and gates
// settings persistence. `apply` runs when the platform delivers a session
// state (non-native: the host's mid-session session-state update).
// `open_export` runs when the platform requests exporting the current
// selection (non-native: the host's "export selected buffer" command).
// Native wires nothing.
class SessionBridge {
  public:
    SessionBridge(host::IpcClient& ipc,
                  const std::function<void(const host::AppSettings&)>& apply,
                  const std::function<void()>& open_export);

    // False until persisting is safe. Non-native: flips true on the first
    // real session-state update -- the host always sends one when the viewer
    // signals ready; saving earlier would echo a default snapshot back and
    // overwrite the persisted buffer list. Native: always true.
    [[nodiscard]] bool can_persist() const {
        return can_persist_;
    }

  private:
    // The in-class initializer is the value the native build relies on; a
    // non-native SessionBridge ctor must override it explicitly if it
    // cannot persist.
    bool can_persist_ = true;
};

// True when the export dialog confirmed this frame. Native draws the ImGui
// modal. Non-native consumes dialog.open directly: there is no in-viewer
// dialog, the host shows the OS save dialog instead.
bool confirm_export(host::ExportDialogState& dialog);

// Perform a confirmed export of `buffer`. Native writes the file at
// dialog.path_buf and updates last_export_dir from it. Non-native hands the
// buffer to the host over IPC (with the live auto-contrast array). Sets
// status_message either way; returns success.
bool perform_export(const Buffer& buffer,
                    host::ExportDialogState& dialog,
                    host::IpcClient& ipc,
                    std::string& status_message,
                    std::string& last_export_dir);

// Multi-select OS open dialog; empty on cancel. `window` may be null.
[[nodiscard]] std::vector<std::string> request_open_files(GLFWwindow* window);

// OS save dialog; nullopt on cancel or when the dialog cannot open.
// Native-only: no non-native port has it.
[[nodiscard]] std::optional<std::string>
request_save_path(const std::string& default_dir,
                  const std::string& default_name);

} // namespace oid::platform

#endif // PLATFORM_APP_SERVICES_H_
