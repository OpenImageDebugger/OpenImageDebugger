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

#include "host/ipc/buffer_decode.h"
#include "host/ipc/ipc_client.h"
#include "ipc/message_exchange.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <deque>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string_view>

#include <gtest/gtest.h>

namespace oid::host {

TEST(BufferDecode, MapsFieldsAndStepEqualsStride) {
    std::vector bytes(12, std::byte{7});
    BufferRecord r = make_buffer_record({.variable_name = "v",
                                         .display_name = "disp",
                                         .pixel_layout = "rgb",
                                         .transpose = false,
                                         .width = 2,
                                         .height = 2,
                                         .channels = 3,
                                         .stride = 6,
                                         .type = BufferType::UNSIGNED_BYTE,
                                         .bytes = std::move(bytes)});
    EXPECT_EQ(r.variable_name, "v");
    EXPECT_EQ(r.display_name, "disp");
    EXPECT_EQ(r.pixel_layout, "rgb");
    EXPECT_FALSE(r.transpose);
    EXPECT_EQ(r.width, 2);
    EXPECT_EQ(r.height, 2);
    EXPECT_EQ(r.channels, 3);
    EXPECT_EQ(r.step, 6); // step == wire stride
    EXPECT_EQ(r.type, oid::BufferType::UNSIGNED_BYTE);
    EXPECT_EQ(r.bytes.size(), 12u);
}

TEST(BufferDecode, Float64ConvertsToFloatBytes) {
    constexpr double value = 3.5;
    std::vector<std::byte> bytes(sizeof(double));
    std::memcpy(bytes.data(), &value, sizeof(double));

    BufferRecord r = make_buffer_record({.variable_name = "v",
                                         .display_name = "disp",
                                         .pixel_layout = "",
                                         .transpose = false,
                                         .width = 1,
                                         .height = 1,
                                         .channels = 1,
                                         .stride = 1,
                                         .type = BufferType::FLOAT64,
                                         .bytes = std::move(bytes)});

    // FLOAT64 payload of 1 double must convert down to 1 float (4 bytes).
    ASSERT_EQ(r.bytes.size(), sizeof(float));
    // BufferRecord::type stays FLOAT64, unchanged, matching the Qt path.
    EXPECT_EQ(r.type, oid::BufferType::FLOAT64);

    float decoded = 0.0F;
    std::memcpy(&decoded, r.bytes.data(), sizeof(float));
    EXPECT_FLOAT_EQ(decoded, value);
}

} // namespace oid::host

using namespace oid;

// Fake transport: `inbound` is a queue of bytes handed out (possibly short)
// via receive(); sent frames are recorded verbatim in `sends`.
struct FakeTransport final : ITransport {
    std::vector<std::byte> inbound;
    std::size_t pos = 0;
    std::vector<std::vector<std::byte>> sends;

    void send(std::span<const std::byte> d) override {
        sends.emplace_back(d.begin(), d.end());
    }

    std::size_t receive(std::span<std::byte> dst) override {
        const std::size_t n = (std::min)(dst.size(), inbound.size() - pos);
        std::memcpy(dst.data(), inbound.data() + pos, n);
        pos += n;
        return n; // may short-read
    }

    bool has_data() const override {
        return pos < inbound.size();
    }

    void feed(const std::vector<std::byte>& f) {
        inbound.insert(inbound.end(), f.begin(), f.end());
    }
};

// Transport whose send() always throws, simulating a dead/closed peer (e.g.
// the viewer opened with no debugger attached).
struct ThrowingTransport final : ITransport {
    [[noreturn]] void send(std::span<const std::byte>) override {
        throw std::runtime_error("transport is dead");
    }

    std::size_t receive(std::span<std::byte>) override {
        return 0;
    }

    bool has_data() const override {
        return false;
    }
};

// Redirects std::cerr into a buffer for the test's duration, restoring the
// original streambuf on destruction (production code logs via std::cerr
// directly, so this is the only hook available to observe it).
struct CerrCapture {
    std::ostringstream out;
    std::streambuf* saved = std::cerr.rdbuf(out.rdbuf());
    ~CerrCapture() {
        std::cerr.rdbuf(saved);
    }
};

// Captures the bytes a MessageComposer would send, without a real socket.
static std::vector<std::byte> frame(const MessageComposer& c) {
    struct Cap final : ITransport {
        std::vector<std::byte> b;
        void send(std::span<const std::byte> d) override {
            b.assign(d.begin(), d.end());
        }
        std::size_t receive(std::span<std::byte>) override {
            return 0;
        }
        bool has_data() const override {
            return false;
        }
    };
    Cap cap;
    c.send(cap);
    return cap.b;
}

// Builds a PLOT_BUFFER_BEGIN frame for `name`: single-channel, row-major
// geometry (channels=1, stride=width) so callers only need to pick width,
// height and the total payload size.
static std::vector<std::byte> begin_frame(const std::string& name,
                                          const int width,
                                          const int height,
                                          const std::size_t total_byte_size) {
    MessageComposer c;
    c.push(MessageType::PLOT_BUFFER_BEGIN)
        .push(name)
        .push(std::string("disp"))
        .push(std::string("rgb"))
        .push(false)
        .push(width)
        .push(height)
        .push(1)
        .push(width)
        .push(static_cast<int>(BufferType::UNSIGNED_BYTE))
        .push(total_byte_size);
    return frame(c);
}

// Like begin_frame, but exposes channels, stride and type: the
// geometry-vs-payload checks need shapes begin_frame's fixed
// channels=1/stride=width can't reach.
static std::vector<std::byte>
begin_frame_ex(const std::string& name,
               const int width,
               const int height,
               const int channels,
               const int stride,
               const BufferType type,
               const std::size_t total_byte_size) {
    MessageComposer c;
    c.push(MessageType::PLOT_BUFFER_BEGIN)
        .push(name)
        .push(std::string("disp"))
        .push(std::string("rgb"))
        .push(false)
        .push(width)
        .push(height)
        .push(channels)
        .push(stride)
        .push(static_cast<int>(type))
        .push(total_byte_size);
    return frame(c);
}

