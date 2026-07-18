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

#ifndef HOST_AGENT_AGENT_SERVER_H_
#define HOST_AGENT_AGENT_SERVER_H_

#include <atomic>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <asio.hpp>
#include <nlohmann/json.hpp>

#include "host/agent/agent_core.h"
#include "host/agent/view_model.h"

namespace oid::host::agent {

// Whether the native agent transport starts at all, and (when the viewer
// was launched attached to a debugger session) which debugger PID to
// advertise in the discovery file.
struct AgentServerConfig {
    bool enabled = false;            // OID_AGENT=1
    std::optional<int> debugger_pid; // --agent-debugger-pid, else nullopt
};

// Localhost TCP transport for the viewer-side agent endpoint. Binds an
// ephemeral 127.0.0.1 port, publishes a discovery file advertising it, and
// frames requests/replies with wire_frame's codec. A dedicated thread runs
// an io_context driving an async accept loop (portable, reliable
// cross-thread cancellation -- unlike a synchronous accept() unblocked by
// a cross-thread close(), which is not guaranteed to wake a thread parked
// in the kernel); each accepted connection is served on its own thread.
// Neither thread ever calls into ViewModel/AgentCore directly --
// every decoded request is queued, and only drain() (which the caller must
// invoke from the thread that owns the GL/ViewModel state, normally the
// main render loop once per frame) actually dispatches it.
class AgentServer {
  public:
    AgentServer(ViewModel& model, AgentServerConfig cfg);
    ~AgentServer();

    AgentServer(const AgentServer&) = delete;
    AgentServer& operator=(const AgentServer&) = delete;
    AgentServer(AgentServer&&) = delete;
    AgentServer& operator=(AgentServer&&) = delete;

    // Dispatches every request queued since the last call, on the calling
    // thread. Intended to be invoked once per frame from the main loop.
    void drain();

    // Registers a listener to be invoked (without arguments) when a request is
    // queued. Thread-safe; the listener is copied under lock and invoked
    // outside the lock to avoid stalling other serve threads. Passing a
    // default-constructed std::function (which is nullptr-like) deregisters
    // the listener (e.g., to restore a no-op). Intended for the FramePacer to
    // use: wake itself and force the main loop to drain() sooner on each
    // request, instead of waiting for the next scheduled frame.
    void set_enqueue_listener(std::function<void()> listener);

    // Stops accepting connections, joins the accept thread, and removes
    // the discovery file. Idempotent; also invoked by the destructor.
    void stop();

    [[nodiscard]] unsigned short port() const;
    [[nodiscard]] const std::string& token() const;

  private:
    // One request awaiting dispatch. `authed` points at the per-connection
    // "hello done" flag, which lives on the owning serve thread's stack
    // (that thread is blocked on `reply`'s future for the lifetime of this
    // entry, so the pointer stays valid).
    struct PendingRequest {
        nlohmann::json request;
        std::promise<Reply> reply;
        bool* authed;
    };

    // One accepted connection: its own io_context/socket (so its bounded
    // pre-auth reads can be cancelled independently of every other
    // connection) plus the thread serving it. `finished` is flipped by
    // that thread just before it returns, so accept_loop/stop() know it is
    // safe to join without blocking on a connection that is still serving
    // a client. The socket is closed from outside (accept_loop reaping,
    // or stop()) to unblock a thread stuck in a read/write.
    struct ClientConnection {
        std::shared_ptr<asio::io_context> ctx;
        std::shared_ptr<asio::ip::tcp::socket> socket;
        std::shared_ptr<std::atomic<bool>> finished;
        // Set by stop() (any thread) to request teardown. The serve thread
        // observes it in run_async_op and closes its own socket there, so the
        // socket/ctx are only ever touched by the thread that runs them --
        // stop() itself only calls ctx->stop() (documented thread-safe).
        std::shared_ptr<std::atomic<bool>> stop_requested;
        std::thread thread; // NOSONAR
    };

    void accept_loop();
    // Posts one async_accept and, on success, spawns the connection and
    // schedules the next one; on cancellation/error (acceptor closed by
    // stop()) the chain simply ends.
    void schedule_accept();
    // Handles one successfully accepted connection: reaps finished
    // connections, then either closes the socket (at MAX_CLIENTS capacity)
    // or spawns a thread to serve it. Called from schedule_accept's
    // async_accept completion handler.
    void handle_accept(const std::shared_ptr<asio::io_context>& conn_ctx,
                       const std::shared_ptr<asio::ip::tcp::socket>& socket);
    void serve_client(asio::io_context& conn_ctx,
                      asio::ip::tcp::socket& socket,
                      const std::atomic<bool>& stop_requested);
    std::future<Reply> enqueue(nlohmann::json request, bool* authed);
    void publish_discovery();
    // Joins and drops every ClientConnection whose thread has finished.
    // Caller must hold clients_mutex_.
    void reap_finished_clients_locked();

    AgentServerConfig cfg_;
    std::string token_;
    AgentCore core_;

    asio::io_context io_context_;
    asio::ip::tcp::acceptor acceptor_;
    // Backoff before re-arming the accept loop after a transient accept error,
    // so a persistent one (e.g. EMFILE) cannot busy-spin the accept thread.
    asio::steady_timer accept_retry_timer_;
    // Bound port cached once after listen() -- written before accept_thread_
    // starts (happens-before), so no synchronization is needed. port() returns
    // this instead of querying the live acceptor, which the accept thread
    // concurrently re-arms and stop() closes (both make a cross-thread
    // local_endpoint() call unsafe / able to throw).
    unsigned short port_ = 0;
    std::thread accept_thread_; // NOSONAR
    // Guards, atomically with queue_mutex_, whether enqueue() may still
    // queue a request; see stop() and enqueue(). Also readable on its own
    // (e.g. from port()) without taking queue_mutex_.
    std::atomic<bool> stopped_{false};

    std::mutex clients_mutex_;
    std::vector<std::unique_ptr<ClientConnection>> clients_;

    std::mutex queue_mutex_;
    // Bounded to MAX_CLIENTS entries by construction, not unbounded: each
    // connection thread blocks on future.get() in serve_client() until drain()
    // services its request, so it has at most one request in flight, and
    // handle_accept() caps concurrent connections at MAX_CLIENTS. drain()
    // therefore processes at most MAX_CLIENTS requests per frame.
    std::vector<PendingRequest> pending_;

    // Listener to invoke when a request is queued. Guarded by queue_mutex_.
    // Thread-safe: copied under lock, invoked outside it to avoid stalling
    // other serve threads' enqueues or drain().
    std::function<void()> enqueue_listener_;

    std::filesystem::path discovery_path_;
    bool discovery_written_ = false;
};

} // namespace oid::host::agent

#endif // HOST_AGENT_AGENT_SERVER_H_
