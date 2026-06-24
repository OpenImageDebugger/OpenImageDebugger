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

#include "ipc/tcp_transport.h"

#include <bit>

#include <QTcpSocket>

#include "ipc/message_exchange.h"

namespace oid {

TcpTransport::TcpTransport(QTcpSocket& socket) : socket_{socket} {}

void TcpTransport::send(std::span<const std::byte> data) {
    auto offset = qint64{0};
    const auto total = static_cast<qint64>(data.size());
    while (offset < total) {
        offset += socket_.write(std::bit_cast<const char*>(data.data() + offset),
                                total - offset);
        if (offset < total && !socket_.waitForBytesWritten(5000)) {
            throw_socket_timeout_error("write");
        }
    }
    if (!socket_.waitForBytesWritten(5000)) {
        throw_socket_timeout_error("write");
    }
}

std::size_t TcpTransport::receive(std::span<std::byte> dst) {
    auto offset = std::size_t{0};
    while (offset < dst.size()) {
        if (!has_data() && !socket_.waitForReadyRead(5000)) {
            throw_socket_timeout_error("read");
        }
        const auto n =
            socket_.read(std::bit_cast<char*>(dst.data() + offset),
                         static_cast<qint64>(dst.size() - offset));
        if (n < 0) {
            throw_socket_timeout_error("read");
        }
        if (n == 0) {
            break;
        }
        offset += static_cast<std::size_t>(n);
    }
    return offset;
}

bool TcpTransport::has_data() const {
    return socket_.bytesAvailable() > 0;
}

} // namespace oid
