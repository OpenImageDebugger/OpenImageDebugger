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
#include <iostream>
#include <new>
#include <set>
#include <stdexcept>
#include <utility>

#include "host/ipc/buffer_decode.h"
#include "host/settings/app_settings.h"
#include "host/util/log_preview.h"
#include "ipc/raw_data_decode.h"

namespace oid::host {

IpcClient::IpcClient(ITransport& transport, IpcBufferModel& model)
    : transport_(transport), model_(model) {}

void IpcClient::poll() {
    while (transport_.has_data()) {
        try {
            auto header = MessageType{};
            MessageDecoder{transport_}.read(header);
            dispatch(header);
        } catch (const std::runtime_error&) { // SocketTimeoutError, base catch
            return; // cross-shared-lib RTTI-safe; drop the partial message
        } catch (const std::length_error&) {
            // Peer-supplied sizes drive allocations under dispatch() that
            // MessageDecoder does not guard (BufferAssembler::begin() on a
            // 32-bit size_t). Not the logic_error base: its other children
            // signal bugs here, and swallowing them would hide them.
            std::cerr << "[OID] container limit exceeded decoding a message; "
                         "dropped\n";
            return;
        } catch (const std::bad_alloc&) {
            // Peer-supplied sizes are capped before any allocation, but the
            // cap is generous enough to still fail on a loaded machine.
            std::cerr << "[OID] out of memory decoding a message; dropped\n";
            return;
        }
    }
}

void IpcClient::dispatch(const MessageType header) {
    using enum MessageType;
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
    MessageDecoder{transport_}.read<std::deque<std::string>, std::string>(
        symbols);
    available_symbols_.assign(symbols.begin(), symbols.end());

    const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
    const std::set<std::string, std::less<>> avail{available_symbols_.begin(),
                                                   available_symbols_.end()};
    for (const auto& [variable_name, expiry_epoch_s] : restore_buffers_) {
        if (expiry_epoch_s <= now) {
            continue;
        }
        if (!avail.contains(variable_name)) {
            continue;
        }
        if (restore_requested_.contains(variable_name)) {
            continue;
        }
        if (model_has(variable_name)) {
            continue;
        }
        request_plot(variable_name);
        restore_requested_.insert(variable_name);
    }
}

void IpcClient::handle_get_observed_symbols() const {
    // LOCAL_FILE buffers are not owned by the debugger: advertising one back
    // would ask for a meaningless replot and persist it into session state.
    std::vector<std::string> observed;
    for (std::size_t i = 0; i < model_.size(); ++i) {
        if (model_.at(i).kind == BufferKind::DEBUGGER_SYMBOL) {
            observed.push_back(model_.variable_name_of(i));
        }
    }

    MessageComposer composer;
    composer.push(MessageType::GET_OBSERVED_SYMBOLS_RESPONSE)
        .push(observed.size());
    for (const std::string& name : observed) {
        composer.push(name);
    }
    send_guarded(composer);
}

void IpcClient::handle_plot_buffer_contents() const {
    std::string variable_name;
    std::string display_name;
    std::string pixel_layout;
    bool transpose{};
    int width{};
    int height{};
    int channels{};
    int stride{};
    auto type = BufferType{};
    std::vector<std::byte> bytes;
    MessageDecoder{transport_}
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
    if (!is_known_buffer_type(static_cast<int>(type))) {
        std::cerr << "[OID] rejected PLOT_BUFFER_CONTENTS for '"
                  << variable_name << "': unknown buffer type "
                  << static_cast<int>(type) << "\n";
        return;
    }
    if (!within_display_limits(width, height, channels)) {
        std::cerr << "[OID] rejected PLOT_BUFFER_CONTENTS for '"
                  << variable_name
                  << "': geometry exceeds the display limits (max dimension "
                  << MAX_BUFFER_DIMENSION << ", max " << MAX_CHANNEL_COUNT
                  << " channels)\n";
        return;
    }
    if (!geometry_fits_payload(
            width, height, channels, stride, type, bytes.size())) {
        std::cerr << "[OID] rejected PLOT_BUFFER_CONTENTS for '"
                  << variable_name << "': payload too small for geometry\n";
        return;
    }
    pixel_layout = resolve_pixel_layout("PLOT_BUFFER_CONTENTS",
                                        variable_name,
                                        std::move(pixel_layout),
                                        channels);
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
    BufferAssembler::BeginParams params;
    MessageDecoder decoder{transport_};
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
    const std::string name = params.variable_name;
    // Captured before the move so a refusal can say what it wanted, mirroring
    // begin()'s rejection reasons one for one so the two never disagree.
    const auto wire_type = static_cast<BufferType>(params.type);
    const bool known_type = is_known_buffer_type(wire_type);
    // padded_payload_size() reports nullopt for two different things, so the
    // shape is checked separately to tell them apart in the message.
    const bool renderable = params.width > 0 && params.height > 0 &&
                            params.channels > 0 &&
                            params.stride >= params.width;
    const bool displayable =
        within_display_limits(params.width, params.height, params.channels);
    const auto expected = padded_payload_size(
        params.width, params.height, params.channels, params.stride, wire_type);
    const auto received = params.total_byte_size;
    // Each rejected BEGIN is a distinct attempted transfer, so there is no
    // flood to collapse and nothing to remember between calls.
    if (!assembler_.begin(std::move(params))) {
        std::cerr << "[OID] rejected PLOT_BUFFER_BEGIN for '" << name << "': ";
        if (!known_type) {
            std::cerr << "unknown buffer type " << type_int << "\n";
        } else if (exceeds_max_buffer_bytes(received)) {
            std::cerr << received << " bytes exceeds the " << MAX_BUFFER_BYTES
                      << " byte limit\n";
        } else if (!displayable) {
            std::cerr << "geometry exceeds the display limits (max dimension "
                      << MAX_BUFFER_DIMENSION << ", max " << MAX_CHANNEL_COUNT
                      << " channels)\n";
        } else if (expected.has_value()) {
            std::cerr << "geometry needs " << *expected << " bytes, got "
                      << received << "\n";
        } else if (renderable) {
            // Reachable only with a 32-bit size_t (wasm): at 64 bits the
            // display limits bound the product far below overflow. Keep it.
            std::cerr << "geometry is too large to size in bytes\n";
        } else {
            std::cerr << "geometry is not renderable (width, height and "
                         "channels must be positive, and stride >= width)\n";
        }
    }
}

void IpcClient::handle_plot_buffer_chunk() {
    std::string name;
    std::size_t row_offset{};
    std::size_t row_count{};
    std::vector<std::byte> bytes;
    MessageDecoder{transport_}
        .read(name)
        .read(row_offset)
        .read(row_count)
        .read(bytes);
    if (assembler_.chunk(name, row_offset, row_count, bytes)) {
        return;
    }
    // abort() frees the partial peer-sized allocation instead of holding it
    // until PLOT_BUFFER_END; gating the report on it having dropped something
    // collapses the flood, since later chunks and stray names find nothing.
    if (assembler_.abort(name)) {
        // Logged separately, not as a row_offset + row_count endpoint: that
        // unchecked sum wraps into an end row below the start row.
        std::cerr << "[OID] rejected PLOT_BUFFER_CHUNK for '" << name
                  << "': row_offset " << row_offset << ", row_count "
                  << row_count << ", " << bytes.size() << " bytes received\n";
    }
}

void IpcClient::handle_plot_buffer_end() {
    std::string name;
    MessageDecoder{transport_}.read(name);
    // Captured before end() (which erases the entry): an END for a name
    // with nothing in flight is a stray, not a genuine incomplete transfer.
    const bool was_in_progress = assembler_.has_in_progress(name);
    if (auto assembled = assembler_.end(name)) {
        assembled->pixel_layout =
            resolve_pixel_layout("PLOT_BUFFER_END",
                                 assembled->variable_name,
                                 std::move(assembled->pixel_layout),
                                 assembled->channels);
        model_.upsert(make_buffer_record(
            {.variable_name = std::move(assembled->variable_name),
             .display_name = std::move(assembled->display_name),
             .pixel_layout = std::move(assembled->pixel_layout),
             .transpose = assembled->transpose,
             .width = assembled->width,
             .height = assembled->height,
             .channels = assembled->channels,
             .stride = assembled->stride,
             .type = static_cast<BufferType>(assembled->type),
             .bytes = std::move(assembled->bytes)}));
    } else if (was_in_progress) {
        std::cerr << "[OID] rejected PLOT_BUFFER_END for '" << name
                  << "': incomplete transfer\n";
    }
}

void IpcClient::handle_apply_session_state() const {
    std::string json;
    MessageDecoder{transport_}.read(json);
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

void IpcClient::request_plot(const std::string& variable_name) const {
    MessageComposer composer;
    composer.push(MessageType::PLOT_BUFFER_REQUEST).push(variable_name);
    send_guarded(composer);
}

void IpcClient::notify_removed(const std::string& variable_name) const {
    MessageComposer composer;
    composer.push(MessageType::BUFFER_REMOVED).push(variable_name);
    send_guarded(composer);
}

void IpcClient::send_session_state_changed(const std::string& json) const {
    MessageComposer composer;
    composer.push(MessageType::SESSION_STATE_CHANGED).push(json);
    send_guarded(composer);
}

void IpcClient::send_export_buffer_request(
    const std::string& variable_name,
    const int format,
    const std::vector<float>& contrast) const {
    MessageComposer composer;
    composer.push(MessageType::EXPORT_BUFFER_REQUEST)
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

void IpcClient::set_restore_buffers(std::vector<PreviousBuffer> buffers) {
    restore_buffers_ = std::move(buffers);
}

std::string IpcClient::resolve_pixel_layout(const std::string_view context,
                                            const std::string& variable_name,
                                            std::string declared_layout,
                                            const int channels) const {
    // Channel order is meaningless for one channel (the shader always renders
    // it from red), so a one-channel layout is convention, not corruption.
    if (channels == 1) {
        return declared_layout;
    }
    if (is_valid_pixel_layout(declared_layout)) {
        return declared_layout;
    }
    // An invalid layout never overwrites a valid one on record: that is the
    // silent corruption this guards against.
    const BufferRecord* existing = nullptr;
    for (std::size_t i = 0; i < model_.size(); ++i) {
        if (model_.variable_name_of(i) == variable_name) {
            existing = &model_.at(i);
            break;
        }
    }
    // The kept layout was declared for the record's shape: after a channel
    // count change its swizzle would address components the texture lacks.
    if (existing != nullptr && existing->channels == channels &&
        is_valid_pixel_layout(existing->pixel_layout)) {
        const std::string& kept = existing->pixel_layout;
        std::cerr << "[OID] " << context << " for '"
                  << log_preview(variable_name, NAME_PREVIEW_CHARS)
                  << "': invalid pixel_layout '" << log_preview(declared_layout)
                  << "'; keeping the existing '" << kept << "' layout\n";
        return kept;
    }
    // Nothing valid to keep either (first plot ever, or an equally invalid
    // existing record): fall back to the documented default, loudly.
    std::cerr << "[OID] " << context << " for '"
              << log_preview(variable_name, NAME_PREVIEW_CHARS)
              << "': invalid pixel_layout '" << log_preview(declared_layout)
              << "' for a " << channels << "-channel buffer; defaulting to \""
              << DEFAULT_PIXEL_LAYOUT << "\"\n";
    return std::string(DEFAULT_PIXEL_LAYOUT);
}

bool IpcClient::model_has(const std::string_view variable_name) const {
    for (std::size_t i = 0; i < model_.size(); ++i) {
        if (model_.variable_name_of(i) == variable_name) {
            return true;
        }
    }
    return false;
}

void IpcClient::send_guarded(const MessageComposer& composer) const {
    try {
        composer.send(transport_);
    } catch (const std::runtime_error&) {
        // Peer gone or transport closed (viewer opened with no debugger).
        // poll() tolerates this inbound; an outbound send must not crash.
    }
}

} // namespace oid::host
