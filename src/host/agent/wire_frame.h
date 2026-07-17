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

#ifndef HOST_AGENT_WIRE_FRAME_H_
#define HOST_AGENT_WIRE_FRAME_H_

#include <cstddef>
#include <functional>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

#include <nlohmann/json.hpp>

namespace oid::host::agent {

// Sanity bound for JSON frames only; binary payloads are sized by the
// "payload" field and bounded by the caller-supplied max_payload.
inline constexpr std::size_t MAX_FRAME_BYTES = 1u << 20;

// Raised for every ValueError-equivalent condition the Python
// oidscripts.wireframe module signals: an oversize JSON frame (on encode
// or decode), a non-object frame, or a negative/over-limit payload size.
class FrameError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

// Encodes obj (optionally carrying payload) into the wire format shared with
// oidscripts.wireframe: a 4-byte big-endian length prefix + UTF-8 JSON
// object, followed by the raw payload bytes when non-empty. obj itself is
// never mutated -- the "payload": <nbytes> field is injected into a copy.
// Throws FrameError if the serialized JSON exceeds MAX_FRAME_BYTES.
std::vector<std::byte> encode_frame(const nlohmann::json& obj,
                                    std::span<const std::byte> payload = {});

// Encodes just the framed JSON header -- the 4-byte length prefix + JSON
// object, with "payload": payload_size injected when payload_size > 0 -- but
// WITHOUT appending any payload bytes. Lets a transport write a large binary
// trailer straight from its own buffer (header first, then the raw bytes)
// instead of copying the whole payload through a combined frame. The two
// writes are byte-for-byte identical on the wire to encode_frame(obj, bytes).
// Throws FrameError if the serialized JSON exceeds MAX_FRAME_BYTES.
std::vector<std::byte> encode_frame_header(const nlohmann::json& obj,
                                           std::size_t payload_size);

// One frame as decoded off the wire: the JSON object and its (possibly
// empty) raw binary payload.
struct DecodedFrame {
    nlohmann::json obj;
    std::vector<std::byte> payload;
};

// Pull-parses one frame using read_exact to fill caller-sized buffers
// (read_exact must fill the whole span or throw -- e.g. on peer close).
// Mirrors oidscripts.wireframe.recv_frame: rejects a declared length over
// MAX_FRAME_BYTES, a frame whose JSON does not parse or is not an object,
// and a "payload" field that is not an integer, is negative, exceeds
// max_payload (when given), or exceeds what size_t can address.
// std::nullopt leaves the payload size unbounded.
// NOSONAR: std::function kept intentionally -- templating decode_frame would
// force it header-only and destabilize the wire codec.
DecodedFrame decode_frame(
    const std::function<void(std::span<std::byte>)>& read_exact, // NOSONAR
    std::optional<std::size_t> max_payload);

} // namespace oid::host::agent

#endif // HOST_AGENT_WIRE_FRAME_H_