static std::vector<std::byte>
chunk_frame(const std::string& name,
            const std::size_t row_offset,
            const std::size_t row_count,
            const std::span<const std::byte> bytes) {
    MessageComposer c;
    c.push(MessageType::PLOT_BUFFER_CHUNK)
        .push(name)
        .push(row_offset)
        .push(row_count)
        .push(bytes);
    return frame(c);
}

static std::vector<std::byte> end_frame(const std::string& name) {
    MessageComposer c;
    c.push(MessageType::PLOT_BUFFER_END).push(name);
    return frame(c);
}

// Counts non-overlapping occurrences of `needle` in `haystack`; used instead
// of find() != npos so tests assert an exact log count, not just presence.
static std::size_t count_occurrences(const std::string_view haystack,
                                     const std::string_view needle) {
    std::size_t count = 0;
    for (std::size_t pos = haystack.find(needle); pos != std::string_view::npos;
         pos = haystack.find(needle, pos + needle.size())) {
        ++count;
    }
    return count;
}

TEST(IpcClient, PlotBufferContentsUpsertsModel) {
    FakeTransport t;
    host::IpcBufferModel model;
    MessageComposer c;
    // 2x2 rgb8, unpadded: stride counts PIXELS per row, so it equals width
    // here, and the payload is channels * stride * height = 12 bytes.
    std::vector bytes(12, std::byte{5});
    c.push(MessageType::PLOT_BUFFER_CONTENTS)
        .push(std::string("v"))
        .push(std::string("disp"))
        .push(std::string("rgb"))
        .push(false)
        .push(2)
        .push(2)
        .push(3)
        .push(2)
        .push(BufferType::UNSIGNED_BYTE)
        .push(std::span<const std::byte>(bytes));
    t.feed(frame(c));

    host::IpcClient client(t, model);
    client.poll();

    ASSERT_EQ(model.size(), 1u);
    EXPECT_EQ(model.at(0).variable_name, "v");
    EXPECT_EQ(model.at(0).step, 2);
    EXPECT_EQ(model.at(0).bytes.size(), 12u);
}

// The single-shot sibling of the chunked-path fix: PLOT_BUFFER_CONTENTS
// carries its whole payload in one message, with no BufferAssembler::begin()
// to reject it, so handle_plot_buffer_contents() must apply the same check
// itself.
TEST(IpcClient, PlotBufferContentsRejectsPayloadTooSmallForGeometry) {
    FakeTransport t;
    host::IpcBufferModel model;
    MessageComposer c;
    // width=4, height=2, channels=1, stride=100000, FLOAT32 needs 100004
    // pixels x 4 bytes; 2 bytes is nowhere close.
    const std::vector bytes(2, std::byte{5});
    c.push(MessageType::PLOT_BUFFER_CONTENTS)
        .push(std::string("v"))
        .push(std::string("disp"))
        .push(std::string("rgb"))
        .push(false)
        .push(4)
        .push(2)
        .push(1)
        .push(100000)
        .push(BufferType::FLOAT32)
        .push(std::span<const std::byte>(bytes));
    t.feed(frame(c));

    host::IpcClient client(t, model);
    CerrCapture cap;
    client.poll();

    EXPECT_EQ(model.size(), 0u);
    const std::string logged = cap.out.str();
    EXPECT_NE(logged.find("rejected PLOT_BUFFER_CONTENTS"), std::string::npos);
    EXPECT_NE(logged.find("'v'"), std::string::npos);
    EXPECT_NE(logged.find("payload too small for geometry"), std::string::npos);
}

// The other half of the deliberate asymmetry: this is the exact geometry
// LastRowOmittingTrailingStridePaddingIsRejectedOnChunkedPath refuses, and
// the single-shot path takes it. Nothing here assembles row strips, so a
// non-uniform final row is harmless and the floor is all that is required.
TEST(IpcClient, PlotBufferContentsAcceptsLastRowOmittingStridePadding) {
    FakeTransport t;
    host::IpcBufferModel model;
    MessageComposer c;
    // width=3, height=2, channels=1, stride=5 (pixels/row): floor is
    // (height-1)*stride+width = 8 bytes, fully padded would be 10.
    const std::vector bytes(8, std::byte{5});
    c.push(MessageType::PLOT_BUFFER_CONTENTS)
        .push(std::string("v"))
        .push(std::string("disp"))
        .push(std::string("rgb"))
        .push(false)
        .push(3)
        .push(2)
        .push(1)
        .push(5)
        .push(BufferType::UNSIGNED_BYTE)
        .push(std::span<const std::byte>(bytes));
    t.feed(frame(c));

    host::IpcClient client(t, model);
    CerrCapture cap;
    client.poll();

    ASSERT_EQ(model.size(), 1u);
    EXPECT_EQ(model.at(0).bytes.size(), 8u);
    EXPECT_TRUE(cap.out.str().empty());
}

TEST(IpcClient, SetAvailableSymbolsPopulatesList) {
    FakeTransport t;
    host::IpcBufferModel model;
    MessageComposer c;
    std::deque<std::string> syms{"a", "b"};
    c.push(MessageType::SET_AVAILABLE_SYMBOLS).push(syms.size());
    for (const auto& s : syms) {
        c.push(s);
    }
    t.feed(frame(c));

    host::IpcClient client(t, model);
    client.poll();

    ASSERT_EQ(client.available_symbols().size(), 2u);
    EXPECT_EQ(client.available_symbols()[0], "a");
}

