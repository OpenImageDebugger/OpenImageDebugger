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

#include "host/agent/agent_core.h"

#include <gtest/gtest.h>

#include "host/agent/fake_view_model.h"
#include "ipc/raw_data_decode.h"

using namespace oid::host::agent;
using json = nlohmann::json;

TEST(AgentCore, RejectsMethodsBeforeHello) {
    FakeViewModel m;
    AgentCore core(m, "tok", 4242);
    bool authed = false;
    auto r = core.handle({{"method", "ping"}}, authed);
    EXPECT_EQ(r.body["error"]["code"], "bad_token");
    EXPECT_FALSE(authed);
}

TEST(AgentCore, HelloRejectsOversizedToken) {
    FakeViewModel m;
    AgentCore core(m, "tok", 4242);
    bool a = false;
    // A wrong-length token is rejected before the constant-time compare, so an
    // oversized token cannot force per-byte work over a huge input.
    auto r = core.handle(
        {{"method", "hello"}, {"token", std::string(100000, 'x')}}, a);
    EXPECT_EQ(r.body["error"]["code"], "bad_token");
    EXPECT_FALSE(a);
}

TEST(AgentCore, HelloThenPing) {
    FakeViewModel m;
    AgentCore core(m, "tok", 4242);
    bool a = false;
    core.handle({{"method", "hello"}, {"token", "tok"}}, a);
    EXPECT_TRUE(a);
    EXPECT_TRUE(core.handle({{"method", "ping"}}, a).body["ok"]);
}

TEST(AgentCore, HelloRejectsBadToken) {
    FakeViewModel m;
    AgentCore core(m, "tok", 4242);
    bool a = false;
    auto r = core.handle({{"method", "hello"}, {"token", "wrong"}}, a);
    EXPECT_EQ(r.body["error"]["code"], "bad_token");
    EXPECT_FALSE(a);
}

TEST(AgentCore, BadReHelloDoesNotRevokeAuth) {
    FakeViewModel m;
    AgentCore core(m, "tok", 4242);
    bool a = false;
    core.handle({{"method", "hello"}, {"token", "tok"}}, a);
    ASSERT_TRUE(a);
    // A stray or malformed re-hello on an already-authenticated connection
    // must not drop it back to the pre-auth state (where the now-expired
    // absolute handshake deadline would then tear it down): a bad token fails
    // to re-grant auth but must never revoke it.
    auto r = core.handle({{"method", "hello"}, {"token", "wrong"}}, a);
    EXPECT_EQ(r.body["error"]["code"], "bad_token");
    EXPECT_TRUE(a);
    EXPECT_TRUE(core.handle({{"method", "ping"}}, a).body["ok"]);
}

TEST(AgentCore, HelloResponseShape) {
    FakeViewModel m;
    AgentCore core(m, "tok", 4242);
    bool a = false;
    auto r = core.handle({{"method", "hello"}, {"token", "tok"}}, a);
    EXPECT_EQ(r.body["version"], 1);
    EXPECT_EQ(r.body["kind"], "viewer");
    EXPECT_EQ(r.body["pid"], 4242);
}

TEST(AgentCore, UnknownMethodAfterHello) {
    FakeViewModel m;
    AgentCore core(m, "tok", 4242);
    bool a = true;
    auto r = core.handle({{"method", "nope"}}, a);
    EXPECT_EQ(r.body["error"]["code"], "unknown_method");
}

TEST(AgentCore, ListBuffers) {
    FakeViewModel m;
    m.add("a", 4, 5, 1);
    m.add("b", 6, 7, 3);
    AgentCore core(m, "tok", 4242);
    bool a = true;
    auto r = core.handle({{"method", "list_buffers"}}, a).body;
    ASSERT_EQ(r["buffers"].size(), 2u);
    EXPECT_EQ(r["buffers"][0]["name"], "a");
    EXPECT_EQ(r["buffers"][0]["width"], 4);
    EXPECT_EQ(r["buffers"][1]["name"], "b");
    EXPECT_EQ(r["buffers"][1]["channels"], 3);
    // list_buffers uses the same "transpose_buffer" field name as get_buffer
    // (and the whole oid-mcp/oidscripts convention), not a bare "transpose".
    EXPECT_TRUE(r["buffers"][0].contains("transpose_buffer"));
    EXPECT_FALSE(r["buffers"][0].contains("transpose"));
}

