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

#include "postmessage_transport.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

EM_JS(void, oid_send_to_js, (const void* ptr, int len), {
  window.oidSend(HEAPU8.slice(ptr, ptr + len));
});
#endif

namespace oid {

void PostMessageTransport::send(std::span<const std::byte> data) {
#ifdef __EMSCRIPTEN__
    oid_send_to_js(data.data(), static_cast<int>(data.size()));
#else
    (void)data;
#endif
}

std::size_t PostMessageTransport::receive(std::span<std::byte> dst) {
    const std::scoped_lock lock{mutex_};
    std::size_t n = 0;
    while (n < dst.size() && !inbound_.empty()) {
        dst[n++] = inbound_.front();
        inbound_.pop_front();
    }
    return n;
}

bool PostMessageTransport::has_data() const {
    const std::scoped_lock lock{mutex_};
    return !inbound_.empty();
}

void PostMessageTransport::enqueue_inbound(std::vector<std::byte> data) {
    const std::scoped_lock lock{mutex_};
    inbound_.insert(inbound_.end(), data.begin(), data.end());
}

#ifdef __EMSCRIPTEN__
namespace {
PostMessageTransport* g_transport = nullptr;
}

extern "C" void oid_set_postmessage_transport(PostMessageTransport* transport) {
    g_transport = transport;
}

extern "C" void oidEnqueueInbound(const std::uintptr_t ptr, const int len) {
    if (g_transport == nullptr || len <= 0) {
        return;
    }
    const auto* data = reinterpret_cast<const std::byte*>(ptr);
    g_transport->enqueue_inbound({data, data + len});
}
#endif

} // namespace oid
