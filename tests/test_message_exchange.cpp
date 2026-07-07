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

#include <array>
#include <bit>
#include <chrono>
#include <cstring>
#include <gtest/gtest.h>
#include <optional>
#include <span>
#include <string_view>
#include <thread>
#include <vector>

#include "ipc/asio_transport.h"
#include "ipc/message_exchange.h"
#include "ipc/transport.h"

using namespace oid;

struct RecordingTransport final : ITransport {
    std::vector<std::vector<std::byte>> sends;
    void send(std::span<const std::byte> data) override {
        sends.emplace_back(data.begin(), data.end());
    }
    std::size_t receive(std::span<std::byte>) override {
        return 0;
    }
    bool has_data() const override {
        return false;
    }
};

namespace {
constexpr int TEST_VALUE_42 = 42;
constexpr int TEST_VALUE_100 = 100;
constexpr int TEST_VALUE_12345 = 12345;
constexpr int TEST_VALUE_10 = 10;
constexpr int TEST_VALUE_20 = 20;
constexpr unsigned char MAX_UCHAR = 255;

// Test string constants
constexpr const char* TEST_STRING_HELLO = "Hello, World!";
constexpr const char* TEST_STRING_TEST = "Test String";
constexpr const char* TEST_STRING_ROUND_TRIP = "Round Trip Test";
constexpr const char* TEST_STRING_BUFFER = "test_buffer";
constexpr const char* TEST_STRING_ONE = "one";
constexpr const char* TEST_STRING_TWO = "two";
constexpr const char* TEST_STRING_THREE = "three";

template <typename T> std::span<const std::byte> as_bytes_span(const T& value) {
    return std::as_bytes(std::span{value});
}
} // namespace

class MessageExchangeTest : public ::testing::Test {
  protected:
    // Spins up a loopback Asio acceptor/connector pair on a background
    // thread. acceptor_ is declared before client_transport_/
    // server_transport_ so it outlives them (member destruction runs in
    // reverse declaration order): AsioTransport's server ctor references
    // the acceptor's io_context without owning it.
    void ConnectSockets() {
        acceptor_.emplace();
        std::jthread server_thread([this] {
            server_transport_.emplace(
                acceptor_->accept(std::chrono::seconds{5}));
        });
        client_transport_.emplace("127.0.0.1", acceptor_->port());
        server_thread.join();
        ASSERT_TRUE(client_transport_->is_connected());
        ASSERT_TRUE(server_transport_.has_value());
    }

    std::optional<AsioAcceptor> acceptor_;
    std::optional<AsioTransport> client_transport_;
    std::optional<AsioTransport> server_transport_;
};

TEST_F(MessageExchangeTest, PrimitiveBlockSize) {
    PrimitiveBlock block(TEST_VALUE_42);
    EXPECT_EQ(block.size(), sizeof(int));
}

TEST_F(MessageExchangeTest, PrimitiveBlockData) {
    PrimitiveBlock block(TEST_VALUE_42);
    int result = 0;
    std::memcpy(&result, block.data(), sizeof(int));
    EXPECT_EQ(result, TEST_VALUE_42);
}

TEST_F(MessageExchangeTest, StringBlockSize) {
    const auto test_string = std::string(TEST_STRING_HELLO);
    StringBlock block(test_string);
    EXPECT_EQ(block.size(), test_string.size());
}

TEST_F(MessageExchangeTest, StringBlockData) {
    const auto test_string = std::string(TEST_STRING_HELLO);
    StringBlock block(test_string);
    // Convert std::byte* to const char* for std::string (safe: same
    // size/alignment)
    const auto* data = std::bit_cast<const char*>(block.data());
    EXPECT_EQ(std::string(data, data + block.size()), test_string);
}

namespace {
constexpr uint8_t TEST_BUFFER_VALUES[] = {1, 2, 3, 4, 5};
constexpr std::size_t TEST_BUFFER_SIZE = sizeof(TEST_BUFFER_VALUES);
} // namespace

TEST_F(MessageExchangeTest, BufferBlockSize) {
    BufferBlock block(std::span<const std::byte>{
        reinterpret_cast<const std::byte*>(TEST_BUFFER_VALUES),
        TEST_BUFFER_SIZE});
    EXPECT_EQ(block.size(), TEST_BUFFER_SIZE);
}

