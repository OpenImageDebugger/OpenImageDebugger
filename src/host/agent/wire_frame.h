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

// Mirrors every ValueError the Python oidscripts.wireframe module raises.
class FrameError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

// obj is never mutated: "payload": <nbytes> is injected into a copy.
std::vector<std::byte> encode_frame(const nlohmann::json& obj,
                                    std::span<const std::byte> payload = {});

// Header plus a separate write is byte-identical to encode_frame(obj, bytes).
std::vector<std::byte> encode_frame_header(const nlohmann::json& obj,
                                           std::size_t payload_size);

// One frame as decoded off the wire: the JSON object and its (possibly
// empty) raw binary payload.
struct DecodedFrame {
    nlohmann::json obj;
    std::vector<std::byte> payload;
};

// read_exact must fill the whole span or throw; nullopt is unbounded.

// NOSONAR: templating decode_frame would force it header-only.
DecodedFrame decode_frame(
    const std::function<void(std::span<std::byte>)>& read_exact, // NOSONAR
    std::optional<std::size_t> max_payload);

} // namespace oid::host::agent

#endif // HOST_AGENT_WIRE_FRAME_H_