TEST(IpcClient, GetObservedSymbolsRespondsWithModelNames) {
    FakeTransport t;
    host::IpcBufferModel model;
    {
        host::BufferRecord r;
        r.variable_name = "x";
        model.upsert(std::move(r));
    }
    MessageComposer c;
    c.push(MessageType::GET_OBSERVED_SYMBOLS);
    t.feed(frame(c));

    host::IpcClient client(t, model);
    client.poll();

    ASSERT_EQ(t.sends.size(), 1u); // sent a GET_OBSERVED_SYMBOLS_RESPONSE
    MessageType h{};
    std::memcpy(&h, t.sends[0].data(), sizeof(h));
    EXPECT_EQ(h, MessageType::GET_OBSERVED_SYMBOLS_RESPONSE);
}

// LOCAL_FILE-tagged records (buffers opened directly from a local file, not
// owned by the debugger) must never be advertised back via
// GET_OBSERVED_SYMBOLS_RESPONSE -- only DEBUGGER_SYMBOL records belong in
// that reply.
TEST(IpcClient, GetObservedSymbolsExcludesLocalFileBuffers) {
    FakeTransport t;
    host::IpcBufferModel model;
    {
        host::BufferRecord r;
        r.variable_name = "debugger_var";
        r.kind = host::BufferKind::DEBUGGER_SYMBOL;
        model.upsert(std::move(r));
    }
    {
        host::BufferRecord r;
        r.variable_name = "local_file.png";
        r.kind = host::BufferKind::LOCAL_FILE;
        model.upsert(std::move(r));
    }

    MessageComposer c;
    c.push(MessageType::GET_OBSERVED_SYMBOLS);
    t.feed(frame(c));

    host::IpcClient client(t, model);
    client.poll();

    ASSERT_EQ(t.sends.size(), 1u); // sent a GET_OBSERVED_SYMBOLS_RESPONSE

    // Decode reply: [MessageType][size_t count][string...]. MessageDecoder
    // only decodes off a live ITransport, so re-feed the captured send
    // through a fresh FakeTransport (mirrors
    // SendExportBufferRequestRoundTripsFields above).
    FakeTransport decode_t;
    decode_t.feed(t.sends[0]);
    MessageType reply_type{};
    MessageDecoder{decode_t}.read(reply_type);
    EXPECT_EQ(reply_type, MessageType::GET_OBSERVED_SYMBOLS_RESPONSE);

    std::size_t count = 0;
    MessageDecoder decoder{decode_t};
    decoder.read(count);
    std::vector<std::string> names;
    for (std::size_t i = 0; i < count; ++i) {
        std::string name;
        decoder.read(name);
        names.push_back(name);
    }

    EXPECT_EQ(names.size(), 1u);
    EXPECT_EQ(names.front(), "debugger_var");
    EXPECT_TRUE(std::ranges::find(names, "local_file.png") == names.end());
}

TEST(IpcClient, NotifyRemovedSurvivesDeadTransport) {
    ThrowingTransport t;
    host::IpcBufferModel model;
    host::IpcClient client(t, model);

    EXPECT_NO_THROW(client.notify_removed("some_buffer"));
}

TEST(IpcClient, RequestPlotSurvivesDeadTransport) {
    ThrowingTransport t;
    host::IpcBufferModel model;
    host::IpcClient client(t, model);

    EXPECT_NO_THROW(client.request_plot("some_buffer"));
}

TEST(IpcClient, RequestPlotSends) {
    FakeTransport t;
    host::IpcBufferModel model;
    host::IpcClient client(t, model);
    client.request_plot("myVar");

    ASSERT_EQ(t.sends.size(), 1u);
    MessageType h{};
    std::memcpy(&h, t.sends[0].data(), sizeof(h));
    EXPECT_EQ(h, MessageType::PLOT_BUFFER_REQUEST);
}

TEST(IpcClient, ChunkedRoundTripReassemblesBuffer) {
    FakeTransport t;
    host::IpcBufferModel model;
    // 4x2 rgb8, unpadded: stride counts PIXELS per row, so it equals width
    // here, and the payload is channels * stride * height = 24 bytes.
    std::vector bytes(24, std::byte{9});

    {
        const std::size_t total = bytes.size();
        MessageComposer c;
        c.push(MessageType::PLOT_BUFFER_BEGIN)
            .push(std::string("v"))
            .push(std::string("disp"))
            .push(std::string("rgb"))
            .push(false)
            .push(4)
            .push(2)
            .push(3)
            .push(4)
            .push(static_cast<int>(BufferType::UNSIGNED_BYTE))
            .push(total);
        t.feed(frame(c));
    }
    {
        MessageComposer c;
        c.push(MessageType::PLOT_BUFFER_CHUNK)
            .push(std::string("v"))
            .push(std::size_t{0})
            .push(std::size_t{2})
            .push(std::span<const std::byte>(bytes));
        t.feed(frame(c));
    }
    {
        MessageComposer c;
        c.push(MessageType::PLOT_BUFFER_END).push(std::string("v"));
        t.feed(frame(c));
    }

    host::IpcClient client(t, model);
    client.poll();

    ASSERT_EQ(model.size(), 1u);
    EXPECT_EQ(model.at(0).variable_name, "v");
    ASSERT_EQ(model.at(0).bytes.size(), 24u);
    for (auto b : model.at(0).bytes) {
        EXPECT_EQ(b, std::byte{9});
    }
}