TEST_F(MessageExchangeTest, BufferBlockData) {
    BufferBlock block(std::span<const std::byte>{
        reinterpret_cast<const std::byte*>(TEST_BUFFER_VALUES),
        TEST_BUFFER_SIZE});
    // Compare std::byte values directly (convert to uint8_t for comparison)
    const auto* data = block.data();
    for (auto i = 0U; i < TEST_BUFFER_SIZE; ++i) {
        EXPECT_EQ(static_cast<uint8_t>(data[i]), TEST_BUFFER_VALUES[i]);
    }
}

TEST_F(MessageExchangeTest, MessageComposerPushPrimitive) {
    MessageComposer composer;
    composer.push(TEST_VALUE_42)
        .push(MAX_UCHAR)
        .push(true)
        .push(TEST_VALUE_100);
    composer.clear();
}

TEST_F(MessageExchangeTest, MessageComposerSendCoalescesBlocks) {
    RecordingTransport transport;
    MessageComposer composer;
    composer.push(MessageType::PLOT_BUFFER_REQUEST)
        .push(std::string("TestField"));
    composer.send(transport);
    ASSERT_EQ(transport.sends.size(), 1u);
    ASSERT_GE(transport.sends[0].size(), 12u);
    MessageType type{};
    std::memcpy(&type, transport.sends[0].data(), sizeof(type));
    EXPECT_EQ(type, MessageType::PLOT_BUFFER_REQUEST);
}

TEST_F(MessageExchangeTest, WireFormatGoldenPlotBufferRequest) {
    RecordingTransport transport;
    MessageComposer composer;
    composer.push(MessageType::PLOT_BUFFER_REQUEST).push(std::string("abc"));
    composer.send(transport);
    ASSERT_EQ(transport.sends.size(), 1u);
    // MessageType(int=4, 4 bytes LE) | size_t length(3, 8 bytes LE) | "abc"
    const std::vector<std::byte> expected = {std::byte{0x04},
                                             std::byte{0x00},
                                             std::byte{0x00},
                                             std::byte{0x00},
                                             std::byte{0x03},
                                             std::byte{0x00},
                                             std::byte{0x00},
                                             std::byte{0x00},
                                             std::byte{0x00},
                                             std::byte{0x00},
                                             std::byte{0x00},
                                             std::byte{0x00},
                                             std::byte{'a'},
                                             std::byte{'b'},
                                             std::byte{'c'}};
    EXPECT_EQ(transport.sends[0], expected);
}

TEST_F(MessageExchangeTest, MessageComposerPushString) {
    MessageComposer composer;
    composer.push(std::string(TEST_STRING_TEST));
    composer.clear();
}

TEST_F(MessageExchangeTest, MessageComposerPushBuffer) {
    MessageComposer composer;
    composer.push(std::span<const std::byte>{
        reinterpret_cast<const std::byte*>(TEST_BUFFER_VALUES),
        TEST_BUFFER_SIZE});
    composer.clear();
}

TEST_F(MessageExchangeTest, MessageComposerPushDeque) {
    MessageComposer composer;
    composer.push(std::deque<std::string>{
        TEST_STRING_ONE, TEST_STRING_TWO, TEST_STRING_THREE});
    composer.clear();
}

TEST_F(MessageExchangeTest, MessageComposerChaining) {
    MessageComposer composer;
    composer.push(1).push(2).push(3);
    composer.clear();
}

TEST_F(MessageExchangeTest, MessageDecoderReadPrimitive) {
    ConnectSockets();

    constexpr int value = TEST_VALUE_42;
    std::array<char, sizeof(int)> buffer;
    std::memcpy(buffer.data(), &value, sizeof(int));
    client_transport_->send(as_bytes_span(buffer));

    MessageDecoder decoder(*server_transport_);
    int result = 0;
    decoder.read(result);
    EXPECT_EQ(result, value);
}

TEST_F(MessageExchangeTest, MessageDecoderReadMultiplePrimitives) {
    ConnectSockets();

    constexpr int value1 = TEST_VALUE_10;
    constexpr int value2 = TEST_VALUE_20;
    constexpr bool value3 = true;

    auto write_value = [this](const auto& value) {
        std::array<char, sizeof(value)> buffer;
        std::memcpy(buffer.data(), &value, sizeof(value));
        client_transport_->send(as_bytes_span(buffer));
    };

    write_value(value1);
    write_value(value2);
    write_value(value3);

    MessageDecoder decoder(*server_transport_);
    int result1 = 0;
    int result2 = 0;
    bool result3 = false;
    decoder.read(result1).read(result2).read(result3);

    EXPECT_EQ(result1, value1);
    EXPECT_EQ(result2, value2);
    EXPECT_EQ(result3, value3);
}

