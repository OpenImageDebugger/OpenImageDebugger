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
#include <cstdint>
#include <deque>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <string_view>
using namespace oid::host::agent;
using json = nlohmann::json;

// Signals the fake reader() ran out of buffered bytes; a dedicated type so
// it stays distinguishable from a FrameError thrown by the code under test.
class ShortReadError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

static std::function<void(std::span<std::byte>)>
reader(std::vector<std::byte> data) {
    auto buf =
        std::make_shared<std::deque<std::byte>>(data.begin(), data.end());
    return [buf](std::span<std::byte> dst) {
        if (buf->size() < dst.size())
            throw ShortReadError("short");
        for (auto& b : dst) {
            b = buf->front();
            buf->pop_front();
        }
    };
}

TEST(WireFrame, RoundTripNoPayload) {
    const auto bytes = encode_frame(json{{"method", "ping"}});
    auto [obj, payload] = decode_frame(reader(bytes), 0);
    EXPECT_EQ(obj.at("method"), "ping");
    EXPECT_TRUE(payload.empty());
}

TEST(WireFrame, RoundTripWithPayload) {
    std::vector<std::byte> raw(1024);
    for (std::size_t i = 0; i < raw.size(); ++i)
        raw[i] = static_cast<std::byte>(i & 0xff);
    const auto bytes = encode_frame(json{{"ok", true}}, raw);
    auto [obj, payload] = decode_frame(reader(bytes), raw.size());
    EXPECT_EQ(obj.at("payload"), raw.size());
    EXPECT_EQ(payload, raw);
}

TEST(WireFrame, EncodeRejectsOversizeJson) {
    json big{{"blob", std::string(MAX_FRAME_BYTES + 1, 'x')}};
    EXPECT_THROW(encode_frame(big), FrameError);
}

TEST(WireFrame, EncodeHeaderMatchesEncodeFrame) {
    // Writing the header then the raw payload separately must be byte-for-byte
    // identical to a single combined encode_frame(obj, raw).
    std::vector<std::byte> raw(1024);
    for (std::size_t i = 0; i < raw.size(); ++i)
        raw[i] = static_cast<std::byte>(i & 0xff);
    const auto header = encode_frame_header(json{{"ok", true}}, raw.size());
    std::vector<std::byte> combined = header;
    combined.insert(combined.end(), raw.begin(), raw.end());
    EXPECT_EQ(combined, encode_frame(json{{"ok", true}}, raw));

    auto [obj, payload] = decode_frame(reader(combined), raw.size());
    EXPECT_EQ(obj.at("payload"), raw.size());
    EXPECT_EQ(payload, raw);
}

TEST(WireFrame, EncodeHeaderNoPayloadOmitsField) {
    // payload_size 0 leaves the JSON untouched (no "payload" field) and emits
    // no trailer, so it decodes as a plain no-payload frame.
    const auto header = encode_frame_header(json{{"method", "ping"}}, 0);
    EXPECT_EQ(header, encode_frame(json{{"method", "ping"}}));
    auto [obj, payload] = decode_frame(reader(header), 0);
    EXPECT_EQ(obj.at("method"), "ping");
    EXPECT_FALSE(obj.contains("payload"));
    EXPECT_TRUE(payload.empty());
}

TEST(WireFrame, EncodeHeaderRejectsOversizeJson) {
    json big{{"blob", std::string(MAX_FRAME_BYTES + 1, 'x')}};
    EXPECT_THROW(encode_frame_header(big, 0), FrameError);
}

TEST(WireFrame, DecodeRejectsOversizePayload) {
    std::vector raw(100, std::byte{'x'});
    auto bytes = encode_frame(json{{"ok", true}}, raw);
    EXPECT_THROW(decode_frame(reader(bytes), 10), FrameError);
}

TEST(WireFrame, DecodeRejectsNonObject) {
    // hand-build a frame whose body is the JSON array [1,2]
    std::string body = "[1,2]";
    std::vector<std::byte> bytes(4 + body.size());
    auto n = static_cast<std::uint32_t>(body.size());
    bytes[0] = static_cast<std::byte>(n >> 24);
    bytes[1] = static_cast<std::byte>(n >> 16);
    bytes[2] = static_cast<std::byte>(n >> 8);
    bytes[3] = static_cast<std::byte>(n);
    for (std::size_t i = 0; i < body.size(); ++i)
        bytes[4 + i] = static_cast<std::byte>(body[i]);
    EXPECT_THROW(decode_frame(reader(bytes), 0), FrameError);
}

