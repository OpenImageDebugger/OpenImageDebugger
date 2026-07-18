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

// -----------------------------------------------------------------------
// AgentCore glue contract
//
// AgentCore is the transport-neutral JSON-in/JSON-out request core for the
// viewer agent endpoint. Its only public entry point is `handle()`: give it
// one already-decoded JSON request plus the per-connection auth state, and
// it returns a Reply. AgentCore never touches sockets, threads, or GL -- it
// is plain C++ over ViewModel, so it links into any transport that can hand
// it a parsed JSON object and read back a JSON body (+ optional binary
// trailer). That is what makes it reachable from an out-of-tree platform
// port's wasm glue as well as from the native asio transport in this repo:
// the wasm export declaration and its JS-side marshalling live entirely in
// that non-native platform port, not here.
//
// Method vocabulary (only hello is order-constrained -- it must come
// first; the rest may be called in any order):
//   "hello"          -- must be the first call on a connection; carries the
//                        auth token. Every other method before a successful
//                        hello fails with ERR_BAD_TOKEN. Its reply's "pid"
//                        is whatever pid the transport supplied when
//                        constructing this AgentCore -- a wasm glue layer
//                        passes its own; 0 if none was given.
//   "ping"            -- liveness check.
//   "list_buffers"    -- enumerate currently held buffers.
//   "get_buffer"      -- fetch one buffer's metadata + pixel bytes.
//   "get_view"        -- read back the current view state.
//   "set_view"        -- apply a view state change.
//   Once authenticated, any method name outside this list fails with
//   ERR_UNKNOWN_METHOD.
//
// Envelope shapes (see Reply below):
//   - success: Reply::body is a JSON object holding the method's result
//     fields directly (no extra wrapping).
//   - error: Reply::body is {"error": {"code": <string>, "message":
//     <string>}}, where <code> is one of the ERR_* constants on AgentCore.
//
// Error codes: "bad_token", "unknown_method", "unknown_buffer",
// "bad_params", "too_large", "internal".
//
// Payload convention: most replies carry no binary payload. `get_buffer` is
// the exception -- its pixel bytes ride in Reply::payload, a raw binary
// trailer that AgentCore itself never frames. The transport is responsible
// for injecting a "payload": <nbytes> field into the JSON body and
// appending the raw bytes after it; a wasm glue layer surfaces those bytes
// to JS as a heap view rather than copying them through the JSON body.
// -----------------------------------------------------------------------

#ifndef HOST_AGENT_AGENT_CORE_H_
#define HOST_AGENT_AGENT_CORE_H_

#include <cstddef>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "host/agent/view_model.h"

namespace oid::host::agent {

// One dispatched request's outcome. `body` is the JSON the transport frames
// as-is; `payload` is the (possibly empty) raw binary trailer -- only
// get_buffer produces one. AgentCore never touches the wire format itself:
// the transport is responsible for injecting the "payload": <nbytes> field
// and appending payload bytes when it frames this Reply.
struct Reply {
    nlohmann::json body;
    std::vector<std::byte> payload;
};

// Transport-neutral request dispatcher: given one already-decoded JSON
// request object, calls into ViewModel and returns a JSON reply. Never
// throws for protocol-level failures -- every error path returns
// {"error": {"code", "message"}} in Reply::body instead.
class AgentCore {
  public:
    AgentCore(ViewModel& model,
              std::string token,
              long pid = 0,
              std::string session_kind = "viewer");

    // Dispatch one already-decoded request. `authed` is the per-connection
    // hello state: it starts false, this call flips it true on a successful
    // hello, and every other method is rejected with ERR_BAD_TOKEN until
    // then.
    Reply handle(const nlohmann::json& request, bool& authed);

    // Error codes reused from the debugger endpoint plus viewer-specific.
    static constexpr auto ERR_BAD_TOKEN = "bad_token";
    static constexpr auto ERR_UNKNOWN_METHOD = "unknown_method";
    static constexpr auto ERR_UNKNOWN_BUFFER = "unknown_buffer";
    static constexpr auto ERR_BAD_PARAMS = "bad_params";
    static constexpr auto ERR_TOO_LARGE = "too_large";
    static constexpr auto ERR_INTERNAL = "internal";

  private:
    using Handler = Reply (AgentCore::*)(const nlohmann::json&) const;

    ViewModel& model_;
    std::string token_;
    long pid_;
    std::string session_kind_;

    Reply handle_hello(const nlohmann::json& request, bool& authed);
    Reply handle_ping(const nlohmann::json& request) const;
    Reply handle_list_buffers(const nlohmann::json& request) const;
    Reply handle_get_buffer(const nlohmann::json& request) const;
    Reply handle_get_view(const nlohmann::json& request) const;
    Reply handle_set_view(const nlohmann::json& request) const;
};

} // namespace oid::host::agent

#endif // HOST_AGENT_AGENT_CORE_H_
