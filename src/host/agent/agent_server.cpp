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

#include "host/agent/agent_server.h"

#include <array>
#include <atomic>
#include <chrono>
#include <exception>
#include <format>
#include <iostream>
#include <memory>
#include <random>
#include <span>
#include <stdexcept>
#include <system_error>
#include <utility>

#include "host/agent/discovery_file.h"
#include "host/agent/wire_frame.h"
#include "system/process/process_id.h"

namespace oid::host::agent {

namespace {

// Logged once per process: a throwing enqueue listener silently degrades
// wake latency to per-frame draining only, which is otherwise invisible.
void log_listener_failure_once(const char* what) {
    static std::atomic_flag logged;
    if (!logged.test_and_set()) {
        std::cerr << "[oid-agent] enqueue listener threw (" << what
                  << "); requests are still served by the per-frame drain\n";
    }
}

// Mirrors agentendpoint.py's MAX_CLIENTS: ceiling on simultaneously served
// connections; connections beyond this are closed immediately.
constexpr std::size_t MAX_CLIENTS = 8;

// Mirrors agentendpoint.py's HANDSHAKE_TIMEOUT; lifted once authenticated.
constexpr std::chrono::seconds HANDSHAKE_TIMEOUT{10};

// std::random_device is the only portable entropy source without a new dep.
// A platform CSPRNG (getrandom()/BCryptGenRandom) is a tracked follow-up.
std::string generate_token() {
    std::random_device rd;
    std::array<std::byte, 32> bytes{};
    for (std::byte& byte : bytes) {
        byte = static_cast<std::byte>(rd() & 0xFFu);
    }

    static constexpr char HEX_DIGITS[] = "0123456789abcdef";
    std::string token(bytes.size() * 2, '0');
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        const auto byte = std::to_integer<unsigned>(bytes[i]);
        token[2 * i] = HEX_DIGITS[(byte >> 4) & 0x0Fu];
        token[2 * i + 1] = HEX_DIGITS[byte & 0x0Fu];
    }
    return token;
}

// Dedicated exception type (S112); callers already catch std::exception.
class AgentServerError : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

// Loop restart()+run(): a stop() landing mid-drain makes a bare run() a no-op.
void drain_until_done(asio::io_context& ctx, const bool& done) {
    // run() == 0 means nothing ran and nothing is pending: stop, don't spin.
    while (!done) {
        ctx.restart();
        if (ctx.run() == 0) {
            break;
        }
    }
}

// Bounded pre-auth so an idle connection cannot hold a serve slot forever.
void run_async_op(asio::io_context& ctx,
                  asio::ip::tcp::socket& socket,
                  const bool& done,
                  const asio::error_code& op_ec,
                  const std::atomic<bool>& stop_requested,
                  const std::optional<std::chrono::milliseconds> timeout) {
    ctx.restart();
    if (stop_requested.load()) {
        // restart() erased a stop() that already landed; close forces the op.
        asio::error_code ignore;
        socket.close(ignore);
        drain_until_done(ctx, done);
        throw AgentServerError("agent connection stopped");
    }
    if (timeout) {
        ctx.run_for(*timeout);
        if (!done) {
            // Timed out, or stop() left ctx stopped; close forces completion.
            asio::error_code ignore;
            socket.close(ignore);
            drain_until_done(ctx, done);
            throw AgentServerError("agent handshake timed out");
        }
    } else {
        ctx.run();
        if (!done) {
            // Woken by stop()'s ctx.stop() with the op still pending: close on
            // this thread, drain the aborted handler, then unwind.
            asio::error_code ignore;
            socket.close(ignore);
            drain_until_done(ctx, done);
            throw AgentServerError("agent connection stopped");
        }
    }
    if (op_ec) {
        throw AgentServerError("agent connection closed");
    }
}

// Full-span read, optionally bounded by `timeout` (see run_async_op).
void read_exact_async(asio::io_context& ctx,
                      asio::ip::tcp::socket& socket,
                      std::span<std::byte> dst,
                      const std::atomic<bool>& stop_requested,
                      const std::optional<std::chrono::milliseconds> timeout) {
    bool done = false;
    asio::error_code op_ec;
    asio::async_read(socket,
                     asio::buffer(dst.data(), dst.size()),
                     [&op_ec, &done](const asio::error_code& e, std::size_t) {
                         op_ec = e;
                         done = true;
                     });
    run_async_op(ctx, socket, done, op_ec, stop_requested, timeout);
}

// Unbounded (see run_async_op): an authenticated reply carries no deadline.
void write_all_async(asio::io_context& ctx,
                     asio::ip::tcp::socket& socket,
                     const std::span<const std::byte> data,
                     const std::atomic<bool>& stop_requested) {
    bool done = false;
    asio::error_code op_ec;
    asio::async_write(socket,
                      asio::buffer(data.data(), data.size()),
                      [&op_ec, &done](const asio::error_code& e, std::size_t) {
                          op_ec = e;
                          done = true;
                      });
    run_async_op(ctx, socket, done, op_ec, stop_requested, std::nullopt);
}

// An oversize JSON frame sends a structured error, not a dropped connection.
// A free function keeps serve_client's loop try/catch from nesting (S1141).
void write_reply(asio::io_context& ctx,
                 asio::ip::tcp::socket& socket,
                 const Reply& reply,
                 const std::atomic<bool>& stop_requested) {
    std::vector<std::byte> header;
    try {
        header = encode_frame_header(reply.body, reply.payload.size());
    } catch (const FrameError&) {
        nlohmann::json err;
        err["error"]["code"] = "internal";
        err["error"]["message"] = "response too large for one frame";
        write_all_async(
            ctx, socket, encode_frame_header(err, 0), stop_requested);
        return;
    }
    write_all_async(ctx, socket, header, stop_requested);
    if (!reply.payload.empty()) {
        write_all_async(ctx, socket, reply.payload, stop_requested);
    }
}

} // namespace

