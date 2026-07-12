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

#include "host/ipc/ipc_client.h"

#include <chrono>
#include <deque>
#include <set>
#include <stdexcept>
#include <utility>

#include "host/ipc/buffer_decode.h"
#include "host/settings/app_settings.h"

namespace oid::host {

IpcClient::IpcClient(oid::ITransport& transport, IpcBufferModel& model)
    : transport_(transport), model_(model) {}

void IpcClient::poll() {
    while (transport_.has_data()) {
        try {
            auto header = oid::MessageType{};
            oid::MessageDecoder{transport_}.read(header);
            dispatch(header);
        } catch (const std::runtime_error&) { // SocketTimeoutError, base catch
            return; // cross-shared-lib RTTI-safe; drop the partial message
        }
    }
}

void IpcClient::dispatch(oid::MessageType header) {
    using enum oid::MessageType;
    switch (header) {
    case SET_AVAILABLE_SYMBOLS:
        handle_set_available_symbols();
        break;
    case GET_OBSERVED_SYMBOLS:
        handle_get_observed_symbols();
        break;
    case PLOT_BUFFER_CONTENTS:
        handle_plot_buffer_contents();
        break;
    case PLOT_BUFFER_BEGIN:
        handle_plot_buffer_begin();
        break;
    case PLOT_BUFFER_CHUNK:
        handle_plot_buffer_chunk();
        break;
    case PLOT_BUFFER_END:
        handle_plot_buffer_end();
        break;
    case APPLY_SESSION_STATE:
        handle_apply_session_state();
        break;
    case EXPORT_SELECTED_BUFFER:
        handle_export_selected_buffer();
        break;
    default:
        // Remaining message types (e.g. BUFFER_REMOVED) are only sent, never
        // received, by this side; ignore.
        break;
    }
}

void IpcClient::handle_set_available_symbols() {
    std::deque<std::string> symbols;
    oid::MessageDecoder{transport_}.read<std::deque<std::string>, std::string>(
        symbols);
    available_symbols_.assign(symbols.begin(), symbols.end());

    const auto now = static_cast<std::int64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
    std::set<std::string, std::less<>> avail{available_symbols_.begin(),
                                             available_symbols_.end()};
    for (const auto& b : restore_buffers_) {
        if (b.expiry_epoch_s <= now) {
            continue;
        }
        if (!avail.contains(b.variable_name)) {
            continue;
        }
        if (restore_requested_.contains(b.variable_name)) {
            continue;
        }
        if (model_has(b.variable_name)) {
            continue;
        }
        request_plot(b.variable_name);
        restore_requested_.insert(b.variable_name);
    }
}

void IpcClient::handle_get_observed_symbols() {
    oid::MessageComposer composer;
    composer.push(oid::MessageType::GET_OBSERVED_SYMBOLS_RESPONSE)
        .push(model_.size());
    for (std::size_t i = 0; i < model_.size(); ++i) {
        composer.push(model_.variable_name_of(i));
    }
    send_guarded(composer);
}

void IpcClient::handle_plot_buffer_contents() {
    std::string variable_name;
    std::string display_name;
    std::string pixel_layout;
    bool transpose{};
    int width{};
    int height{};
    int channels{};
    int stride{};
    auto type = oid::BufferType{};
    std::vector<std::byte> bytes;
    oid::MessageDecoder{transport_}
        .read(variable_name)
        .read(display_name)
        .read(pixel_layout)
        .read(transpose)
        .read(width)
        .read(height)
        .read(channels)
        .read(stride)
        .read(type)
        .read(bytes);
    model_.upsert(make_buffer_record({.variable_name = std::move(variable_name),
                                      .display_name = std::move(display_name),
                                      .pixel_layout = std::move(pixel_layout),
                                      .transpose = transpose,
                                      .width = width,
                                      .height = height,
                                      .channels = channels,
                                      .stride = stride,
                                      .type = type,
                                      .bytes = std::move(bytes)}));
}

void IpcClient::handle_plot_buffer_begin() {
    oid::BufferAssembler::BeginParams params;
    oid::MessageDecoder decoder{transport_};
    decoder.read(params.variable_name)
        .read(params.display_name)
        .read(params.pixel_layout)
        .read(params.transpose)
        .read(params.width)
        .read(params.height)
        .read(params.channels)
        .read(params.stride);
    int type_int{};
    decoder.read(type_int);
    params.type = type_int;
    decoder.read(params.total_byte_size);
    assembler_.begin(std::move(params));
}

void IpcClient::handle_plot_buffer_chunk() {
    std::string name;
    std::size_t row_offset{};
    std::size_t row_count{};
    std::vector<std::byte> bytes;
    oid::MessageDecoder{transport_}
        .read(name)
        .read(row_offset)
        .read(row_count)
        .read(bytes);
    (void)assembler_.chunk(name, row_offset, row_count, bytes);
}

void IpcClient::handle_plot_buffer_end() {
    std::string name;
    oid::MessageDecoder{transport_}.read(name);
    auto assembled = assembler_.end(name);
    if (assembled) {
        model_.upsert(make_buffer_record(
            {.variable_name = std::move(assembled->variable_name),
             .display_name = std::move(assembled->display_name),
             .pixel_layout = std::move(assembled->pixel_layout),
             .transpose = assembled->transpose,
             .width = assembled->width,
             .height = assembled->height,
             .channels = assembled->channels,
             .stride = assembled->stride,
             .type = static_cast<oid::BufferType>(assembled->type),
             .bytes = std::move(assembled->bytes)}));
    }
}

void IpcClient::handle_apply_session_state() {
    std::string json;
    oid::MessageDecoder{transport_}.read(json);
    if (session_state_callback_) {
        session_state_callback_(json);
    }
}

void IpcClient::handle_export_selected_buffer() const {
    // No payload on the wire (mirrors the Qt side, which just emits
    // exportSelectedBufferRequested() with no arguments) -- nothing to
    // decode beyond the header already consumed by poll().
    if (export_selected_callback_) {
        export_selected_callback_();
    }
}

void IpcClient::request_plot(const std::string& variable_name) {
    oid::MessageComposer composer;
    composer.push(oid::MessageType::PLOT_BUFFER_REQUEST).push(variable_name);
    send_guarded(composer);
}

void IpcClient::notify_removed(const std::string& variable_name) {
    oid::MessageComposer composer;
    composer.push(oid::MessageType::BUFFER_REMOVED).push(variable_name);
    send_guarded(composer);
}

void IpcClient::send_session_state_changed(const std::string& json) {
    oid::MessageComposer composer;
    composer.push(oid::MessageType::SESSION_STATE_CHANGED).push(json);
    send_guarded(composer);
}

void IpcClient::send_export_buffer_request(const std::string& variable_name,
                                           int format,
                                           const std::vector<float>& contrast) {
    oid::MessageComposer composer;
    composer.push(oid::MessageType::EXPORT_BUFFER_REQUEST)
        .push(variable_name)
        .push(format);
    // Fixed 8-float contrast layout on the wire (mirrors the Qt sender,
    // MessageHandler::request_export_buffer): pad missing entries with
    // 0.0f, ignore any beyond the 8th.
    for (int i = 0; i < 8; ++i) {
        const float value =
            static_cast<std::size_t>(i) < contrast.size() ? contrast[i] : 0.0F;
        composer.push(value);
    }
    send_guarded(composer);
}

void IpcClient::set_session_state_callback(
    std::function<void(const std::string& json)> cb) {
    session_state_callback_ = std::move(cb);
}

void IpcClient::set_export_selected_callback(std::function<void()> cb) {
    export_selected_callback_ = std::move(cb);
}

const std::vector<std::string>& IpcClient::available_symbols() const {
    return available_symbols_;
}

void IpcClient::set_restore_buffers(
    std::vector<oid::host::PreviousBuffer> buffers) {
    restore_buffers_ = std::move(buffers);
}

bool IpcClient::model_has(std::string_view variable_name) const {
    for (std::size_t i = 0; i < model_.size(); ++i) {
        if (model_.variable_name_of(i) == variable_name) {
            return true;
        }
    }
    return false;
}

void IpcClient::send_guarded(const oid::MessageComposer& composer) {
    try {
        composer.send(transport_);
    } catch (const std::runtime_error&) {
        // Transport is closed or peer is gone (e.g. viewer opened with no
        // debugger attached). Inbound poll() already tolerates this;
        // outbound sends must too, so a stray IPC message never crashes the
        // viewer.
    }
}

} // namespace oid::host
