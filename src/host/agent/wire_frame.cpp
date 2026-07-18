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

#include "host/agent/wire_frame.h"

#include <array>
#include <cstdint>
#include <format>
#include <string>

namespace oid::host::agent {

namespace {

std::uint32_t read_be_length(const std::span<const std::byte> header) {
    return std::to_integer<std::uint32_t>(header[0]) << 24 |
           std::to_integer<std::uint32_t>(header[1]) << 16 |
           std::to_integer<std::uint32_t>(header[2]) << 8 |
           std::to_integer<std::uint32_t>(header[3]);
}

// Serialize `obj` to a frame's JSON string, injecting the "payload": <nbytes>
// length hint when a binary trailer follows, and enforcing the frame cap.
std::string dump_frame_json(const nlohmann::json& obj,
                            std::size_t payload_size) {
    nlohmann::json j = obj;
    if (payload_size > 0) {
        j["payload"] = payload_size;
    }
    std::string s = j.dump();
    if (s.size() > MAX_FRAME_BYTES) {
        throw FrameError(
            std::format("JSON frame too large: {} bytes", s.size()));
    }
    return s;
}

// Write the big-endian 4-byte JSON length prefix followed by the JSON bytes
// into the front of `out`, which must already be sized to hold at least them.
// Big-endian byte ops (rather than htonl) keep this translation unit free of
// any platform sockets header. Filling by index (rather than push_back onto a
// reserved vector) avoids a -Wfree-nonheap-object false positive in GCC 15's
// optimizer.
void fill_prefixed_json(std::vector<std::byte>& out, const std::string& s) {
    const auto json_len = static_cast<std::uint32_t>(s.size());
    out[0] = static_cast<std::byte>(json_len >> 24);
    out[1] = static_cast<std::byte>(json_len >> 16);
    out[2] = static_cast<std::byte>(json_len >> 8);
    out[3] = static_cast<std::byte>(json_len);
    std::size_t pos = 4;
    for (const char c : s) {
        out[pos++] = static_cast<std::byte>(c);
    }
}

} // namespace

std::vector<std::byte> encode_frame_header(const nlohmann::json& obj,
                                           const std::size_t payload_size) {
    const std::string s = dump_frame_json(obj, payload_size);
    std::vector<std::byte> out(4 + s.size());
    fill_prefixed_json(out, s);
    return out;
}

std::vector<std::byte> encode_frame(const nlohmann::json& obj,
                                    const std::span<const std::byte> payload) {
    const std::string s = dump_frame_json(obj, payload.size());
    std::vector<std::byte> out(4 + s.size() + payload.size());
    fill_prefixed_json(out, s);
    std::size_t pos = 4 + s.size();
    for (const std::byte b : payload) {
        out[pos++] = b;
    }
    return out;
}

// NOSONAR: std::function kept intentionally -- templating decode_frame would
// force it header-only and destabilize the wire codec.
DecodedFrame decode_frame(
    const std::function<void(std::span<std::byte>)>& read_exact, // NOSONAR
    std::optional<std::size_t> max_payload) {
    std::array<std::byte, 4> header{};
    read_exact(header);
    const std::uint32_t length = read_be_length(header);
    if (length > MAX_FRAME_BYTES) {
        throw FrameError(std::format("JSON frame too large: {} bytes", length));
    }

    std::vector<std::byte> body(length);
    read_exact(body);
    std::string body_str(reinterpret_cast<const char*>(body.data()), // NOSONAR
                         body.size());
    auto obj = nlohmann::json::parse(body_str, nullptr, false);
    if (obj.is_discarded()) {
        throw FrameError("frame is not valid JSON");
    }
    if (!obj.is_object()) {
        throw FrameError("frame is not a JSON object");
    }

    DecodedFrame frame{std::move(obj), {}};
    if (frame.obj.contains("payload")) {
        const auto& payload_field = frame.obj.at("payload");
        if (!payload_field.is_number_integer()) {
            throw FrameError("payload field is not an integer");
        }
        const auto nbytes = payload_field.get<std::int64_t>();
        if (nbytes < 0) {
            throw FrameError(std::format("negative payload size: {}", nbytes));
        }
        // Bound-check the still-widest (int64) representation before ever
        // narrowing to std::size_t: on a 32-bit size_t target (e.g. wasm32)
        // a declared length that overflows size_t would otherwise wrap to a
        // small value and silently defeat both the resize() below and the
        // max_payload cap. Comparisons happen in the uint64 domain so the
        // check itself cannot overflow regardless of the platform's size_t
        // width.
        const auto unbytes64 = static_cast<std::uint64_t>(nbytes);
        if (unbytes64 > static_cast<std::uint64_t>(SIZE_MAX)) {
            throw FrameError(
                std::format("payload size {} exceeds size_t range", nbytes));
        }
        if (max_payload.has_value() &&
            unbytes64 > static_cast<std::uint64_t>(*max_payload)) {
            throw FrameError(std::format(
                "payload of {} bytes exceeds limit {}", nbytes, *max_payload));
        }
        const auto unbytes = static_cast<std::size_t>(nbytes);
        frame.payload.resize(unbytes);
        read_exact(frame.payload);
    }
    return frame;
}

} // namespace oid::host::agent
