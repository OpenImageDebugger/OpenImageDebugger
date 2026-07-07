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

#include "ipc/asio_transport.h"
#include "ipc/message_exchange.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <memory>
#include <optional>
#include <thread>

#include <asio.hpp>
#include <gtest/gtest.h>

using namespace oid;

TEST(AsioTransport, RoundTripBytes) {
    asio::io_context server_ctx;
    asio::ip::tcp::acceptor acceptor(
        server_ctx,
        asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), 0));
    const unsigned short port = acceptor.local_endpoint().port();

    asio::ip::tcp::socket server_sock(server_ctx);
    std::jthread server([&acceptor, &server_sock] {
        acceptor.accept(server_sock);
        // Echo 4 bytes back after receiving 4.
        std::array<std::byte, 4> buf{};
        asio::read(server_sock, asio::buffer(buf));
        asio::write(server_sock, asio::buffer(buf));
    });

    AsioTransport client("127.0.0.1", port);
    const std::array<std::byte, 4> out{
        std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
    client.send(out);
    std::array<std::byte, 4> in{};
    std::size_t got = 0;
    while (got < in.size()) {
        got += client.receive(std::span{in}.subspan(got));
    }
    server.join();
    EXPECT_EQ(in, out);
}

TEST(AsioTransport, HasDataFalseBeforeSend) {
    asio::io_context server_ctx;
    asio::ip::tcp::acceptor acceptor(
        server_ctx,
        asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), 0));
    const unsigned short port = acceptor.local_endpoint().port();
    asio::ip::tcp::socket server_sock(server_ctx);
    std::jthread server(
        [&acceptor, &server_sock] { acceptor.accept(server_sock); });
    AsioTransport client("127.0.0.1", port);
    server.join();
    EXPECT_FALSE(client.has_data()); // nothing sent by peer yet
}

TEST(AsioCodec, RoundTripPlotBufferRequest) {
    asio::io_context server_ctx;
    asio::ip::tcp::acceptor acceptor(
        server_ctx,
        asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), 0));
    const unsigned short port = acceptor.local_endpoint().port();
    asio::ip::tcp::socket raw_server(server_ctx);
    std::jthread server(
        [&acceptor, &raw_server] { acceptor.accept(raw_server); });

    AsioTransport client("127.0.0.1", port);
    server.join();

    // Send: PLOT_BUFFER_REQUEST + "myVar" from the client.
    MessageComposer composer;
    composer.push(MessageType::PLOT_BUFFER_REQUEST).push(std::string("myVar"));
    composer.send(client);

    // Decode on the server side via a thin ITransport over raw_server.
    struct ServerTransport final : ITransport {
        asio::ip::tcp::socket& s;
        explicit ServerTransport(asio::ip::tcp::socket& sock) : s(sock) {}
        void send(std::span<const std::byte> d) override {
            asio::write(s, asio::buffer(d.data(), d.size()));
        }
        std::size_t receive(std::span<std::byte> dst) override {
            asio::error_code ec;
            const std::size_t n =
                s.read_some(asio::buffer(dst.data(), dst.size()), ec);
            return ec ? 0 : n;
        }
        bool has_data() const override {
            asio::error_code ec;
            return s.available(ec) > 0 && !ec;
        }
    };
    ServerTransport server_transport{raw_server};

    MessageType type{};
    MessageDecoder decoder(server_transport);
    decoder.read(type);
    std::string name;
    decoder.read(name);
    EXPECT_EQ(type, MessageType::PLOT_BUFFER_REQUEST);
    EXPECT_EQ(name, "myVar");
}

TEST(AsioTransport, ReceiveTimesOutOnSilentPeer) {
    AsioAcceptor acceptor;
    std::jthread server([&acceptor] {
        auto t = acceptor.accept(std::chrono::seconds{5});
        std::this_thread::sleep_for(std::chrono::milliseconds{300});
    });
    AsioTransport client("127.0.0.1", acceptor.port());
    client.set_timeout(std::chrono::milliseconds{100});
    std::array<std::byte, 4> buf{};
    EXPECT_THROW(client.receive(buf), SocketTimeoutError); // peer never sends
    server.join();
}

TEST(AsioAcceptor, AcceptThenRoundTrip) {
    AsioAcceptor acceptor;
    const unsigned short port = acceptor.port();
    std::optional<AsioTransport> server_side;
    std::jthread server([&server_side, &acceptor] {
        server_side.emplace(acceptor.accept(std::chrono::seconds{5}));
    });
    AsioTransport client("127.0.0.1", port);
    server.join();
    const std::array<std::byte, 3> out{
        std::byte{7}, std::byte{8}, std::byte{9}};
    client.send(out);
    std::array<std::byte, 3> in{};
    std::size_t got = 0;
    while (got < in.size()) {
        got += server_side->receive(std::span{in}.subspan(got));
    }
    EXPECT_EQ(in, out);
}

TEST(AsioTransport, ConnectRefusedDoesNotThrow) {
    // Bind an ephemeral port, learn its number, then let the acceptor go
    // out of scope so nothing is listening on it anymore.
    unsigned short port = 0;
    {
        asio::io_context ctx;
        asio::ip::tcp::acceptor acceptor(
            ctx,
            asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), 0));
        port = acceptor.local_endpoint().port();
    }

    std::optional<AsioTransport> client;
    EXPECT_NO_THROW(
        client.emplace("127.0.0.1", port, std::chrono::milliseconds{300}));
    EXPECT_FALSE(client->is_connected());
}

TEST(AsioTransport, IsConnectedTrueWhileOpenNoData) {
    AsioAcceptor acceptor;
    const unsigned short port = acceptor.port();
    std::optional<AsioTransport> server_side;
    std::jthread server([&server_side, &acceptor] {
        server_side.emplace(acceptor.accept(std::chrono::seconds{5}));
    });
    AsioTransport client("127.0.0.1", port);
    server.join();
    EXPECT_TRUE(client.is_connected());
}

TEST(AsioTransport, IsConnectedFalseAfterPeerClose) {
    AsioAcceptor acceptor;
    const unsigned short port = acceptor.port();
    std::optional<AsioTransport> server_side;
    std::jthread server([&server_side, &acceptor] {
        server_side.emplace(acceptor.accept(std::chrono::seconds{5}));
    });
    AsioTransport client("127.0.0.1", port);
    server.join();

    server_side.reset(); // close the server side of the connection

    std::this_thread::sleep_for(std::chrono::milliseconds{50});
    EXPECT_FALSE(client.is_connected());
}