TEST(WireFrame, DecodeRejectsNonIntegerPayloadField) {
    // "payload" must be an integer length; a string must raise a
    // FrameError, not an nlohmann::json type_error from get<int64_t>().
    auto bytes = encode_frame(json{{"payload", "not-a-length"}});
    EXPECT_THROW(decode_frame(reader(bytes), 0), FrameError);
}

TEST(WireFrame, DecodeRejectsPayloadAboveInt64Max) {
    // nlohmann's get<int64_t>() static_casts unchecked, so UINT64_MAX becomes
    // -1 and trips the negative-size check, not the size_t guard used below.
    auto bytes = encode_frame(json{{"payload", UINT64_MAX}});
    EXPECT_THROW(decode_frame(reader(bytes), 0), FrameError);
}

TEST(WireFrame, DecodeRejectsPayloadOverMaxBeforeNarrowing) {
    // max_payload must be compared widened to uint64_t: on a 32-bit size_t
    // target (wasm32) this length wraps to 0 and bypasses the cap. On 64-bit
    // hosts the narrowing is a no-op, so this pins the ordering, not the wrap.
    auto bytes = encode_frame(json{{"payload", std::int64_t{1} << 32}});
    EXPECT_THROW(decode_frame(reader(bytes), 0), FrameError);
}

// Golden fixtures are Python-encoded wire bytes; parity is pinned by decode,
// not bytes: nlohmann's dump() differs from json.dumps (whitespace, order).

static std::vector<std::byte> read_golden_bytes(const std::string& name) {
    std::ifstream is{std::filesystem::path(GOLDEN_DIR) / name,
                     std::ios::binary};
    std::vector<std::byte> data;
    for (std::istreambuf_iterator<char> it{is}, end; it != end; ++it) {
        data.push_back(static_cast<std::byte>(*it));
    }
    return data;
}

static json golden_manifest() {
    std::ifstream is{std::filesystem::path(GOLDEN_DIR) / "manifest.json"};
    json manifest;
    is >> manifest;
    return manifest;
}

static std::vector<std::byte> hex_to_bytes(std::string_view hex) {
    std::vector<std::byte> out(hex.size() / 2);
    for (std::size_t i = 0; i < out.size(); ++i) {
        out[i] = static_cast<std::byte>(
            std::stoul(std::string(hex.substr(i * 2, 2)), nullptr, 16));
    }
    return out;
}

TEST(WireFrame, GoldenPingFixtureDecodesAndRoundTrips) {
    const json manifest = golden_manifest();
    const json& expected_obj = manifest.at("ping.bin").at("decoded_obj");

    auto bytes = read_golden_bytes("ping.bin");
    auto [obj, payload] = decode_frame(reader(bytes), 0);
    EXPECT_EQ(obj, expected_obj);
    EXPECT_TRUE(payload.empty());

    auto own_bytes = encode_frame(expected_obj);
    auto round_tripped = decode_frame(reader(own_bytes), 0);
    EXPECT_EQ(round_tripped.obj, expected_obj);
    EXPECT_TRUE(round_tripped.payload.empty());
}

TEST(WireFrame, GoldenOkPayloadFixtureDecodesAndRoundTrips) {
    const json manifest = golden_manifest();
    const json& entry = manifest.at("ok_payload.bin");
    const json& expected_obj = entry.at("decoded_obj");
    auto expected_payload =
        hex_to_bytes(entry.at("payload_hex").get<std::string>());

    auto bytes = read_golden_bytes("ok_payload.bin");
    auto [obj, payload] = decode_frame(reader(bytes), expected_payload.size());
    EXPECT_EQ(obj, expected_obj);
    EXPECT_EQ(payload, expected_payload);

    json input_obj = expected_obj;
    input_obj.erase("payload"); // encode_frame injects this itself
    auto own_bytes = encode_frame(input_obj, expected_payload);
    auto round_tripped =
        decode_frame(reader(own_bytes), expected_payload.size());
    EXPECT_EQ(round_tripped.obj, expected_obj);
    EXPECT_EQ(round_tripped.payload, expected_payload);
}