TEST_F(MessageExchangeTest, MessageDecoderReadString) {
    ConnectSockets();

    const auto test_string = std::string(TEST_STRING_HELLO);
    const auto length = test_string.size();

    std::array<char, sizeof(std::size_t)> buffer;
    std::memcpy(buffer.data(), &length, sizeof(std::size_t));
    client_transport_->send(as_bytes_span(buffer));
    client_transport_->send(as_bytes_span(test_string));

    MessageDecoder decoder(*server_transport_);
    std::string result;
    decoder.read(result);
    EXPECT_EQ(result, test_string);
}

TEST_F(MessageExchangeTest, MessageDecoderReadVector) {
    ConnectSockets();

    const std::vector<uint8_t> test_vector(
        TEST_BUFFER_VALUES, TEST_BUFFER_VALUES + TEST_BUFFER_SIZE);
    const auto size = test_vector.size();

    std::array<char, sizeof(std::size_t)> buffer;
    std::memcpy(buffer.data(), &size, sizeof(std::size_t));
    client_transport_->send(as_bytes_span(buffer));
    client_transport_->send(as_bytes_span(test_vector));

    MessageDecoder decoder(*server_transport_);
    std::vector<std::byte> result;
    decoder.read(result);

    EXPECT_EQ(result.size(), test_vector.size());
    EXPECT_EQ(std::memcmp(result.data(), test_vector.data(), result.size()), 0);
}

TEST_F(MessageExchangeTest, MessageDecoderReadStringContainer) {
    ConnectSockets();

    const std::vector<std::string> strings = {
        TEST_STRING_ONE, TEST_STRING_TWO, TEST_STRING_THREE};

    auto write_string = [this](std::string_view str) {
        const auto len = str.size();
        std::array<char, sizeof(std::size_t)> buffer;
        std::memcpy(buffer.data(), &len, sizeof(std::size_t));
        client_transport_->send(as_bytes_span(buffer));
        client_transport_->send(
            std::as_bytes(std::span<const char>{str.data(), str.size()}));
    };

    const auto count = strings.size();
    std::array<char, sizeof(std::size_t)> buffer;
    std::memcpy(buffer.data(), &count, sizeof(std::size_t));
    client_transport_->send(as_bytes_span(buffer));

    for (const auto& str : strings) {
        write_string(str);
    }

    MessageDecoder decoder(*server_transport_);
    std::vector<std::string> result;
    decoder.read<std::vector<std::string>, std::string>(result);

    EXPECT_EQ(result, strings);
}

TEST_F(MessageExchangeTest, RoundTripPrimitive) {
    ConnectSockets();

    constexpr int value = TEST_VALUE_12345;
    MessageComposer composer;
    composer.push(value);
    composer.send(*client_transport_);

    MessageDecoder decoder(*server_transport_);
    int result = 0;
    decoder.read(result);
    EXPECT_EQ(result, value);
}

TEST_F(MessageExchangeTest, RoundTripString) {
    ConnectSockets();

    const auto test_string = std::string(TEST_STRING_ROUND_TRIP);
    MessageComposer composer;
    composer.push(test_string);
    composer.send(*client_transport_);

    MessageDecoder decoder(*server_transport_);
    std::string result;
    decoder.read(result);
    EXPECT_EQ(result, test_string);
}

TEST_F(MessageExchangeTest, RoundTripComplexMessage) {
    ConnectSockets();

    constexpr int test_int_value = TEST_VALUE_100;
    constexpr bool test_bool_value = true;
    const auto test_buffer_name = std::string(TEST_STRING_BUFFER);

    MessageComposer composer;
    composer.push(MessageType::PLOT_BUFFER_CONTENTS)
        .push(test_int_value)
        .push(test_bool_value)
        .push(test_buffer_name);
    composer.send(*client_transport_);

    MessageDecoder decoder(*server_transport_);
    MessageType type = MessageType::PLOT_BUFFER_CONTENTS;
    int value = 0;
    bool flag = false;
    std::string name;
    decoder.read(type).read(value).read(flag).read(name);

    EXPECT_EQ(type, MessageType::PLOT_BUFFER_CONTENTS);
    EXPECT_EQ(value, test_int_value);
    EXPECT_EQ(flag, test_bool_value);
    EXPECT_EQ(name, test_buffer_name);
}