// The reported failure mode: a huge stride paired with a tiny, but
// height-divisible, total_byte_size sails through the old height-only/
// divisibility checks. begin() must now refuse it outright, so the buffer
// never reaches the model even if well-formed chunks and an END follow.
TEST(IpcClient, BeginRejectsGeometryTooLargeForPayload) {
    FakeTransport t;
    host::IpcBufferModel model;
    // width=4, height=2, channels=1, stride=100000, FLOAT32: needs
    // (height-1)*stride+width = 100004 pixels, but total_byte_size=2 covers
    // zero (2 / type_size(FLOAT32) == 0).
    t.feed(begin_frame_ex("v", 4, 2, 1, 100000, BufferType::FLOAT32, 2));
    const std::vector bytes(2, std::byte{9}); // 1 byte/row, height-divisible
    t.feed(chunk_frame("v", 0, 2, bytes));
    t.feed(end_frame("v"));

    host::IpcClient client(t, model);
    {
        CerrCapture cap; // silence the expected diagnostic
        client.poll();
    }

    EXPECT_EQ(model.size(), 0u);
}

// begin() must reject non-positive width/channels and stride < width: none
// of these are checked by the pre-existing height/divisibility validation,
// so a peer could otherwise declare a shape the payload cannot back.
TEST(IpcClient, BeginRejectsNonPositiveWidthChannelsAndStrideBelowWidth) {
    // width <= 0.
    {
        FakeTransport t;
        host::IpcBufferModel model;
        t.feed(begin_frame_ex("v", 0, 2, 1, 1, BufferType::UNSIGNED_BYTE, 2));
        host::IpcClient client(t, model);
        CerrCapture cap;
        client.poll();
        EXPECT_NE(cap.out.str().find("rejected PLOT_BUFFER_BEGIN"),
                  std::string::npos);
    }
    // channels <= 0.
    {
        FakeTransport t;
        host::IpcBufferModel model;
        t.feed(begin_frame_ex("v", 4, 2, 0, 4, BufferType::UNSIGNED_BYTE, 8));
        host::IpcClient client(t, model);
        CerrCapture cap;
        client.poll();
        EXPECT_NE(cap.out.str().find("rejected PLOT_BUFFER_BEGIN"),
                  std::string::npos);
    }
    // stride < width.
    {
        FakeTransport t;
        host::IpcBufferModel model;
        t.feed(begin_frame_ex("v", 4, 2, 1, 2, BufferType::UNSIGNED_BYTE, 8));
        host::IpcClient client(t, model);
        CerrCapture cap;
        client.poll();
        EXPECT_NE(cap.out.str().find("rejected PLOT_BUFFER_BEGIN"),
                  std::string::npos);
    }
}

// A producer that trims the trailing stride padding off only the last row is
// rejected on the chunked path: chunk() spaces every strip by
// total_byte_size / height, so a non-uniform final row has nowhere to go.
// (The single-shot path still accepts this shape via geometry_fits_payload,
// unaffected by this test.) This exercises stride > width so the fully
// padded size (stride*height = 10) differs from the trimmed lower bound
// ((height-1)*stride+width = 8) that used to be let through.
TEST(IpcClient, LastRowOmittingTrailingStridePaddingIsRejectedOnChunkedPath) {
    FakeTransport t;
    host::IpcBufferModel model;
    // width=3, height=2, channels=1, stride=5 (pixels/row): trimmed lower
    // bound is 8 bytes, the exact padded size begin() now requires is 10.
    constexpr std::size_t trimmed_total = 8;
    t.feed(begin_frame_ex(
        "v", 3, 2, 1, 5, BufferType::UNSIGNED_BYTE, trimmed_total));
    const std::vector bytes(trimmed_total, std::byte{7});
    t.feed(chunk_frame("v", 0, 2, bytes));
    t.feed(end_frame("v"));

    host::IpcClient client(t, model);
    {
        CerrCapture cap; // silence the expected diagnostic
        client.poll();
    }

    EXPECT_EQ(model.size(), 0u);
}

// Pins the exact contradiction the design review found: a geometry that
// geometry_fits_payload() (the single-shot floor) accepts outright is still
// refused on the chunked path, because chunk() needs every row spaced by a
// single uniform size and this payload only has one if the trimmed last row
// is ignored.
TEST(IpcClient, BeginRejectsFloorAcceptedGeometryThatIsNotFullyPadded) {
    // width=4, height=3, channels=2, stride=6, FLOAT32: floor =
    // ((3-1)*6+4)*2*4 = 128 bytes; the fully padded size begin() now
    // requires is stride*height*channels*type_size = 6*3*2*4 = 144.
    constexpr std::size_t floor_total = 128;
    ASSERT_TRUE(
        geometry_fits_payload(4, 3, 2, 6, BufferType::FLOAT32, floor_total));

    FakeTransport t;
    host::IpcBufferModel model;
    t.feed(begin_frame_ex("v", 4, 3, 2, 6, BufferType::FLOAT32, floor_total));
    const std::vector bytes(floor_total, std::byte{5});
    t.feed(chunk_frame("v", 0, 3, bytes));
    t.feed(end_frame("v"));

    host::IpcClient client(t, model);
    CerrCapture cap;
    client.poll();

    EXPECT_EQ(model.size(), 0u);
    // The empty model alone proves nothing here: 128 / 3 truncates to 42
    // bytes per row, so even a wrongly accepted BEGIN would refuse the
    // 128-byte chunk and leave the model empty. The diagnostic is what
    // discriminates -- it names BEGIN only when begin() did the refusing,
    // and CHUNK otherwise.
    const std::string logged = cap.out.str();
    EXPECT_NE(logged.find("rejected PLOT_BUFFER_BEGIN"), std::string::npos);
    // Naming both sizes is what makes this failure self-diagnosing for a
    // producer that computes the total from the trimmed lower bound.
    EXPECT_NE(logged.find("needs 144 bytes"), std::string::npos);
    EXPECT_NE(logged.find("got 128"), std::string::npos);
}

