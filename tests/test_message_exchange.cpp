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

#include <QCoreApplication>
#include <QTcpServer>
#include <QTcpSocket>
#include <array>
#include <chrono>
#include <cstring>
#include <gtest/gtest.h>
#include <span>
#include <string_view>
#include <thread>

#include "ipc/message_exchange.h"

using namespace oid;

namespace {
// Get or create QCoreApplication instance for all tests
QCoreApplication &GetApplication() {
  static int argc = 1;
  static std::string app_name = "test";
  static std::array<char *, 2> argv = {app_name.data(), nullptr};
  static QCoreApplication application(argc, argv.data());
  return application;
}

constexpr int CONNECTION_TIMEOUT_MS = 5000;
constexpr int POLL_STEP_MS = 10;
constexpr int TEST_VALUE_42 = 42;
constexpr int TEST_VALUE_100 = 100;
constexpr int TEST_VALUE_12345 = 12345;
constexpr int TEST_VALUE_10 = 10;
constexpr int TEST_VALUE_20 = 20;
constexpr unsigned char MAX_UCHAR = 255;

// Test string constants
constexpr const char *TEST_STRING_HELLO = "Hello, World!";
constexpr const char *TEST_STRING_TEST = "Test String";
constexpr const char *TEST_STRING_ROUND_TRIP = "Round Trip Test";
constexpr const char *TEST_STRING_BUFFER = "test_buffer";
constexpr const char *TEST_STRING_ONE = "one";
constexpr const char *TEST_STRING_TWO = "two";
constexpr const char *TEST_STRING_THREE = "three";
} // namespace

class MessageExchangeTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Initialize QCoreApplication if not already initialized
    GetApplication();
  }

  void TearDown() override {
    if (client_socket_) {
      client_socket_->close();
    }
    if (server_socket_) {
      server_socket_->close();
    }
    if (server_) {
      server_->close();
    }
    client_socket_.reset();
    server_.reset();
    server_socket_ = nullptr;
    QCoreApplication::processEvents();
  }

  void SetupServer() {
    if (!server_) {
      server_ = std::make_unique<QTcpServer>();
      ASSERT_TRUE(server_->listen(QHostAddress::LocalHost, 0))
          << "Failed to start server";
      port_ = server_->serverPort();
    }
  }

  template <typename Condition>
  bool WaitForConnection(const Condition &condition) const {
    int elapsed_ms = 0;
    while (!condition() && elapsed_ms < CONNECTION_TIMEOUT_MS) {
      QCoreApplication::processEvents();
      std::this_thread::sleep_for(std::chrono::milliseconds(POLL_STEP_MS));
      elapsed_ms += POLL_STEP_MS;
    }
    return condition();
  }

  void ConnectSockets() {
    SetupServer();
    client_socket_ = std::make_unique<QTcpSocket>();
    client_socket_->connectToHost(QHostAddress::LocalHost, port_);

    ASSERT_TRUE(WaitForConnection([this]() {
      return client_socket_->state() == QAbstractSocket::ConnectedState;
    })) << "Client failed to connect";

    ASSERT_TRUE(WaitForConnection([this]() {
      return server_->hasPendingConnections();
    })) << "Server has no pending connections";

    server_socket_ = server_->nextPendingConnection();
    ASSERT_NE(server_socket_, nullptr) << "Failed to get server socket";
    QCoreApplication::processEvents();
  }

  std::unique_ptr<QTcpServer> server_;
  std::unique_ptr<QTcpSocket> client_socket_;
  QTcpSocket *server_socket_ = nullptr;
  quint16 port_ = 0;
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
  EXPECT_EQ(std::string(block.data(), block.data() + block.size()),
            test_string);
}

namespace {
constexpr uint8_t TEST_BUFFER_VALUES[] = {1, 2, 3, 4, 5};
constexpr std::size_t TEST_BUFFER_SIZE = sizeof(TEST_BUFFER_VALUES);
} // namespace

TEST_F(MessageExchangeTest, BufferBlockSize) {
  BufferBlock block(
      std::span<const uint8_t>{TEST_BUFFER_VALUES, TEST_BUFFER_SIZE});
  EXPECT_EQ(block.size(), TEST_BUFFER_SIZE);
}

TEST_F(MessageExchangeTest, BufferBlockData) {
  BufferBlock block(
      std::span<const uint8_t>{TEST_BUFFER_VALUES, TEST_BUFFER_SIZE});
  const auto *data = block.data();
  for (auto i = 0U; i < TEST_BUFFER_SIZE; ++i) {
    EXPECT_EQ(data[i], TEST_BUFFER_VALUES[i]);
  }
}

TEST_F(MessageExchangeTest, MessageComposerPushPrimitive) {
  MessageComposer composer;
  composer.push(TEST_VALUE_42).push(MAX_UCHAR).push(true).push(TEST_VALUE_100);
  composer.clear();
}

TEST_F(MessageExchangeTest, MessageComposerPushString) {
  MessageComposer composer;
  composer.push(std::string(TEST_STRING_TEST));
  composer.clear();
}

TEST_F(MessageExchangeTest, MessageComposerPushBuffer) {
  MessageComposer composer;
  composer.push(TEST_BUFFER_VALUES, TEST_BUFFER_SIZE);
  composer.clear();
}

