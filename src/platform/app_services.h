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
#include <string>
#include <vector>

#include "host/settings/app_settings.h"

struct GLFWwindow;

namespace oid {
class Buffer;
} // namespace oid

namespace oid::host {
class IpcClient;
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

    [[nodiscard]] oid::host::AppSettings load() const;

    [[nodiscard]] std::function<void(const oid::host::AppSettings&)>
    make_save_sink(oid::host::IpcClient& ipc) const;

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
    SessionBridge(
        oid::host::IpcClient& ipc,
        const std::function<void(const oid::host::AppSettings&)>& apply,
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
bool confirm_export(oid::host::ExportDialogState& dialog);

// Perform a confirmed export of `buffer`. Native writes the file at
// dialog.path_buf and updates last_export_dir from it. Non-native hands the
// buffer to the host over IPC (with the live auto-contrast array). Sets
// status_message either way; returns success.
bool perform_export(const oid::Buffer& buffer,
                    oid::host::ExportDialogState& dialog,
                    oid::host::IpcClient& ipc,
                    std::string& status_message,
                    std::string& last_export_dir);

// Shows a native OS file-selection dialog (multi-select) filtered to the
// image and .npy formats the viewer supports. Returns the chosen absolute
// paths, or an empty vector if the user cancels. `window` may be null; the
// dialog is shown unparented either way.
[[nodiscard]] std::vector<std::string> request_open_files(GLFWwindow* window);

} // namespace oid::platform

#endif // PLATFORM_APP_SERVICES_H_