// An unknown type would fall through type_size()'s default and be measured
// as one byte per element, so the size arithmetic would use the wrong
// element width without anything noticing.
TEST(IpcClient, BeginRejectsUnknownBufferType) {
    FakeTransport t;
    host::IpcBufferModel model;
    // 1 is deliberately absent from BufferType; the enum skips it.
    constexpr int unknown_type = 1;
    MessageComposer c;
    c.push(MessageType::PLOT_BUFFER_BEGIN)
        .push(std::string("v"))
        .push(std::string("disp"))
        .push(std::string("rgb"))
        .push(false)
        .push(4)
        .push(2)
        .push(1)
        .push(4)
        .push(unknown_type)
        .push(std::size_t{8});
    t.feed(frame(c));

    host::IpcClient client(t, model);
    CerrCapture cap;
    client.poll();

    EXPECT_EQ(model.size(), 0u);
    // 8 bytes is exactly the padded size at one byte per element, so only
    // the type check can be doing the refusing here.
    EXPECT_NE(cap.out.str().find("unknown buffer type 1"), std::string::npos);
}

TEST(IpcClient, PlotBufferContentsRejectsUnknownBufferType) {
    FakeTransport t;
    host::IpcBufferModel model;
    const std::vector bytes(8, std::byte{5});
    MessageComposer c;
    c.push(MessageType::PLOT_BUFFER_CONTENTS)
        .push(std::string("v"))
        .push(std::string("disp"))
        .push(std::string("rgb"))
        .push(false)
        .push(4)
        .push(2)
        .push(1)
        .push(4)
        .push(1) // absent from BufferType
        .push(std::span<const std::byte>(bytes));
    t.feed(frame(c));

    host::IpcClient client(t, model);
    CerrCapture cap;
    client.poll();

    EXPECT_EQ(model.size(), 0u);
    EXPECT_NE(cap.out.str().find("unknown buffer type 1"), std::string::npos);
}

// The declared size drives an allocation, so it is capped before allocating
// rather than left for the allocator to refuse. Nothing is allocated here:
// the BEGIN only carries the number.
TEST(IpcClient, BeginRejectsDeclarationAboveTheByteLimit) {
    FakeTransport t;
    host::IpcBufferModel model;
    // stride 2^20 x height 2^15 x 1 channel x 1 byte = 32 GiB, which is
    // exactly its own padded size -- so the cap, not the geometry, refuses.
    constexpr int stride = 1 << 20;
    constexpr int height = 1 << 15;
    constexpr std::size_t declared =
        static_cast<std::size_t>(stride) * static_cast<std::size_t>(height);
    ASSERT_GT(declared, MAX_BUFFER_BYTES);
    t.feed(begin_frame_ex(
        "v", stride, height, 1, stride, BufferType::UNSIGNED_BYTE, declared));

    host::IpcClient client(t, model);
    CerrCapture cap;
    client.poll();

    EXPECT_EQ(model.size(), 0u);
    EXPECT_NE(cap.out.str().find("exceeds the"), std::string::npos);
}

// padded_payload_size() reports nullopt both for a shape that makes no sense
// and for one whose byte count will not fit, and those need different
// messages: this geometry is entirely well-formed, just too big to measure.
TEST(IpcClient, BeginDistinguishesAnOversizedGeometryFromAnInvalidOne) {
    FakeTransport t;
    host::IpcBufferModel model;
    constexpr int huge = std::numeric_limits<int>::max();
    t.feed(begin_frame_ex(
        "v", huge, huge, 4, huge, BufferType::FLOAT64, std::size_t{8}));

    host::IpcClient client(t, model);
    CerrCapture cap;
    client.poll();

    EXPECT_EQ(model.size(), 0u);
    const std::string logged = cap.out.str();
    EXPECT_NE(logged.find("too large to size"), std::string::npos);
    EXPECT_EQ(logged.find("not renderable"), std::string::npos);
}

// The other rejection reason: a shape that has no size at all, rather than
// one whose size disagrees with the payload.
TEST(IpcClient, BeginReportsUnrenderableGeometryDistinctly) {
    FakeTransport t;
    host::IpcBufferModel model;
    // stride < width: no padded size exists, so there is no byte count to
    // quote back.
    t.feed(begin_frame_ex("v", 8, 2, 1, 4, BufferType::UNSIGNED_BYTE, 16));

    host::IpcClient client(t, model);
    CerrCapture cap;
    client.poll();

    EXPECT_EQ(model.size(), 0u);
    const std::string logged = cap.out.str();
    EXPECT_NE(logged.find("not renderable"), std::string::npos);
    EXPECT_EQ(logged.find("needs"), std::string::npos);
}

TEST(IpcClient, BeginRejectsPayloadOneByteAboveOrBelowPaddedSize) {
    // width=4, height=1, channels=2, stride=6, FLOAT32: padded size is
    // stride*height*channels*type_size = 6*1*2*4 = 48. height=1 is
    // deliberate: with height>1 an off-by-one payload is already caught by
    // the divisibility the exact size implies, so only height=1 leaves the
    // exact comparison itself as the thing under test.
    for (const std::size_t total : {std::size_t{47}, std::size_t{49}}) {
        FakeTransport t;
        host::IpcBufferModel model;
        t.feed(begin_frame_ex("v", 4, 1, 2, 6, BufferType::FLOAT32, total));
        // Chunk sized exactly as the old code would have computed
        // bytes-per-row (total / height), followed by END: a full,
        // internally-consistent round trip, so a wrongly-accepted begin()
        // would actually complete the transfer instead of silently stalling
        // on a missing END.
        const std::vector bytes(total, std::byte{9});
        t.feed(chunk_frame("v", 0, 1, bytes));
        t.feed(end_frame("v"));

        host::IpcClient client(t, model);
        CerrCapture cap; // silence the expected diagnostic
        client.poll();

        EXPECT_EQ(model.size(), 0u);
    }
}

