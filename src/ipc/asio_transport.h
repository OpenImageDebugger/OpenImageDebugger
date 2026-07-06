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

#ifndef IPC_ASIO_TRANSPORT_H_
#define IPC_ASIO_TRANSPORT_H_

#include <chrono>
#include <cstddef>
#include <memory>
#include <span>
#include <string>

#include <asio.hpp>

#include "transport.h"

namespace oid {

// Standalone-Asio TCP transport implementing ITransport, usable on both
// ends of the connection. Reproduces TcpTransport's semantics: send blocks
// until all bytes are handed to the OS (throws on error), receive is a
// best-effort single read that may return a short count and returns 0 only
// when the peer sends nothing / disconnects (the MessageDecoder read loop
// escalates a 0 to a timeout), has_data is a non-blocking peek. All
// blocking operations (connect/receive) are bounded by `timeout_`, on which
// they throw SocketTimeoutError -- except the client ctor's connect, which
// matches the old Qt-based TcpTransport's graceful-degrade behavior (see
// below).
class AsioTransport final : public ITransport {
  public:
    // Client: owns its io_context; resolve+connect with a bounded deadline.
    // Does NOT throw on connect failure (timeout, refused, resolve error):
    // instead it closes the socket and marks disconnected_, leaving
    // construction to complete so callers (e.g. MainWindow's ctor) can rely
    // on should_quit_on_disconnect()/is_connected() to notice on the next
    // tick, exactly as the old TcpTransport did when its blocking
    // wait-for-connected call failed.
    AsioTransport(const std::string& host,
                  unsigned short port,
                  std::chrono::milliseconds timeout = std::chrono::seconds{5});

    // Server: adopt an already-accepted socket bound to `ctx` (the
    // acceptor's io_context). LIFETIME: `ctx` is NOT owned by this
    // transport and must outlive it -- it is normally the AsioAcceptor's
    // io_context_, so the AsioAcceptor must outlive every AsioTransport it
    // produced via accept().
    AsioTransport(asio::io_context& ctx,
                  asio::ip::tcp::socket&& sock,
                  std::chrono::milliseconds timeout = std::chrono::seconds{5});

    void send(std::span<const std::byte> data) override;
    // Bounded single read; throws SocketTimeoutError if nothing arrives
    // within timeout_. Returns 0 on EOF/error (sets disconnected()) so
    // MessageDecoder::read_impl escalates it.
    std::size_t receive(std::span<std::byte> dst) override;
    [[nodiscard]] bool has_data() const override;

    // True once receive() has observed EOF/an error on the socket.
    [[nodiscard]] bool disconnected() const;

    // Active liveness probe: a non-blocking MSG_PEEK read. would_block means
    // the peer is still alive with nothing pending; EOF/error/0 means the
    // peer closed (latches disconnected_); data pending also means
    // connected. Needed because has_data()/available() reports 0 with no
    // error on a graceful close, so a peer that disconnects without ever
    // sending data would otherwise go undetected.
    [[nodiscard]] bool is_connected() const;

    // Overrides the bounded-op deadline (e.g. to speed up tests).
    void set_timeout(std::chrono::milliseconds timeout);

  private:
    // asio::io_context is non-copyable AND non-movable, so the client owns
    // it via a unique_ptr (which is movable); the server (adopt) path
    // leaves owned_ctx_ null and points ctx_ at the acceptor's context.
    // ctx_ is a pointer (not a reference) so AsioTransport itself stays
    // movable, since AsioAcceptor::accept() returns one by value.
    std::unique_ptr<asio::io_context>
        owned_ctx_;         // non-null only for the client
    asio::io_context* ctx_; // -> owned_ctx_ or the acceptor's
    // mutable: the const is_connected() liveness probe toggles the socket's
    // non-blocking mode and issues a MSG_PEEK receive, neither of which
    // asio exposes as const, even though they don't change observable
    // transport state.
    mutable asio::ip::tcp::socket socket_; // constructed on *ctx_
    std::chrono::milliseconds timeout_;
    // mutable: latched by the const is_connected() liveness probe on
    // EOF/error, in addition to the non-const receive() path.
    mutable bool disconnected_{false};
};

// Standalone-Asio TCP listener producing AsioTransport instances for the
// server side of the connection. Binds an ephemeral port on all
// interfaces; callers query port() to learn what it picked.
class AsioAcceptor {
  public:
    AsioAcceptor();

    [[nodiscard]] unsigned short port() const;

    // Bounded accept; throws SocketTimeoutError if no peer connects within
    // `timeout`. The returned AsioTransport references THIS acceptor's
    // io_context (it does not own it), so `this` must outlive it.
    AsioTransport accept(std::chrono::milliseconds timeout);

  private:
    asio::io_context
        io_context_; // owned; must outlive every accepted transport
    asio::ip::tcp::acceptor acceptor_;
};

} // namespace oid

#endif // IPC_ASIO_TRANSPORT_H_