AgentServer::AgentServer(ViewModel& model,
                         const std::optional<int> debugger_pid)
    : debugger_pid_(debugger_pid), token_(generate_token()),
      core_(model, token_, oid::system::current_process_id()),
      acceptor_(io_context_), accept_retry_timer_(io_context_) {
    const asio::ip::tcp::endpoint loopback{asio::ip::make_address("127.0.0.1"),
                                           0};
    acceptor_.open(loopback.protocol());
    acceptor_.bind(loopback);
    acceptor_.listen();
    port_ = acceptor_.local_endpoint().port();

    publish_discovery();

    accept_thread_ = std::thread(&AgentServer::accept_loop, this); // NOSONAR
}

AgentServer::~AgentServer() {
    // A destructor must not propagate (S1048); teardown failures are benign.
    try {
        stop();
    } catch (...) { // NOSONAR
        // Nothing actionable during destruction.
    }
}

unsigned short AgentServer::port() const {
    if (stopped_.load()) {
        return 0;
    }
    return port_;
}

const std::string& AgentServer::token() const {
    return token_;
}

void AgentServer::publish_discovery() {
    const std::filesystem::path dir = viewer_discovery_dir();
    // A symlinked/wrong-owner base redirects the token file; no chmod (CWD).
    prepare_private_dir(dir.parent_path(), /*enforce_mode=*/false);
    prepare_private_dir(dir);

    nlohmann::json body;
    body["version"] = 1;
    body["port"] = port();
    body["token"] = token_;
    body["start_time"] =
        std::chrono::duration<double>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();
    body["pid"] = oid::system::current_process_id();
    if (debugger_pid_.has_value()) {
        body["debugger_pid"] = *debugger_pid_;
    }

    discovery_path_ =
        dir / std::format("{}.json", oid::system::current_process_id());
    write_discovery_atomic(discovery_path_, body.dump());
    discovery_written_ = true;
}

void AgentServer::reap_finished_clients_locked() {
    for (auto it = clients_.begin(); it != clients_.end();) {
        if ((*it)->finished->load()) {
            if ((*it)->thread.joinable()) {
                (*it)->thread.join();
            }
            it = clients_.erase(it);
        } else {
            ++it;
        }
    }
}

void AgentServer::accept_loop() {
    // On Linux close() does not wake a blocked accept(); async_accept does.
    schedule_accept();
    io_context_.run();
}

void AgentServer::schedule_accept() {
    if (!acceptor_.is_open()) {
        return; // stop() already closed it; nothing left to accept.
    }

    auto conn_ctx = std::make_shared<asio::io_context>();
    auto socket = std::make_shared<asio::ip::tcp::socket>(*conn_ctx);
    acceptor_.async_accept(
        *socket, [this, conn_ctx, socket](const asio::error_code& ec) {
            if (ec == asio::error::operation_aborted) {
                return; // cancelled by stop(); stop the accept chain.
            }
            if (!ec) {
                handle_accept(conn_ctx, socket);
                schedule_accept(); // keep accepting subsequent connections
                return;
            }
            // Back off: a persistent error must not busy-spin this thread.
            accept_retry_timer_.expires_after(std::chrono::milliseconds(100));
            accept_retry_timer_.async_wait(
                [this](const asio::error_code& wait_ec) {
                    if (wait_ec != asio::error::operation_aborted) {
                        schedule_accept();
                    }
                });
        });
}