TEST(IpcClient, BeginAcceptsExactlyPaddedSizeAndRoundTripsIntoModel) {
    // Same geometry as the contradiction test above, but with the fully
    // padded size (144) rather than the floor (128).
    constexpr int height = 3;
    constexpr std::size_t total = 144;
    FakeTransport t;
    host::IpcBufferModel model;
    t.feed(begin_frame_ex("v", 4, height, 2, 6, BufferType::FLOAT32, total));
    const std::vector bytes(total, std::byte{3});
    t.feed(chunk_frame("v", 0, static_cast<std::size_t>(height), bytes));
    t.feed(end_frame("v"));

    host::IpcClient client(t, model);
    client.poll();

    ASSERT_EQ(model.size(), 1u);
    EXPECT_EQ(model.at(0).variable_name, "v");
    EXPECT_EQ(model.at(0).bytes.size(), total);
}

TEST(IpcClient, RejectedChunkAbortsTransferSoAWellFormedRetryCannotResumeIt) {
    FakeTransport t;
    host::IpcBufferModel model;
    std::vector bytes(8, std::byte{1}); // 2 rows x 4 bytes/row

    {
        MessageComposer c;
        c.push(MessageType::PLOT_BUFFER_BEGIN)
            .push(std::string("v"))
            .push(std::string("disp"))
            .push(std::string("rgb"))
            .push(false)
            .push(4)
            .push(2)
            .push(1)
            .push(4)
            .push(static_cast<int>(BufferType::UNSIGNED_BYTE))
            .push(bytes.size());
        t.feed(frame(c));
    }
    {
        // row_offset 5 is out of range for height 2: chunk() rejects it,
        // which must abort the transfer outright.
        MessageComposer c;
        c.push(MessageType::PLOT_BUFFER_CHUNK)
            .push(std::string("v"))
            .push(std::size_t{5})
            .push(std::size_t{1})
            .push(std::span<const std::byte>(bytes));
        t.feed(frame(c));
    }
    {
        // A well-formed chunk covering every row, sent right after: without
        // abort() this alone would complete the transfer, masking the
        // earlier rejection instead of surfacing it.
        MessageComposer c;
        c.push(MessageType::PLOT_BUFFER_CHUNK)
            .push(std::string("v"))
            .push(std::size_t{0})
            .push(std::size_t{2})
            .push(std::span<const std::byte>(bytes));
        t.feed(frame(c));
    }
    {
        MessageComposer c;
        c.push(MessageType::PLOT_BUFFER_END).push(std::string("v"));
        t.feed(frame(c));
    }

    host::IpcClient client(t, model);
    {
        CerrCapture cap; // silence the expected diagnostic in test output
        client.poll();
    }

    // Aborted transfer: even the later well-formed retry can't resume it.
    EXPECT_EQ(model.size(), 0u);
}

TEST(IpcClient, RepeatedChunkFailuresForOneTransferReportOnlyOnce) {
    FakeTransport t;
    host::IpcBufferModel model;
    std::vector bytes(8, std::byte{1});

    {
        MessageComposer c;
        c.push(MessageType::PLOT_BUFFER_BEGIN)
            .push(std::string("v"))
            .push(std::string("disp"))
            .push(std::string("rgb"))
            .push(false)
            .push(4)
            .push(2)
            .push(1)
            .push(4)
            .push(static_cast<int>(BufferType::UNSIGNED_BYTE))
            .push(bytes.size());
        t.feed(frame(c));
    }
    // First chunk aborts the transfer; every one after fails too (unknown
    // name) -- a naive implementation would log all of these.
    constexpr int kBadChunks = 200;
    for (int i = 0; i < kBadChunks; ++i) {
        MessageComposer c;
        c.push(MessageType::PLOT_BUFFER_CHUNK)
            .push(std::string("v"))
            .push(std::size_t{5})
            .push(std::size_t{1})
            .push(std::span<const std::byte>(bytes));
        t.feed(frame(c));
    }

    host::IpcClient client(t, model);
    CerrCapture cap;
    client.poll();

    const std::string logged = cap.out.str();
    EXPECT_EQ(std::ranges::count(logged, '\n'), 1);
    EXPECT_NE(logged.find("PLOT_BUFFER_CHUNK"), std::string::npos);
    EXPECT_NE(logged.find("'v'"), std::string::npos);
}

TEST(IpcClient, StrayChunksForNeverBegunNamesAreSilent) {
    FakeTransport t;
    host::IpcBufferModel model;
    const std::vector bytes(4, std::byte{1});
    t.feed(chunk_frame("a", 0, 1, bytes));
    t.feed(chunk_frame("b", 0, 1, bytes));
    t.feed(chunk_frame("c", 0, 1, bytes));

    host::IpcClient client(t, model);
    CerrCapture cap;
    client.poll();

    EXPECT_TRUE(cap.out.str().empty());
}

TEST(IpcClient, LaterTransferForSameNameStillReportsAfterEarlierRefusal) {
    FakeTransport t;
    host::IpcBufferModel model;

    // First attempt: invalid geometry refuses the BEGIN outright.
    t.feed(begin_frame("v", 4, 0, 0)); // height <= 0 -> rejected

    // Second attempt: a fresh, valid transfer under the same name that is
    // then refused too (wrong-sized chunk). The old code tracked "already
    // reported" per name with no bound on how long that lasts, so a second,
    // unrelated failure for "v" could go unreported forever; it must not.
    t.feed(begin_frame("v", 4, 2, 8));
    const std::vector wrong_size_bytes(3, std::byte{1});
    t.feed(chunk_frame("v", 0, 1, wrong_size_bytes));

    host::IpcClient client(t, model);
    CerrCapture cap;
    client.poll();

    const std::string logged = cap.out.str();
    EXPECT_NE(logged.find("rejected PLOT_BUFFER_BEGIN"), std::string::npos);
    EXPECT_NE(logged.find("rejected PLOT_BUFFER_CHUNK"), std::string::npos);
}

