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

#ifndef IPC_POSTMESSAGE_TRANSPORT_H_
#define IPC_POSTMESSAGE_TRANSPORT_H_

#include <deque>
#include <mutex>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <cstdint>
#include <emscripten.h>
#endif

#include "ipc/transport.h"

namespace oid {

class PostMessageTransport final : public ITransport {
  public:
    void send(std::span<const std::byte> data) override;
    std::size_t receive(std::span<std::byte> dst) override;
    bool has_data() const override;

    void enqueue_inbound(std::vector<std::byte> data);

  private:
    mutable std::mutex mutex_;
    std::deque<std::byte> inbound_;
};

#ifdef __EMSCRIPTEN__
extern "C" {
void oid_set_postmessage_transport(PostMessageTransport* transport);
EMSCRIPTEN_KEEPALIVE void oidEnqueueInbound(std::uintptr_t ptr, unsigned len);
}
#endif

} // namespace oid

#endif // IPC_POSTMESSAGE_TRANSPORT_H_