void AgentServer::handle_accept(
    const std::shared_ptr<asio::io_context>& conn_ctx,
    const std::shared_ptr<asio::ip::tcp::socket>& socket) {
    std::scoped_lock lock(clients_mutex_);
    reap_finished_clients_locked();

    if (clients_.size() >= MAX_CLIENTS) {
        asio::error_code close_ec;
        socket->close(close_ec);
    } else {
        auto conn = std::make_unique<ClientConnection>();
        conn->ctx = conn_ctx;
        conn->socket = socket;
        conn->finished = std::make_shared<std::atomic<bool>>(false);
        conn->stop_requested = std::make_shared<std::atomic<bool>>(false);
        const std::shared_ptr<std::atomic<bool>> finished = conn->finished;
        const std::shared_ptr<std::atomic<bool>> stop_requested =
            conn->stop_requested;
        auto serve = [this, conn_ctx, socket, finished, stop_requested]() {
            serve_client(*conn_ctx, *socket, *stop_requested);
            finished->store(true);
        };
        conn->thread = std::thread(std::move(serve)); // NOSONAR
        clients_.push_back(std::move(conn));
    }
}

void AgentServer::serve_client(asio::io_context& conn_ctx,
                               asio::ip::tcp::socket& socket,
                               const std::atomic<bool>& stop_requested) {
    bool authed = false;
    // One budget for all pre-auth reads; a resetting window never expires.
    const auto handshake_deadline =
        std::chrono::steady_clock::now() + HANDSHAKE_TIMEOUT;
    try {
        while (true) {
            auto frame = decode_frame(
                [&conn_ctx,
                 &socket,
                 &authed,
                 &stop_requested,
                 handshake_deadline](std::span<std::byte> dst) {
                    std::optional<std::chrono::milliseconds> timeout;
                    if (!authed) {
                        const auto remaining = std::chrono::duration_cast<
                            std::chrono::milliseconds>(
                            handshake_deadline -
                            std::chrono::steady_clock::now());
                        if (remaining <= std::chrono::milliseconds::zero()) {
                            throw AgentServerError("agent handshake timed out");
                        }
                        timeout = remaining;
                    }
                    read_exact_async(
                        conn_ctx, socket, dst, stop_requested, timeout);
                },
                0);

            std::future<Reply> future = enqueue(std::move(frame.obj), &authed);
            const Reply reply = future.get();

            write_reply(conn_ctx, socket, reply, stop_requested);
        }
    } catch (const std::exception&) { // NOSONAR
        // Peer closed, garbage, deadline or stop(); the socket closes on exit.
    }
}

std::future<Reply> AgentServer::enqueue(nlohmann::json request, bool* authed) {
    std::promise<Reply> promise;
    std::future<Reply> future = promise.get_future();
    std::function<void()> on_enqueue;
    {
        const std::scoped_lock lock(queue_mutex_);
        if (stopped_.load()) {
            // No drain() runs again: a queued request would block future.get().
            promise.set_exception(std::make_exception_ptr(
                std::runtime_error("agent server stopped")));
        } else {
            pending_.emplace_back(
                std::move(request), std::move(promise), authed);
            // Copied under lock, invoked outside it: slow listener
            // must not stall other serve threads' enqueues or drain().
            on_enqueue = enqueue_listener_;
        }
    }
    if (on_enqueue) {
        // A listener bug must not unwind this serve thread's request loop.
        try {
            on_enqueue();
        } catch (const std::exception& e) { // NOSONAR - deliberate boundary
            log_listener_failure_once(e.what());
        } catch (...) { // NOSONAR - deliberately absorb listener failures
            log_listener_failure_once("unknown exception");
        }
    }
    return future;
}

void AgentServer::drain() {
    std::vector<PendingRequest> batch;
    {
        std::scoped_lock lock(queue_mutex_);
        batch.swap(pending_);
    }
    for (auto& [request, reply, authed] : batch) {
        // Contain a handler throw to one connection, not the render frame.
        try {
            reply.set_value(core_.handle(request, *authed));
        } catch (...) { // NOSONAR
            reply.set_exception(std::current_exception());
        }
    }
}

void AgentServer::set_enqueue_listener(std::function<void()> listener) {
    const std::scoped_lock lock(queue_mutex_);
    enqueue_listener_ = std::move(listener);
}

void AgentServer::stop() {
    {
        // One step under enqueue()'s lock, so no future.get() blocks forever.
        std::scoped_lock lock(queue_mutex_);
        if (stopped_.load()) {
            return;
        }
        stopped_.store(true);
        pending_.clear();
    }

    // Close the acceptor only after the join: asio I/O objects race.
    io_context_.stop();
    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }
    // Safe now: accept_thread_ is joined, so nothing else touches it.
    asio::error_code close_ec;
    acceptor_.close(close_ec);

    // Cross-thread-safe calls only: run_async_op closes the socket, not us.
    {
        std::scoped_lock lock(clients_mutex_);
        for (const std::unique_ptr<ClientConnection>& client : clients_) {
            client->stop_requested->store(true);
            client->ctx->stop();
        }
    }
    while (true) {
        bool empty = false;
        {
            std::scoped_lock lock(clients_mutex_);
            reap_finished_clients_locked();
            empty = clients_.empty();
        }
        if (empty) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if (discovery_written_) {
        std::error_code ec;
        std::filesystem::remove(discovery_path_, ec);
        discovery_written_ = false;
    }
}

} // namespace oid::host::agent
