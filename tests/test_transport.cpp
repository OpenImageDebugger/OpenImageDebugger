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

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>

#include <array>
#include <thread>
#include <vector>

#include "ipc/tcp_transport.h"
#include "platform/postmessage_transport.h"

namespace {

QCoreApplication& app() {
    static int argc = 1;
    static std::array<char*, 2> argv = {const_cast<char*>("test"), nullptr};
    static QCoreApplication application(argc, argv.data());
    return application;
}

} // namespace

TEST(TcpTransport, RoundTripBytes) {
    (void)app();
    QTcpServer server;
    ASSERT_TRUE(server.listen(QHostAddress::LocalHost));

    std::vector<std::byte> server_received;
    std::thread server_thread([&] {
        ASSERT_TRUE(server.waitForNewConnection(5000));
        QTcpSocket* peer = server.nextPendingConnection();
        ASSERT_TRUE(peer->waitForReadyRead(5000));
        const QByteArray data = peer->readAll();
        server_received.assign(
            reinterpret_cast<const std::byte*>(data.constData()),
            reinterpret_cast<const std::byte*>(data.constData()) + data.size());
        peer->write(data);
        peer->waitForBytesWritten(5000);
        peer->close();
        delete peer;
    });

    QTcpSocket client;
    client.connectToHost(QHostAddress::LocalHost, server.serverPort());
    ASSERT_TRUE(client.waitForConnected(5000));

    oid::TcpTransport transport(client);
    const std::vector<std::byte> sent{std::byte{0x03}, std::byte{0x01}};
    transport.send(sent);

    std::array<std::byte, 2> buf{};
    const auto n = transport.receive(buf);
    EXPECT_EQ(n, 2u);
    EXPECT_EQ(buf[0], std::byte{0x03});
    EXPECT_EQ(buf[1], std::byte{0x01});
    EXPECT_EQ(server_received, sent);

    server_thread.join();
}

TEST(PostMessageTransport, QueuesInboundAndReceive) {
    oid::PostMessageTransport transport;
    const std::vector<std::byte> msg{std::byte{0x03}, std::byte{0xAA}};
    transport.enqueue_inbound(msg);

    EXPECT_TRUE(transport.has_data());

    std::array<std::byte, 2> out{};
    EXPECT_EQ(transport.receive(out), 2u);
    EXPECT_EQ(out[0], std::byte{0x03});
    EXPECT_EQ(out[1], std::byte{0xAA});
    EXPECT_FALSE(transport.has_data());
}

TEST(PostMessageTransport, SendIsNoOpOnDesktop) {
    oid::PostMessageTransport transport;
    const std::vector<std::byte> msg{std::byte{0x01}};
    transport.send(msg);
    EXPECT_FALSE(transport.has_data());
}
