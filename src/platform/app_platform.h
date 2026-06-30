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

#ifndef PLATFORM_APP_PLATFORM_H_
#define PLATFORM_APP_PLATFORM_H_

#include "ui/main_window/main_window.h"

namespace oid::platform {

/// Set up the default QSurfaceFormat (GLES 3.0 on WASM; no-op on native).
void configure_surface_format();

/// Parse -h / -p command-line arguments into a ConnectionSettings.
/// On WASM there are no CLI args — returns a default-constructed value.
ConnectionSettings parse_connection_settings(int argc, char** argv);

/// Install JS inbound message hooks (oidOnMessage).
/// No-op on native.
void install_inbound_hooks();

/// WASM: emit oid_notify_viewer_ready exactly once when both flags are true.
/// Native: no-op.
void notify_viewer_ready_once(bool window_ready, bool first_paint_done);

} // namespace oid::platform

#endif // PLATFORM_APP_PLATFORM_H_
