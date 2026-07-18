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

#include "asio_transport.h"

#include "message_exchange.h" // throw_socket_timeout_error

namespace oid {

namespace {
// Runs `ctx` until `done` flips true or `timeout` elapses; on timeout
// cancels `cancellable` (the socket or acceptor the pending async op was
// posted on) so the handler completes (with an operation_aborted error),
// then drains it. Returns whether it timed out. Templated because the
// cancel target differs per call site: a socket for connect/receive, the
// acceptor itself for accept (the accepted-into socket has no pending op
// of its own until the accept completes).
template <typename Cancellable>
bool run_until(asio::io_context& ctx,
               Cancellable& cancellable,
               const bool& done,
               const std::chrono::milliseconds timeout) {
    ctx.restart();
    ctx.run_for(timeout);
    if (!done) { // deadline hit before the op completed
        asio::error_code ignore;
        cancellable.cancel(ignore);
        ctx.run(); // let the cancelled handler run so its captures are safe to
                   // destroy
        return true; // timed out
    }
    return false;
}
} // namespace

AsioTransport::AsioTransport(const std::string& host,
                             const unsigned short port,
                             const std::chrono::milliseconds timeout)
    : owned_ctx_{std::make_unique<asio::io_context>()}, ctx_{owned_ctx_.get()},
      socket_{*ctx_}, timeout_{timeout} {
    // NOTE: connect failure here must NOT throw -- it must leave the socket
    // closed and disconnected_ set, matching the old Qt-based TcpTransport,
    // which never checked its wait-for-connected result and let the app
    // self-quit on the first loop() tick via should_quit_on_disconnect.
    // Throwing here would escape MainWindow's ctor and crash the process
    // instead.
    asio::ip::tcp::resolver resolver{*ctx_};
    asio::error_code ec;
    const auto endpoints = resolver.resolve(host, std::to_string(port), ec);
    if (ec) {
        disconnected_ = true;
        return;
    }

    bool done = false;
    asio::error_code op_ec;
    asio::async_connect(socket_,
                        endpoints,
                        [&op_ec, &done](const asio::error_code& e,
                                        const asio::ip::tcp::endpoint&) {
                            op_ec = e;
                            done = true;
                        });
    if (run_until(*ctx_, socket_, done, timeout_) || op_ec) {
        disconnected_ = true;
        asio::error_code close_ec;
        socket_.close(close_ec);
    }
}

AsioTransport::AsioTransport(asio::io_context& ctx,
                             asio::ip::tcp::socket&& sock,
                             const std::chrono::milliseconds timeout)
    : owned_ctx_{nullptr}, ctx_{&ctx}, socket_{std::move(sock)},
      timeout_{timeout} {}

void AsioTransport::send(const std::span<const std::byte> data) {
    asio::error_code ec;
    // asio::write loops until all bytes are written or an error occurs.
    asio::write(socket_, asio::buffer(data.data(), data.size()), ec);
    if (ec) {
        throw_socket_timeout_error("write");
    }
}

std::size_t AsioTransport::receive(const std::span<std::byte> dst) {
    bool done = false;
    asio::error_code op_ec;
    std::size_t n = 0;
    socket_.async_read_some(
        asio::buffer(dst.data(), dst.size()),
        [&op_ec, &n, &done](const asio::error_code& e, std::size_t got) {
            op_ec = e;
            n = got;
            done = true;
        });
    if (run_until(*ctx_, socket_, done, timeout_)) {
        throw_socket_timeout_error("read");
    }
    if (op_ec) {
        // EOF/error -> read_impl escalates the 0 to a timeout.
        disconnected_ = true;
        return 0;
    }
    return n;
}

bool AsioTransport::has_data() const {
    asio::error_code ec;
    return socket_.available(ec) > 0 && !ec;
}

bool AsioTransport::disconnected() const {
    return disconnected_;
}

bool AsioTransport::is_connected() const {
    if (disconnected_ || !socket_.is_open()) {
        return false;
    }
    asio::error_code ec;
    const bool prev_nb = socket_.non_blocking();
    socket_.non_blocking(true, ec);
    char probe = 0;
    const std::size_t n = socket_.receive(
        asio::buffer(&probe, 1), asio::socket_base::message_peek, ec);
    asio::error_code restore_ec;
    socket_.non_blocking(prev_nb, restore_ec);
    if (ec == asio::error::would_block) {
        return true; // connected, nothing to read yet
    }
    if (ec || n == 0) {
        disconnected_ = true; // EOF/error -> peer gone
        return false;
    }
    return true; // data pending -> connected
}

void AsioTransport::set_timeout(const std::chrono::milliseconds timeout) {
    timeout_ = timeout;
}

AsioAcceptor::AsioAcceptor()
    : acceptor_{io_context_, asio::ip::tcp::endpoint{asio::ip::tcp::v4(), 0}} {}

unsigned short AsioAcceptor::port() const {
    return acceptor_.local_endpoint().port();
}

AsioTransport AsioAcceptor::accept(const std::chrono::milliseconds timeout) {
    asio::ip::tcp::socket sock{io_context_};

    bool done = false;
    asio::error_code op_ec;
    acceptor_.async_accept(sock, [&op_ec, &done](const asio::error_code& e) {
        op_ec = e;
        done = true;
    });
    if (run_until(io_context_, acceptor_, done, timeout) || op_ec) {
        throw_socket_timeout_error("accept");
    }

    return AsioTransport{io_context_, std::move(sock), timeout};
}

} // namespace oid
