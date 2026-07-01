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

#include "platform/app_platform.h"

#include <emscripten.h>

#include <QSurfaceFormat>

// JS hook: installs window.oidOnMessage so the VS Code extension can push
// buffers into the WASM module via Module._oidEnqueueInbound.
EM_JS(void, oid_install_js_hooks, (), {
  window.oidOnMessage = function(buf) {
    let arr;
    if (buf instanceof Uint8Array) {
      arr = buf;
    } else if (Array.isArray(buf)) {
      arr = new Uint8Array(buf);
    } else {
      arr = new Uint8Array(Object.values(buf));
    }
    const len = arr.length;
    const ptr = _malloc(len);
    HEAPU8.set(arr, ptr);
    Module._oidEnqueueInbound(ptr, len);
    _free(ptr);
  };
});

// Notifies the VS Code extension that the OID viewer is ready to receive
// buffer data.  Fired at most once per session.
EM_JS(void, oid_notify_viewer_ready, (), {
  const msg = {type: 'oid-control', event: 'viewer-ready', version: 'dev'};
  if (typeof window.oidOnViewerReady === 'function') {
    window.oidOnViewerReady(msg);
  }
  if (typeof window.dispatchEvent === 'function') {
    window.dispatchEvent(new CustomEvent('oid-viewer-ready', {detail: msg}));
  }
});

namespace oid::platform {

void configure_surface_format() {
    auto format = QSurfaceFormat{};
    format.setRenderableType(QSurfaceFormat::OpenGLES);
    format.setVersion(3, 0);
    format.setProfile(QSurfaceFormat::CoreProfile);
    QSurfaceFormat::setDefaultFormat(format);
}

ConnectionSettings parse_connection_settings(int /*argc*/, char** /*argv*/) {
    // WASM has no command-line arguments; connection is managed by the
    // VS Code extension via postMessage.
    return ConnectionSettings{};
}

void install_inbound_hooks() {
    oid_install_js_hooks();
}

void notify_viewer_ready_once(bool window_ready, bool first_paint_done) {
    static auto sent = false;
    if (!sent && window_ready && first_paint_done) {
        oid_notify_viewer_ready();
        sent = true;
    }
}

} // namespace oid::platform