TEST(AgentCore, GetBufferSuccess) {
    FakeViewModel m;
    m.add("frame", 2, 2, 1);
    m.set_bytes("frame", std::vector<std::byte>(4, std::byte{7}));
    AgentCore core(m, "tok", 4242);
    bool a = true;
    auto [body, payload] =
        core.handle({{"method", "get_buffer"}, {"symbol", "frame"}}, a);
    EXPECT_EQ(body["width"], 2);
    EXPECT_EQ(body["height"], 2);
    ASSERT_EQ(payload.size(), 4u);
    EXPECT_EQ(payload[0], std::byte{7});
}

TEST(AgentCore, GetBufferTooLarge) {
    FakeViewModel m;
    m.add("frame", 8, 8, 1);
    m.set_bytes("frame", std::vector<std::byte>(1000));
    AgentCore core(m, "tok", 4242);
    bool a = true;
    auto r = core.handle(
        {{"method", "get_buffer"}, {"symbol", "frame"}, {"max_bytes", 10}}, a);
    EXPECT_EQ(r.body["error"]["code"], "too_large");
}

TEST(AgentCore, GetBufferTooLargeSkipsReadPixels) {
    FakeViewModel m;
    // step (1000*1) * height (1000) is declared as far past max_bytes; no
    // set_bytes() call, so if read_pixels() were reached the fake would
    // return false (no pixel_data entry) and yield "internal", not
    // "too_large" -- proving the cap is enforced before that call.
    m.add("frame", 1000, 1000, 1);
    AgentCore core(m, "tok", 4242);
    bool a = true;
    auto r = core.handle(
        {{"method", "get_buffer"}, {"symbol", "frame"}, {"max_bytes", 10}}, a);
    EXPECT_EQ(r.body["error"]["code"], "too_large");
    EXPECT_EQ(m.read_pixels_calls, 0);
}

TEST(AgentCore, GetBufferRejectsInvalidDimsBeforeRead) {
    FakeViewModel m;
    // width 0 => step 0: a malformed BufferRecord whose declared size can't be
    // computed. Even with large backing bytes, it must be rejected before
    // read_pixels() copies them (read_pixels_calls stays 0).
    m.add("bad", 0, 5, 1);
    m.set_bytes("bad", std::vector<std::byte>(4096));
    AgentCore core(m, "tok", 4242);
    bool a = true;
    auto r = core.handle({{"method", "get_buffer"}, {"symbol", "bad"}}, a);
    EXPECT_EQ(r.body["error"]["code"], "internal");
    EXPECT_EQ(m.read_pixels_calls, 0);
}

TEST(AgentCore, GetBufferRejectsNonPositiveWidthWithPositiveStep) {
    FakeViewModel m;
    // The fake couples step to width, but the real ingest pipeline sets them
    // independently: width <= 0 with a positive step slips past a step<=0 guard
    // (the declared-bytes fold uses step, not width), yet the width sent to the
    // client would break decode_buffer (needs width > 0). Reject before
    // read_pixels() copies the backing vector.
    m.add("bad", 4, 5, 1);
    m.buffers.back().width = 0;
    m.set_bytes("bad", std::vector<std::byte>(4096));
    AgentCore core(m, "tok", 4242);
    bool a = true;
    auto r = core.handle({{"method", "get_buffer"}, {"symbol", "bad"}}, a);
    EXPECT_EQ(r.body["error"]["code"], "internal");
    EXPECT_EQ(m.read_pixels_calls, 0);
}

TEST(AgentCore, GetBufferRejectsStrideNarrowerThanWidth) {
    FakeViewModel m;
    // A row stride (step, in pixels) narrower than width is an inconsistent
    // BufferRecord: step > 0 so a step<=0 guard misses it, but oid-mcp's
    // decode_buffer requires stride >= width. Reject before read_pixels().
    m.add("bad", 8, 5, 1);
    m.buffers.back().step = 4;
    m.set_bytes("bad", std::vector<std::byte>(4096));
    AgentCore core(m, "tok", 4242);
    bool a = true;
    auto r = core.handle({{"method", "get_buffer"}, {"symbol", "bad"}}, a);
    EXPECT_EQ(r.body["error"]["code"], "internal");
    EXPECT_EQ(m.read_pixels_calls, 0);
}

