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

#ifdef _WIN32
#include <process.h> // _getpid
#else
#include <unistd.h> // getpid
#endif

#include "host/agent/discovery_file.h"
#include "host/agent/wire_frame.h"

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

// Mirrors agentendpoint.py's HANDSHAKE_TIMEOUT: an unauthenticated
// connection must complete hello within this window; the deadline is
// lifted once the connection has authenticated.
constexpr std::chrono::seconds HANDSHAKE_TIMEOUT{10};

long current_pid() {
#ifdef _WIN32
    return static_cast<long>(_getpid());
#else
    return static_cast<long>(getpid());
#endif
}

// Fills a 64-hex-character session token from 32 bytes of
// std::random_device entropy. std::random_device is the only portable
// entropy source available without a third-party dependency; a stronger,
// platform-specific CSPRNG (e.g. getrandom()/BCryptGenRandom) would be
// preferable and is a known follow-up.
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

// Dedicated exception type (S112) for agent-transport failures raised
// below; callers already catch it via catch (const std::exception&), so
// this changes no behavior.
class AgentServerError : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

// Drains `ctx` until the pending op's completion handler has run (`done`).
// A single run() can return early: AgentServer::stop() flips stop_requested and
// calls ctx.stop() as two separate steps, so the ctx.stop() can land after
// run_async_op's restart() or partway through this drain, leaving the context
// stopped and a bare run() a no-op. Looping restart()+run() absorbs that -- the
// caller has already closed the socket, so the pending op completes promptly
// (operation_aborted) and some run() invocation runs its handler while the
// stack-captured done/op_ec are still alive.
void drain_until_done(asio::io_context& ctx, const bool& done) {
    // ctx.run() blocks until the (already socket.close()'d) op completes, so
    // one pass normally suffices; the loop only re-runs when stop() calls
    // ctx.stop() again mid-drain and makes run() return early. run() == 0 means
    // no handler ran and none is pending -- nothing left to drain -- so stop
    // rather than spin, keeping this bounded even in a pathological state.
    while (!done) {
        ctx.restart();
        if (ctx.run() == 0) {
            break;
        }
    }
}