// Sharper version of the above: no intervening successful begin() at all, so
// the only way a second BEGIN failure for "v" can be reported is if rejected
// BEGINs are never gated by "already reported" in the first place. This is
// the precise regression test for the suppression half of the bug: the old
// per-name "already reported" set was cleared only by a successful begin()
// or by end() running for that name, so a peer that only ever sends invalid
// BEGINs for "v" got exactly one diagnostic, forever, no matter how many
// distinct malformed attempts followed.
TEST(IpcClient, RepeatedInvalidBeginsForSameNameEachReport) {
    FakeTransport t;
    host::IpcBufferModel model;
    t.feed(begin_frame("v", 4, 0, 0)); // height <= 0 -> rejected
    t.feed(begin_frame("v", 4, 0, 0)); // rejected again, unrelated attempt

    host::IpcClient client(t, model);
    CerrCapture cap;
    client.poll();

    EXPECT_EQ(count_occurrences(cap.out.str(), "rejected PLOT_BUFFER_BEGIN"),
              2u);
}

TEST(IpcClient, StrayEndIsSilentButRealIncompleteTransferStillLogs) {
    // A PLOT_BUFFER_END for a name with no transfer in flight is a stray
    // (e.g. the tail of an already-reported refusal) and must be silent.
    {
        FakeTransport t;
        host::IpcBufferModel model;
        t.feed(end_frame("v"));

        host::IpcClient client(t, model);
        CerrCapture cap;
        client.poll();

        EXPECT_TRUE(cap.out.str().empty());
    }
    // A valid BEGIN whose rows are never fully covered before END is a
    // genuine incomplete transfer and must still be reported.
    {
        FakeTransport t;
        host::IpcBufferModel model;
        t.feed(begin_frame("v", 4, 2, 8));
        const std::vector bytes(4, std::byte{9});
        t.feed(chunk_frame("v", 0, 1, bytes)); // only row 0 of 2 covered
        t.feed(end_frame("v"));

        host::IpcClient client(t, model);
        CerrCapture cap;
        client.poll();

        EXPECT_NE(cap.out.str().find("incomplete transfer"), std::string::npos);
    }
}

TEST(IpcClient, PollDoesNotThrowOnTruncatedMessage) {
    FakeTransport t;
    host::IpcBufferModel model;
    std::vector bytes(12, std::byte{5});
    MessageComposer c;
    c.push(MessageType::PLOT_BUFFER_CONTENTS)
        .push(std::string("v"))
        .push(std::string("disp"))
        .push(std::string("rgb"))
        .push(false)
        .push(2)
        .push(2)
        .push(3)
        .push(6)
        .push(BufferType::UNSIGNED_BYTE)
        .push(std::span<const std::byte>(bytes));
    const auto full_frame = frame(c);

    // Header is present but the body is truncated: FakeTransport::receive()
    // returns 0 once the queued bytes run out, which makes
    // MessageDecoder::read_impl throw. poll() must catch that throw instead
    // of letting it escape and terminate the process.
    ASSERT_GT(full_frame.size(), 8u);
    t.feed(std::vector(full_frame.begin(), full_frame.begin() + 8));

    host::IpcClient client(t, model);
    EXPECT_NO_THROW(client.poll());

    EXPECT_EQ(model.size(), 0u); // partial message dropped, not half-applied
}

TEST(IpcClient, RestoresAvailableUnexpiredBuffersOnSetAvailableSymbols) {
    FakeTransport t;
    host::IpcBufferModel model;
    host::IpcClient client(t, model);
    constexpr std::int64_t future = 4102444800; // year 2100
    constexpr std::int64_t past = 1;
    client.set_restore_buffers(
        {{"want", future}, {"expired", past}, {"absent", future}});
    MessageComposer c;
    std::deque<std::string> syms{"want", "expired", "other"};
    c.push(MessageType::SET_AVAILABLE_SYMBOLS).push(syms.size());
    for (const auto& s : syms) {
        c.push(s);
    }
    t.feed(frame(c));
    client.poll();
    // Exactly one PLOT_BUFFER_REQUEST, for "want" (available + unexpired +
    // unloaded).
    int plot_reqs = 0;
    std::string requested;
    for (auto& snd : t.sends) {
        MessageType h{};
        std::memcpy(&h, snd.data(), sizeof(h));
        if (h == MessageType::PLOT_BUFFER_REQUEST) {
            ++plot_reqs;
            // name follows the 4-byte header + size_t length
            std::size_t len = 0;
            std::memcpy(&len, snd.data() + sizeof(h), sizeof(len));
            requested.assign(reinterpret_cast<const char*>(
                                 snd.data() + sizeof(h) + sizeof(len)),
                             len);
        }
    }
    EXPECT_EQ(plot_reqs,
              1); // "expired" skipped (past), "absent" skipped (not available)
    EXPECT_EQ(requested, "want");
}

TEST(IpcClient, RestoreRequestsOnlyOnceAcrossRepeatedSetAvailableSymbols) {
    FakeTransport t;
    host::IpcBufferModel model;
    host::IpcClient client(t, model);
    constexpr std::int64_t future = 4102444800; // year 2100
    client.set_restore_buffers({{"want", future}});

    MessageComposer c;
    std::deque<std::string> syms{"want"};
    c.push(MessageType::SET_AVAILABLE_SYMBOLS).push(syms.size());
    for (const auto& s : syms) {
        c.push(s);
    }
    const auto f = frame(c);
    t.feed(f);
    client.poll();
    t.feed(f); // same SET_AVAILABLE_SYMBOLS frame fed a second time
    client.poll();

    int plot_reqs = 0;
    for (auto& snd : t.sends) {
        MessageType h{};
        std::memcpy(&h, snd.data(), sizeof(h));
        if (h == MessageType::PLOT_BUFFER_REQUEST) {
            ++plot_reqs;
        }
    }
    EXPECT_EQ(plot_reqs, 1); // already requested_, second round is a no-op
}