TEST(AgentCore, GetBufferWideElementTypeRejectedBeforeRead) {
    FakeViewModel m;
    // step is a *pixel* count (GL_UNPACK_ROW_LENGTH), so step * height
    // alone undercounts a FLOAT32 buffer's real byte size: 100 * 100 =
    // 10'000 looks well under max_bytes, but each pixel is actually 4
    // bytes wide, so the true size is step * channels * sizeof(float) *
    // height = 100 * 1 * 4 * 100 = 40'000 bytes. No set_bytes() call, so
    // if read_pixels() were reached the fake would return false and the
    // reply would be "internal", not "too_large" -- proving the
    // type-aware cap rejects the buffer before that call.
    m.add("frame", 100, 100, 1);
    m.buffers.back().type = static_cast<int>(oid::BufferType::FLOAT32);
    AgentCore core(m, "tok", 4242);
    bool a = true;
    auto r = core.handle(
        {{"method", "get_buffer"}, {"symbol", "frame"}, {"max_bytes", 20000}},
        a);
    EXPECT_EQ(r.body["error"]["code"], "too_large");
    EXPECT_EQ(m.read_pixels_calls, 0);
}

TEST(AgentCore, UnknownBuffer) {
    FakeViewModel m;
    AgentCore core(m, "tok", 4242);
    bool a = true;
    auto r = core.handle({{"method", "get_buffer"}, {"symbol", "nope"}}, a);
    EXPECT_EQ(r.body["error"]["code"], "unknown_buffer");
}

TEST(AgentCore, EmptyViewerGetView) {
    FakeViewModel m;
    AgentCore core(m, "tok", 4242);
    bool a = true;
    auto r = core.handle({{"method", "get_view"}}, a).body;
    EXPECT_TRUE(r["buffer"].is_null());
    EXPECT_TRUE(r["center"].is_null());
    EXPECT_TRUE(r["zoom"].is_null());
    EXPECT_TRUE(r["rotation_deg"].is_null());
    EXPECT_TRUE(r["channel"].is_null());
    EXPECT_FALSE(r["auto_contrast"].is_null());
    ASSERT_TRUE(r["viewport"].is_array());
}

TEST(AgentCore, SetViewIsAbsoluteIdempotent) {
    FakeViewModel m;
    m.add("frame", 64, 48, 3);
    AgentCore core(m, "tok", 4242);
    bool a = true;
    json patch{{"method", "set_view"},
               {"buffer", "frame"},
               {"zoom", 1.5},
               {"rotation_deg", 90},
               {"channel", 1}};
    auto r1 = core.handle(patch, a);
    auto r2 = core.handle(patch, a);
    EXPECT_EQ(r1.body, r2.body);
    EXPECT_NEAR(r1.body["zoom"], 1.5, 1e-6);
    EXPECT_NEAR(r1.body["rotation_deg"], 90.0, 1e-6);
    EXPECT_EQ(r1.body["channel"], "1");
    EXPECT_EQ(r1.body["buffer"], "frame");
}

TEST(AgentCore, SetViewAcceptsStringChannelAndCenterAndAutoContrast) {
    FakeViewModel m;
    m.add("frame", 64, 48, 3);
    AgentCore core(m, "tok", 4242);
    bool a = true;
    auto r = core.handle({{"method", "set_view"},
                          {"buffer", "frame"},
                          {"channel", "2"},
                          {"center", json::array({3.0, 4.0})},
                          {"auto_contrast", false}},
                         a);
    EXPECT_EQ(r.body["channel"], "2");
    EXPECT_EQ(r.body["center"], json::array({3.0, 4.0}));
    EXPECT_EQ(r.body["auto_contrast"], false);
}

TEST(AgentCore, SetViewRejectsFloatOverflowingValues) {
    FakeViewModel m;
    m.add("frame", 64, 48, 3);
    AgentCore core(m, "tok", 4242);
    bool a = true;
    // A finite double that overflows to +/-inf once the native adapter
    // narrows to float must be rejected (bad_params), not forwarded -- it
    // would install non-finite Camera state and yield NaN read-back.
    auto r = core.handle(
        {{"method", "set_view"}, {"buffer", "frame"}, {"rotation_deg", 1e300}},
        a);
    EXPECT_EQ(r.body["error"]["code"], "bad_params");
    auto r2 = core.handle({{"method", "set_view"},
                           {"buffer", "frame"},
                           {"center", json::array({1e300, 0.0})}},
                          a);
    EXPECT_EQ(r2.body["error"]["code"], "bad_params");
}

TEST(AgentCore, SetViewRejectsUnderflowingZoom) {
    FakeViewModel m;
    m.add("frame", 64, 48, 3);
    AgentCore core(m, "tok", 4242);
    bool a = true;
    // A positive-but-tiny zoom narrows to a subnormal/zero float in the
    // camera's pow-based zoom, where 1/zoom blows up to +inf; reject it.
    auto r = core.handle(
        {{"method", "set_view"}, {"buffer", "frame"}, {"zoom", 1e-300}}, a);
    EXPECT_EQ(r.body["error"]["code"], "bad_params");
}

