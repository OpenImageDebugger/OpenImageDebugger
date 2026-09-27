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

// debugger_pid, when set, is advertised in the discovery file.
struct AgentServerConfig {
    bool enabled = false;            // OID_AGENT=1
    std::optional<int> debugger_pid; // --agent-debugger-pid, else nullopt
};

// No socket thread touches ViewModel: only drain() dispatches, on its thread.
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

    // A default-constructed std::function deregisters the listener.
    void set_enqueue_listener(std::function<void()> listener);

    // Stops accepting connections, joins the accept thread, and removes
    // the discovery file. Idempotent; also invoked by the destructor.
    void stop();

    [[nodiscard]] unsigned short port() const;
    [[nodiscard]] const std::string& token() const;

  private:
    // `authed` points at a serve-thread stack flag; it blocks on `reply`.
    struct PendingRequest {
        nlohmann::json request;
        std::promise<Reply> reply;
        bool* authed;
    };

    // Its own ctx/socket, so one connection's reads cancel independently.
    // `finished` flips just before that thread returns; joins never block.
    struct ClientConnection {
        std::shared_ptr<asio::io_context> ctx;
        std::shared_ptr<asio::ip::tcp::socket> socket;
        std::shared_ptr<std::atomic<bool>> finished;
        // The serve thread closes its own socket; stop() only stops the ctx.
        std::shared_ptr<std::atomic<bool>> stop_requested;
        std::thread thread; // NOSONAR
    };

    void accept_loop();
    // Once the acceptor is closed by stop(), the accept chain ends.
    void schedule_accept();
    // At MAX_CLIENTS capacity the socket is closed instead of served.
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
    // Cached: a cross-thread local_endpoint() on the live acceptor can throw.
    // Written before accept_thread_ starts, so no synchronization is needed.
    unsigned short port_ = 0;
    std::thread accept_thread_; // NOSONAR
    // Guards, atomically with queue_mutex_, whether enqueue() may still queue.
    std::atomic<bool> stopped_{false};

    std::mutex clients_mutex_;
    std::vector<std::unique_ptr<ClientConnection>> clients_;

    std::mutex queue_mutex_;
    // Bounded to MAX_CLIENTS: each serve thread blocks until its request runs.
    std::vector<PendingRequest> pending_;

    // Guarded by queue_mutex_; copied under lock, invoked outside it.
    std::function<void()> enqueue_listener_;

    std::filesystem::path discovery_path_;
    bool discovery_written_ = false;
};

} // namespace oid::host::agent

#endif // HOST_AGENT_AGENT_SERVER_H_