TEST(IpcClient, ApplySessionStateInvokesCallbackWithJsonVerbatim) {
    FakeTransport t;
    host::IpcBufferModel model;
    host::IpcClient client(t, model);
    std::string received;
    int calls = 0;
    client.set_session_state_callback(
        [&received, &calls](std::string_view json) {
            received = json;
            ++calls;
        });

    const std::string json = R"({"window":{"w":800,"h":600}})";
    MessageComposer c;
    c.push(MessageType::APPLY_SESSION_STATE).push(json);
    t.feed(frame(c));
    client.poll();

    EXPECT_EQ(calls, 1);
    EXPECT_EQ(received, json);
}

TEST(IpcClient, ApplySessionStateWithoutCallbackDoesNotThrow) {
    FakeTransport t;
    oid::host::IpcBufferModel model;
    oid::host::IpcClient client(t, model);
    // No callback registered; message must still be fully consumed (not
    // left half-read desyncing the stream) and must not throw.
    MessageComposer c;
    c.push(MessageType::APPLY_SESSION_STATE).push(std::string("{}"));
    t.feed(frame(c));
    EXPECT_NO_THROW(client.poll());
}

TEST(IpcClient, ExportSelectedBufferInvokesCallback) {
    FakeTransport t;
    host::IpcBufferModel model;
    host::IpcClient client(t, model);
    int calls = 0;
    client.set_export_selected_callback([&calls] { ++calls; });

    MessageComposer c;
    c.push(MessageType::EXPORT_SELECTED_BUFFER);
    t.feed(frame(c));
    client.poll();

    EXPECT_EQ(calls, 1);
}

TEST(IpcClient, ExportSelectedBufferWithoutCallbackDoesNotThrow) {
    FakeTransport t;
    host::IpcBufferModel model;
    host::IpcClient client(t, model);
    MessageComposer c;
    c.push(MessageType::EXPORT_SELECTED_BUFFER);
    t.feed(frame(c));
    EXPECT_NO_THROW(client.poll());
}

TEST(IpcClient, SendSessionStateChangedSendsTypeAndJsonVerbatim) {
    FakeTransport t;
    host::IpcBufferModel model;
    host::IpcClient client(t, model);

    client.send_session_state_changed(R"({"a":1})");

    ASSERT_EQ(t.sends.size(), 1u);
    MessageType h{};
    std::memcpy(&h, t.sends[0].data(), sizeof(h));
    EXPECT_EQ(h, MessageType::SESSION_STATE_CHANGED);

    std::size_t len = 0;
    std::memcpy(&len, t.sends[0].data() + sizeof(h), sizeof(len));
    std::string json(reinterpret_cast<const char*>(t.sends[0].data() +
                                                   sizeof(h) + sizeof(len)),
                     len);
    EXPECT_EQ(json, R"({"a":1})");
}

TEST(IpcClient, SendExportBufferRequestRoundTripsFields) {
    FakeTransport t;
    host::IpcBufferModel model;
    host::IpcClient client(t, model);

    client.send_export_buffer_request("myVar", 2, {0.1f, 0.2f, 0.3f});

    ASSERT_EQ(t.sends.size(), 1u);

    FakeTransport decode_t;
    decode_t.feed(t.sends[0]);
    MessageType h{};
    MessageDecoder{decode_t}.read(h);
    EXPECT_EQ(h, MessageType::EXPORT_BUFFER_REQUEST);

    std::string name;
    int format{};
    MessageDecoder decoder{decode_t};
    decoder.read(name).read(format);
    std::array<float, 8> contrast{};
    for (auto& v : contrast) {
        decoder.read(v);
    }

    EXPECT_EQ(name, "myVar");
    EXPECT_EQ(format, 2);
    EXPECT_FLOAT_EQ(contrast[0], 0.1f);
    EXPECT_FLOAT_EQ(contrast[1], 0.2f);
    EXPECT_FLOAT_EQ(contrast[2], 0.3f);
    for (std::size_t i = 3; i < contrast.size(); ++i) {
        EXPECT_FLOAT_EQ(contrast[i], 0.0f); // padded, mirrors Qt's sender
    }
}

TEST(IpcClient, UnknownMessageTypeStillIgnored) {
    FakeTransport t;
    host::IpcBufferModel model;
    host::IpcClient client(t, model);
    MessageComposer c;
    c.push(static_cast<MessageType>(9999));
    t.feed(frame(c));

    EXPECT_NO_THROW(client.poll());
    EXPECT_EQ(model.size(), 0u);
}

TEST(IpcClient, RestoreSkipsBufferAlreadyInModel) {
    FakeTransport t;
    host::IpcBufferModel model;
    {
        host::BufferRecord r;
        r.variable_name = "want";
        model.upsert(std::move(r));
    }
    host::IpcClient client(t, model);
    constexpr std::int64_t future = 4102444800; // year 2100
    client.set_restore_buffers({{"want", future}});

    MessageComposer c;
    std::deque<std::string> syms{"want"};
    c.push(MessageType::SET_AVAILABLE_SYMBOLS).push(syms.size());
    for (const auto& s : syms) {
        c.push(s);
    }
    t.feed(frame(c));
    client.poll();

    int plot_reqs = 0;
    for (auto& snd : t.sends) {
        MessageType h{};
        std::memcpy(&h, snd.data(), sizeof(h));
        if (h == MessageType::PLOT_BUFFER_REQUEST) {
            ++plot_reqs;
        }
    }
    EXPECT_EQ(plot_reqs, 0); // "want" already in model, restore skips it
}
