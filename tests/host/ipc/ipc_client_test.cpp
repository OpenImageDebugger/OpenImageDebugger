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

#include <array>
#include <cstring>
#include <deque>
#include <stdexcept>
#include <string_view>

#include <gtest/gtest.h>

namespace oid::host {

TEST(BufferDecode, MapsFieldsAndStepEqualsStride) {
    std::vector<std::byte> bytes(12, std::byte{7});
    BufferRecord r = make_buffer_record({.variable_name = "v",
                                         .display_name = "disp",
                                         .pixel_layout = "rgb",
                                         .transpose = false,
                                         .width = 2,
                                         .height = 2,
                                         .channels = 3,
                                         .stride = 6,
                                         .type = oid::BufferType::UNSIGNED_BYTE,
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
    const double value = 3.5;
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
                                         .type = oid::BufferType::FLOAT64,
                                         .bytes = std::move(bytes)});

    // FLOAT64 payload of 1 double must convert down to 1 float (4 bytes).
    ASSERT_EQ(r.bytes.size(), sizeof(float));
    // BufferRecord::type stays FLOAT64, unchanged, matching the Qt path.
    EXPECT_EQ(r.type, oid::BufferType::FLOAT64);

    float decoded = 0.0F;
    std::memcpy(&decoded, r.bytes.data(), sizeof(float));
    EXPECT_FLOAT_EQ(decoded, static_cast<float>(value));
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
    void send(std::span<const std::byte>) override {
        throw std::runtime_error("transport is dead");
    }

    std::size_t receive(std::span<std::byte>) override {
        return 0;
    }

    bool has_data() const override {
        return false;
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

TEST(IpcClient, PlotBufferContentsUpsertsModel) {
    FakeTransport t;
    oid::host::IpcBufferModel model;
    MessageComposer c;
    std::vector<std::byte> bytes(12, std::byte{5});
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
    t.feed(frame(c));

    oid::host::IpcClient client(t, model);
    client.poll();

    ASSERT_EQ(model.size(), 1u);
    EXPECT_EQ(model.at(0).variable_name, "v");
    EXPECT_EQ(model.at(0).step, 6);
    EXPECT_EQ(model.at(0).bytes.size(), 12u);
}

TEST(IpcClient, SetAvailableSymbolsPopulatesList) {
    FakeTransport t;
    oid::host::IpcBufferModel model;
    MessageComposer c;
    std::deque<std::string> syms{"a", "b"};
    c.push(MessageType::SET_AVAILABLE_SYMBOLS).push(syms.size());
    for (const auto& s : syms) {
        c.push(s);
    }
    t.feed(frame(c));

    oid::host::IpcClient client(t, model);
    client.poll();

    ASSERT_EQ(client.available_symbols().size(), 2u);
    EXPECT_EQ(client.available_symbols()[0], "a");
}

TEST(IpcClient, GetObservedSymbolsRespondsWithModelNames) {
    FakeTransport t;
    oid::host::IpcBufferModel model;
    {
        oid::host::BufferRecord r;
        r.variable_name = "x";
        model.upsert(std::move(r));
    }
    MessageComposer c;
    c.push(MessageType::GET_OBSERVED_SYMBOLS);
    t.feed(frame(c));

    oid::host::IpcClient client(t, model);
    client.poll();

    ASSERT_EQ(t.sends.size(), 1u); // sent a GET_OBSERVED_SYMBOLS_RESPONSE
    MessageType h{};
    std::memcpy(&h, t.sends[0].data(), sizeof(h));
    EXPECT_EQ(h, MessageType::GET_OBSERVED_SYMBOLS_RESPONSE);
}

TEST(IpcClient, NotifyRemovedSurvivesDeadTransport) {
    ThrowingTransport t;
    oid::host::IpcBufferModel model;
    oid::host::IpcClient client(t, model);

    EXPECT_NO_THROW(client.notify_removed("some_buffer"));
}

TEST(IpcClient, RequestPlotSurvivesDeadTransport) {
    ThrowingTransport t;
    oid::host::IpcBufferModel model;
    oid::host::IpcClient client(t, model);

    EXPECT_NO_THROW(client.request_plot("some_buffer"));
}

TEST(IpcClient, RequestPlotSends) {
    FakeTransport t;
    oid::host::IpcBufferModel model;
    oid::host::IpcClient client(t, model);
    client.request_plot("myVar");

    ASSERT_EQ(t.sends.size(), 1u);
    MessageType h{};
    std::memcpy(&h, t.sends[0].data(), sizeof(h));
    EXPECT_EQ(h, MessageType::PLOT_BUFFER_REQUEST);
}

TEST(IpcClient, ChunkedRoundTripReassemblesBuffer) {
    FakeTransport t;
    oid::host::IpcBufferModel model;
    std::vector<std::byte> bytes(24, std::byte{9});
    const std::size_t total = bytes.size();

    {
        MessageComposer c;
        c.push(MessageType::PLOT_BUFFER_BEGIN)
            .push(std::string("v"))
            .push(std::string("disp"))
            .push(std::string("rgb"))
            .push(false)
            .push(4)
            .push(2)
            .push(3)
            .push(12)
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

    oid::host::IpcClient client(t, model);
    client.poll();

    ASSERT_EQ(model.size(), 1u);
    EXPECT_EQ(model.at(0).variable_name, "v");
    ASSERT_EQ(model.at(0).bytes.size(), 24u);
    for (auto b : model.at(0).bytes) {
        EXPECT_EQ(b, std::byte{9});
    }
}

TEST(IpcClient, PollDoesNotThrowOnTruncatedMessage) {
    FakeTransport t;
    oid::host::IpcBufferModel model;
    std::vector<std::byte> bytes(12, std::byte{5});
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
    t.feed(std::vector<std::byte>(full_frame.begin(), full_frame.begin() + 8));

    oid::host::IpcClient client(t, model);
    EXPECT_NO_THROW(client.poll());

    EXPECT_EQ(model.size(), 0u); // partial message dropped, not half-applied
}

TEST(IpcClient, RestoresAvailableUnexpiredBuffersOnSetAvailableSymbols) {
    FakeTransport t;
    oid::host::IpcBufferModel model;
    oid::host::IpcClient client(t, model);
    const std::int64_t future = 4102444800; // year 2100
    const std::int64_t past = 1;
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
    oid::host::IpcBufferModel model;
    oid::host::IpcClient client(t, model);
    const std::int64_t future = 4102444800; // year 2100
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
    oid::host::IpcBufferModel model;
    oid::host::IpcClient client(t, model);
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
    oid::host::IpcBufferModel model;
    oid::host::IpcClient client(t, model);
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
    oid::host::IpcBufferModel model;
    oid::host::IpcClient client(t, model);
    MessageComposer c;
    c.push(MessageType::EXPORT_SELECTED_BUFFER);
    t.feed(frame(c));
    EXPECT_NO_THROW(client.poll());
}

TEST(IpcClient, SendSessionStateChangedSendsTypeAndJsonVerbatim) {
    FakeTransport t;
    oid::host::IpcBufferModel model;
    oid::host::IpcClient client(t, model);

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
    oid::host::IpcBufferModel model;
    oid::host::IpcClient client(t, model);

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
    oid::host::IpcBufferModel model;
    oid::host::IpcClient client(t, model);
    MessageComposer c;
    c.push(static_cast<MessageType>(9999));
    t.feed(frame(c));

    EXPECT_NO_THROW(client.poll());
    EXPECT_EQ(model.size(), 0u);
}

TEST(IpcClient, RestoreSkipsBufferAlreadyInModel) {
    FakeTransport t;
    oid::host::IpcBufferModel model;
    {
        oid::host::BufferRecord r;
        r.variable_name = "want";
        model.upsert(std::move(r));
    }
    oid::host::IpcClient client(t, model);
    const std::int64_t future = 4102444800; // year 2100
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