TEST(AgentCore, SetViewRejectsHugeZoom) {
    FakeViewModel m;
    m.add("frame", 64, 48, 3);
    AgentCore core(m, "tok", 4242);
    bool a = true;
    // Finite but so large that its engine zoom-power overflows the camera's
    // float pow(); reject it before a non-finite scale is installed.
    auto r = core.handle(
        {{"method", "set_view"}, {"buffer", "frame"}, {"zoom", 3.3e38}}, a);
    EXPECT_EQ(r.body["error"]["code"], "bad_params");
}

TEST(AgentCore, SetViewNormalizesRotation) {
    FakeViewModel m;
    m.add("frame", 8, 8, 3);
    AgentCore core(m, "tok", 4242);
    bool a = true;
    // set_view accepts any finite angle but normalizes it into [0, 360) so the
    // accepted value matches the get_view read-back contract.
    auto r = core.handle(
        {{"method", "set_view"}, {"buffer", "frame"}, {"rotation_deg", 450.0}},
        a);
    EXPECT_NEAR(r.body["rotation_deg"], 90.0, 1e-6);
    auto r2 = core.handle(
        {{"method", "set_view"}, {"buffer", "frame"}, {"rotation_deg", -90.0}},
        a);
    EXPECT_NEAR(r2.body["rotation_deg"], 270.0, 1e-6);
}

TEST(AgentCore, SetViewChannelAllRestoresNaturalLayout) {
    FakeViewModel m;
    m.add("frame", 8, 8, 3);
    AgentCore core(m, "tok", 4242);
    bool a = true;
    core.handle({{"method", "set_view"}, {"buffer", "frame"}, {"channel", 1}},
                a);
    auto r = core.handle(
        {{"method", "set_view"}, {"buffer", "frame"}, {"channel", "all"}}, a);
    EXPECT_EQ(r.body["channel"], "all");
}

TEST(AgentCore, SetViewRejectsAlphaChannel) {
    FakeViewModel m;
    m.add("rgba", 8, 8, 4);
    AgentCore core(m, "tok", 4242);
    bool a = true;
    auto r = core.handle(
        {{"method", "set_view"}, {"buffer", "rgba"}, {"channel", 3}}, a);
    EXPECT_EQ(r.body["error"]["code"], "bad_params");
}

TEST(AgentCore, SetViewRejectsUnknownBuffer) {
    FakeViewModel m;
    AgentCore core(m, "tok", 4242);
    bool a = true;
    auto r = core.handle(
        {{"method", "set_view"}, {"buffer", "nope"}, {"zoom", 2.0}}, a);
    EXPECT_EQ(r.body["error"]["code"], "unknown_buffer");
}

TEST(AgentCore, SetViewRejectsNonFiniteZoom) {
    FakeViewModel m;
    m.add("frame", 8, 8, 1);
    AgentCore core(m, "tok", 4242);
    bool a = true;
    auto r = core.handle(
        {{"method", "set_view"}, {"buffer", "frame"}, {"zoom", -3}}, a);
    EXPECT_EQ(r.body["error"]["code"], "bad_params");
}

TEST(AgentCore, SetViewNoBufferToApplyViewTo) {
    FakeViewModel m;
    m.add("frame", 8, 8, 1);
    AgentCore core(m, "tok", 4242);
    bool a = true;
    auto r = core.handle({{"method", "set_view"}, {"zoom", 2.0}}, a);
    EXPECT_EQ(r.body["error"]["code"], "bad_params");
}

TEST(AgentCore, SetViewMutatorFailureIsInternalError) {
    FakeViewModel m;
    m.add("frame", 8, 8, 1);
    m.fail_set_zoom_power = true;
    AgentCore core(m, "tok", 4242);
    bool a = true;
    auto r = core.handle(
        {{"method", "set_view"}, {"buffer", "frame"}, {"zoom", 2.0}}, a);
    EXPECT_EQ(r.body["error"]["code"], "internal");
}

TEST(AgentCore, SetViewInvalidPatchAppliesNothing) {
    FakeViewModel m;
    m.add("frame", 8, 8, 1);
    AgentCore core(m, "tok", 4242);
    bool a = true;
    const auto before = core.handle({{"method", "get_view"}}, a).body;
    core.handle({{"method", "set_view"}, {"zoom", -3}}, a); // invalid
    const auto after = core.handle({{"method", "get_view"}}, a).body;
    EXPECT_EQ(before, after);
}