// Drives one already-started async operation on `ctx` to completion: bounded
// by `timeout` pre-auth (so an idle connection cannot hold a serve thread/slot
// open indefinitely without completing hello), unbounded post-auth. It runs
// only on the connection's own serve thread, so the socket and ctx are never
// touched concurrently by another thread. AgentServer::stop() requests teardown
// with two documented-thread-safe calls -- stop_requested->store(true) and
// ctx.stop() -- and the actual socket close happens here, on the serve thread.
// The post-restart flag check closes the race where stop() runs between two
// ops: ctx.restart() would otherwise erase the pending ctx.stop() and the next
// ctx.run() would block forever.
void run_async_op(asio::io_context& ctx,
                  asio::ip::tcp::socket& socket,
                  const bool& done,
                  const asio::error_code& op_ec,
                  const std::atomic<bool>& stop_requested,
                  const std::optional<std::chrono::milliseconds> timeout) {
    ctx.restart();
    if (stop_requested.load()) {
        // stop() flipped the flag (and called ctx.stop()) before this restart
        // cleared the stopped state; close the socket to force prompt
        // completion of the pending op and drain its aborted handler so the
        // stack-captured done/op_ec run while still alive, then unwind.
        asio::error_code ignore;
        socket.close(ignore);
        drain_until_done(ctx, done);
        throw AgentServerError("agent connection stopped");
    }
    if (timeout) {
        ctx.run_for(*timeout);
        if (!done) {
            // Timed out, or stop() called ctx.stop() during run_for (which
            // leaves the context stopped, so a bare run() would no-op and
            // strand the pending handler). close() forces the op to complete;
            // drain_until_done then runs the handler while its captures live.
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

// Full-span write, unbounded (see run_async_op): used for every outgoing
// reply, once a connection has authenticated there is no deadline on it
// either.
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

// Frames and sends `reply` on `socket`: the JSON header, then the binary
// payload straight from the reply (no combined-frame copy). If the reply's JSON
// serializes past the frame cap (e.g. list_buffers on a session with a huge
// number of buffers), sends a small structured error instead so the client
// sees a clean failure rather than a dropped connection. A free function
// (rather than an inline try in serve_client) keeps that loop's try/catch from
// nesting (S1141).
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

AgentServer::AgentServer(ViewModel& model, const AgentServerConfig cfg)
    : cfg_(cfg), token_(generate_token()), core_(model, token_, current_pid()),
      acceptor_(io_context_), accept_retry_timer_(io_context_) {
    if (!cfg_.enabled) {
        return;
    }

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
    // A destructor must not propagate exceptions (S1048): stop() touches the
    // filesystem (discovery-file unlink) and joins threads, any of which could
    // throw. Teardown failures are non-fatal at shutdown, so swallow them.
    try {
        stop();
    } catch (...) { // NOSONAR
        // Nothing actionable during destruction.
    }
}

unsigned short AgentServer::port() const {
    if (!cfg_.enabled || stopped_.load()) {
        return 0;
    }
    return port_;
}

const std::string& AgentServer::token() const {
    return token_;
}

void AgentServer::publish_discovery() {
    const std::filesystem::path dir = viewer_discovery_dir();
    // Harden the predictable base dir (dir's parent, tempdir/oid-agent-<user>)
    // before the viewer subdir. Previously only the leaf was checked, so a
    // base pre-planted as a symlink or owned by another user could redirect
    // where this token-bearing discovery file lands. Rejecting a symlinked /
    // wrong-owner base here (plus /tmp's sticky bit, which stops the base from
    // being swapped once we create it) closes that redirection.
    // enforce_mode=false: the base only needs the symlink/owner check (its
    // private viewer/ leaf protects the tokens), so hardening it never chmods a
    // surprising directory such as the CWD when OID_AGENT_DIR is ".".
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
    body["pid"] = current_pid();
    if (cfg_.debugger_pid.has_value()) {
        body["debugger_pid"] = *cfg_.debugger_pid;
    }

    discovery_path_ = dir / std::format("{}.json", current_pid());
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
    // A synchronous acceptor_.accept() unblocked by a cross-thread
    // acceptor_.close() is not portable: on Linux, close() does not wake a
    // thread blocked in the kernel's accept(), so stop() could hang
    // joining this thread. async_accept driven by io_context_.run() is
    // reliably cancellable from another thread (io_context_.stop(), plus
    // acceptor_.close() to cancel the operation itself -- see stop()),
    // mirroring how each per-connection ctx is cancelled in run_async_op.
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
            // A transient accept error (e.g. ECONNABORTED when a peer aborts
            // before accept, or EMFILE) must not permanently stop the endpoint
            // from accepting. Re-arm after a short backoff so a persistent
            // error cannot busy-spin the accept thread; schedule_accept()
            // no-ops once stop() has closed the acceptor.
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
    // Absolute deadline for the whole pre-auth phase, captured at connection
    // start. HANDSHAKE_TIMEOUT is a single budget shared across every read of
    // every pre-auth frame -- not a per-read window that resets, which would
    // let a client hold a MAX_CLIENTS slot forever by trickling frames just
    // under the timeout without ever authenticating.
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
        // Peer closed, sent garbage, missed the handshake deadline, or the
        // server was stopped while a request/reply was pending (which
        // surfaces here as a broken_promise future error, or as the
        // "agent connection stopped" run_async_op raises once
        // AgentServer::stop() has stopped conn_ctx) -- nothing to clean up
        // beyond the socket, which closes on scope exit.
    }
}

std::future<Reply> AgentServer::enqueue(nlohmann::json request, bool* authed) {
    std::promise<Reply> promise;
    std::future<Reply> future = promise.get_future();
    std::function<void()> on_enqueue;
    {
        const std::scoped_lock lock(queue_mutex_);
        if (stopped_.load()) {
            // Shutting down: a serve thread can reach here concurrently
            // with stop() (e.g. it finished decode_frame just as stop()
            // was clearing pending_); fail immediately instead of queuing
            // a request drain() will never run again to service, which
            // would otherwise block this thread's future.get() forever.
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
        // Exception boundary: this runs on a serve thread, where an escaping
        // exception would unwind the request loop and drop the client (or
        // terminate). The listener contract says cheap and nonthrowing (it is
        // FramePacer::wake), but a listener bug must not destabilize the
        // transport, so failures are absorbed: the request is already queued
        // and the per-frame drain still serves it.
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
        // Contain any handler exception (e.g. bad_alloc from a large get_buffer
        // copy) to the one request: the serve thread's future.get() rethrows it
        // and its existing catch closes just that connection, rather than
        // letting it unwind through the render frame and crash the viewer.
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
        // Flip stopped_ and break every already-queued request as a single
        // step guarded by queue_mutex_ -- the same lock enqueue() takes to
        // decide whether to queue a request. That serialization is what
        // closes the shutdown race: a serve thread that calls enqueue()
        // concurrently with this either queues before this clear() (and is
        // then broken by it) or observes stopped_ == true afterward (and
        // fails immediately in enqueue() instead); either way its
        // future.get() is guaranteed to return rather than block forever
        // with no drain() left to run.
        std::scoped_lock lock(queue_mutex_);
        if (stopped_.load()) {
            return;
        }
        stopped_.store(true);
        pending_.clear();
    }

    if (cfg_.enabled) {
        // Stopping the io_context is what ends the accept loop: async_accept
        // never blocks, so unwinding accept_thread_'s io_context_.run() is
        // enough for it to return. The acceptor itself is closed only AFTER
        // accept_thread_ is joined (below): closing it here, while that
        // thread may still be inside the async_accept handler (schedule_accept
        // re-arming the acceptor), is concurrent access to a non-thread-safe
        // asio I/O object (CWE-362) and can corrupt state / crash under load.
        io_context_.stop();
    }
    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }
    if (cfg_.enabled) {
        // Safe now: accept_thread_ has been joined, so nothing else touches
        // the acceptor. (Any still-pending async_accept is abandoned; its
        // captured shared_ptrs release when io_context_ is destroyed.)
        asio::error_code ec;
        acceptor_.close(ec);
    }

    // Unblock every still-serving connection using only documented-thread-safe
    // cross-thread calls: set stop_requested, then stop the connection's ctx.
    // ctx->stop() makes a currently-running ctx.run()/run_for() return
    // immediately; run_async_op then observes stop_requested and closes its own
    // socket on the serve thread (never here), which is what cancels the
    // pending read/write. The flag also covers the window where a serve thread
    // is between ops when stop() runs and would otherwise restart the ctx and
    // block on the next read. Without this, a connection still being served
    // when stop() returns would leave a joinable std::thread in `clients_`, and
    // destroying that (here, or in ~AgentServer) would call std::terminate.
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
