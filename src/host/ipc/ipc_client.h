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

#ifndef HOST_IPC_IPC_CLIENT_H_
#define HOST_IPC_IPC_CLIENT_H_

#include <functional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "host/settings/app_settings.h"
#include "host/ui/ipc_buffer_model.h"
#include "ipc/buffer_assembler.h"
#include "ipc/message_exchange.h"
#include "ipc/transport.h"

namespace oid::host {

// Qt-free port of the window-side of the Qt MessageHandler: decodes inbound
// messages (SET_AVAILABLE_SYMBOLS, GET_OBSERVED_SYMBOLS, PLOT_BUFFER_CONTENTS,
// PLOT_BUFFER_BEGIN/Chunk/End) into the IpcBufferModel + symbol list, and
// sends outbound requests (PLOT_BUFFER_REQUEST, BUFFER_REMOVED). The transport
// is injected as oid::ITransport& so this is unit-testable against a fake
// transport with no live socket.
class IpcClient {
  public:
    IpcClient(oid::ITransport& transport, IpcBufferModel& model);

    // Drains all currently-available inbound messages into the model /
    // symbol list. Called once per frame. Never blocks the caller beyond
    // the transport's own bounded reads; catches std::runtime_error
    // (SocketTimeoutError) if a message header/body isn't fully available
    // yet.
    void poll();

    // Outbound (from the chrome):
    void request_plot(const std::string& variable_name); // PLOT_BUFFER_REQUEST
    void notify_removed(const std::string& variable_name); // BUFFER_REMOVED

    // Sends SESSION_STATE_CHANGED (type 7): a single JSON string, verbatim
    // (no parsing/validation here -- the caller owns the JSON shape).
    void send_session_state_changed(const std::string& json);

    // Sends EXPORT_BUFFER_REQUEST (type 5): variable name, export format, and
    // an 8-entry contrast vector. Mirrors the Qt sender
    // (MessageHandler::request_export_buffer): entries beyond `contrast`'s
    // size are padded with 0.0f, and only the first 8 entries of `contrast`
    // are sent (extras ignored), matching the wire's fixed 8-float layout.
    void send_export_buffer_request(const std::string& variable_name,
                                    int format,
                                    const std::vector<float>& contrast);

    // Registers the callback invoked when an inbound APPLY_SESSION_STATE
    // (type 6) message is decoded; called with the JSON string verbatim.
    // Not required to be set: if unset, the message is still fully
    // consumed from the transport (so the stream isn't left desynced) and
    // silently dropped.
    void
    set_session_state_callback(std::function<void(const std::string& json)> cb);

    // Registers the callback invoked when an inbound EXPORT_SELECTED_BUFFER
    // (type 8) message is decoded. EXPORT_SELECTED_BUFFER carries no payload
    // (mirrors the Qt side, which just emits exportSelectedBufferRequested()
    // with no arguments). Not required to be set.
    void set_export_selected_callback(std::function<void()> cb);

    // Latest available-symbols list (from SET_AVAILABLE_SYMBOLS); UiState
    // reads it.
    [[nodiscard]] const std::vector<std::string>& available_symbols() const;

    // Seed the previous-session buffers to auto-restore. On the next
    // SET_AVAILABLE_SYMBOLS, each entry that is available, not already loaded,
    // not expired (now < expiry) is re-requested via request_plot.
    // Idempotent.
    void set_restore_buffers(std::vector<oid::host::PreviousBuffer> buffers);

  private:
    void dispatch(oid::MessageType header);

    // Per-message-type handlers: each decodes (or applies) exactly one
    // oid::MessageType and fully consumes its wire payload.
    void handle_set_available_symbols();
    void handle_get_observed_symbols();
    void handle_plot_buffer_contents();
    void handle_plot_buffer_begin();
    void handle_plot_buffer_chunk();
    void handle_plot_buffer_end();
    void handle_apply_session_state();
    void handle_export_selected_buffer() const;

    [[nodiscard]] bool model_has(std::string_view variable_name) const;

    // Sends `composer` over transport_, catching and swallowing
    // std::runtime_error. Mirrors poll()'s tolerance of transport errors on
    // the inbound side: if the peer is gone (e.g. viewer opened with no
    // debugger attached), an outbound send must not crash the viewer.
    void send_guarded(const oid::MessageComposer& composer);

    oid::ITransport& transport_;
    IpcBufferModel& model_;
    oid::BufferAssembler assembler_;
    std::vector<std::string> available_symbols_;
    std::vector<oid::host::PreviousBuffer> restore_buffers_;
    std::set<std::string, std::less<>> restore_requested_;
    std::function<void(const std::string& json)> session_state_callback_;
    std::function<void()> export_selected_callback_;
};

} // namespace oid::host

#endif // HOST_IPC_IPC_CLIENT_H_