TEST_F(MessageExchangeTest, MessageComposerPushDeque) {
  MessageComposer composer;
  composer.push(std::deque<std::string>{TEST_STRING_ONE, TEST_STRING_TWO,
                                        TEST_STRING_THREE});
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
  client_socket_->write(buffer.data(), sizeof(int));
  client_socket_->waitForBytesWritten();
  QCoreApplication::processEvents();

  MessageDecoder decoder(server_socket_);
  int result = 0;
  decoder.read(result);
  EXPECT_EQ(result, value);
}

TEST_F(MessageExchangeTest, MessageDecoderReadMultiplePrimitives) {
  ConnectSockets();

  constexpr int value1 = TEST_VALUE_10;
  constexpr int value2 = TEST_VALUE_20;
  constexpr bool value3 = true;

  auto write_value = [this](const auto &value) {
    std::array<char, sizeof(value)> buffer;
    std::memcpy(buffer.data(), &value, sizeof(value));
    client_socket_->write(buffer.data(), sizeof(value));
  };

  write_value(value1);
  write_value(value2);
  write_value(value3);
  client_socket_->waitForBytesWritten();
  QCoreApplication::processEvents();

  MessageDecoder decoder(server_socket_);
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
  client_socket_->write(buffer.data(), sizeof(std::size_t));
  client_socket_->write(test_string.data(), static_cast<qint64>(length));
  client_socket_->waitForBytesWritten();
  QCoreApplication::processEvents();

  MessageDecoder decoder(server_socket_);
  std::string result;
  decoder.read(result);
  EXPECT_EQ(result, test_string);
}

TEST_F(MessageExchangeTest, MessageDecoderReadVector) {
  ConnectSockets();

  const std::vector<uint8_t> test_vector(TEST_BUFFER_VALUES,
                                         TEST_BUFFER_VALUES + TEST_BUFFER_SIZE);
  const auto size = test_vector.size();

  std::array<char, sizeof(std::size_t)> buffer;
  std::memcpy(buffer.data(), &size, sizeof(std::size_t));
  client_socket_->write(buffer.data(), sizeof(std::size_t));
  client_socket_->write(
      static_cast<const char *>(static_cast<const void *>(test_vector.data())),
      static_cast<qint64>(test_vector.size()));
  client_socket_->waitForBytesWritten();
  QCoreApplication::processEvents();

  MessageDecoder decoder(server_socket_);
  std::vector<uint8_t> result;
  decoder.read(result);

  EXPECT_EQ(result, test_vector);
}

TEST_F(MessageExchangeTest, MessageDecoderReadStringContainer) {
  ConnectSockets();

  const std::vector<std::string> strings = {TEST_STRING_ONE, TEST_STRING_TWO,
                                            TEST_STRING_THREE};

  auto write_string = [this](std::string_view str) {
    const auto len = str.size();
    std::array<char, sizeof(std::size_t)> buffer;
    std::memcpy(buffer.data(), &len, sizeof(std::size_t));
    client_socket_->write(buffer.data(), sizeof(std::size_t));
    client_socket_->write(str.data(), static_cast<qint64>(len));
  };

  const auto count = strings.size();
  std::array<char, sizeof(std::size_t)> buffer;
  std::memcpy(buffer.data(), &count, sizeof(std::size_t));
  client_socket_->write(buffer.data(), sizeof(std::size_t));

  for (const auto &str : strings) {
    write_string(str);
  }
  client_socket_->waitForBytesWritten();
  QCoreApplication::processEvents();

  MessageDecoder decoder(server_socket_);
  std::vector<std::string> result;
  decoder.read<std::vector<std::string>, std::string>(result);

  EXPECT_EQ(result, strings);
}

TEST_F(MessageExchangeTest, RoundTripPrimitive) {
  ConnectSockets();

  constexpr int value = TEST_VALUE_12345;
  MessageComposer composer;
  composer.push(value).send(client_socket_.get());
  QCoreApplication::processEvents();

  MessageDecoder decoder(server_socket_);
  int result = 0;
  decoder.read(result);
  EXPECT_EQ(result, value);
}

TEST_F(MessageExchangeTest, RoundTripString) {
  ConnectSockets();

  const auto test_string = std::string(TEST_STRING_ROUND_TRIP);
  MessageComposer composer;
  composer.push(test_string).send(client_socket_.get());
  QCoreApplication::processEvents();

  MessageDecoder decoder(server_socket_);
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
  composer.push(MessageType::PlotBufferContents)
      .push(test_int_value)
      .push(test_bool_value)
      .push(test_buffer_name)
      .send(client_socket_.get());
  QCoreApplication::processEvents();

  MessageDecoder decoder(server_socket_);
  MessageType type = MessageType::PlotBufferContents;
  int value = 0;
  bool flag = false;
  std::string name;
  decoder.read(type).read(value).read(flag).read(name);

  EXPECT_EQ(type, MessageType::PlotBufferContents);
  EXPECT_EQ(value, test_int_value);
  EXPECT_EQ(flag, test_bool_value);
  EXPECT_EQ(name, test_buffer_name);
}
