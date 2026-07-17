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

#include "host/agent/agent_server.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <future>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include <asio.hpp>
#include <gtest/gtest.h>

#include "host/agent/discovery_file.h"
#include "host/agent/fake_view_model.h"
#include "host/agent/wire_frame.h"

#ifdef _WIN32
#include <process.h> // _getpid
#else
#include <sys/stat.h>
#include <unistd.h> // getpid
#endif

using namespace oid::host::agent;
using json = nlohmann::json;

namespace {

long current_pid() {
#ifdef _WIN32
    return static_cast<long>(_getpid());
#else
    return static_cast<long>(getpid());
#endif
}

// Sets an environment variable for the lifetime of the object, restoring
// whatever (if anything) was there before.
class ScopedEnvVar {
  public:
    ScopedEnvVar(const char* name, const std::string& value) : name_(name) {
        if (const char* prev = std::getenv(name)) {
            had_prev_ = true;
            prev_ = prev;
        }
#ifdef _WIN32
        _putenv_s(name_.c_str(), value.c_str());
#else
        setenv(name_.c_str(), value.c_str(), 1);
#endif
    }

    ~ScopedEnvVar() {
#ifdef _WIN32
        _putenv_s(name_.c_str(), had_prev_ ? prev_.c_str() : "");
#else
        if (had_prev_) {
            setenv(name_.c_str(), prev_.c_str(), 1);
        } else {
            unsetenv(name_.c_str());
        }
#endif
    }

    ScopedEnvVar(const ScopedEnvVar&) = delete;
    ScopedEnvVar& operator=(const ScopedEnvVar&) = delete;

  private:
    std::string name_;
    std::string prev_;
    bool had_prev_ = false;
};

std::filesystem::path make_temp_dir() {
#ifndef _WIN32
    // mkdtemp atomically creates a uniquely-named 0700 directory, avoiding the
    // predictable-name race of creating a temp dir in a world-writable
    // location (S5443).
    std::string tmpl = (std::filesystem::temp_directory_path() / // NOSONAR
                        "oid_agent_server_test_XXXXXX")
                           .string();
    if (::mkdtemp(tmpl.data()) == nullptr) {
        throw std::runtime_error( // NOSONAR
            "mkdtemp failed for test temp directory");
    }
    return std::filesystem::path{tmpl};
#else
    static std::atomic<int> counter{0};
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() /
        ("oid_agent_server_test_" + std::to_string(stamp) + "_" +
         std::to_string(counter++));
    std::filesystem::create_directories(dir);
    return dir;
#endif
}

// Reads one frame off `sock`, blocking until the whole frame has arrived
// (or throwing if the peer closes first).
DecodedFrame read_frame(asio::ip::tcp::socket& sock) {
    return decode_frame(
        [&sock](std::span<std::byte> dst) {
            asio::read(sock, asio::buffer(dst.data(), dst.size()));
        },
        std::nullopt);
}

void send_frame(asio::ip::tcp::socket& sock, const json& obj) {
    const std::vector<std::byte> bytes = encode_frame(obj);
    asio::write(sock, asio::buffer(bytes.data(), bytes.size()));
}

// Repeatedly calls server.drain() (as the real main loop would once per
// frame) until `sock` has bytes ready to read, bounded by a timeout so a
// broken test fails fast instead of hanging.
void drain_until_reply(AgentServer& server, const asio::ip::tcp::socket& sock) {
    asio::error_code ec;
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (sock.available(ec) == 0 &&
           std::chrono::steady_clock::now() < deadline) {
        server.drain();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

class AgentServerTest : public ::testing::Test { // NOSONAR
  protected:
    void SetUp() override {
        tmp_dir_ = make_temp_dir();
        env_ =
            std::make_unique<ScopedEnvVar>("OID_AGENT_DIR", tmp_dir_.string());
    }

    void TearDown() override {
        env_.reset();
        std::error_code ec;
        std::filesystem::remove_all(tmp_dir_, ec);
    }

    asio::ip::tcp::socket connect_to(const AgentServer& server) {
        asio::ip::tcp::socket sock(io_context_);
        sock.connect(asio::ip::tcp::endpoint(
            asio::ip::make_address("127.0.0.1"), server.port()));
        return sock;
    }

    std::filesystem::path tmp_dir_;
    std::unique_ptr<ScopedEnvVar> env_;
    asio::io_context io_context_;
};

} // namespace

TEST_F(AgentServerTest, WritesDiscoveryFileInViewerSubdir) {
    FakeViewModel model;
    AgentServerConfig cfg;
    cfg.enabled = true;
    cfg.debugger_pid = 4200;
    AgentServer server(model, cfg);

    const std::filesystem::path viewer_dir = tmp_dir_ / "viewer";
    const std::filesystem::path discovery_path =
        viewer_dir / std::format("{}.json", current_pid());
    ASSERT_TRUE(std::filesystem::exists(discovery_path));

    std::ifstream in(discovery_path, std::ios::binary);
    std::ostringstream contents;
    contents << in.rdbuf();
    const json body = json::parse(contents.str());

    EXPECT_EQ(body["port"], server.port());
    EXPECT_EQ(body["token"], server.token());
    EXPECT_EQ(body["debugger_pid"], 4200);

#ifndef _WIN32
    struct stat file_info{};
    ASSERT_EQ(::stat(discovery_path.c_str(), &file_info), 0);
    EXPECT_EQ(file_info.st_mode & 0777, 0600);

    struct stat dir_info{};
    ASSERT_EQ(::stat(viewer_dir.c_str(), &dir_info), 0);
    EXPECT_EQ(dir_info.st_mode & 0777, 0700);
#endif

    server.stop();
}

TEST_F(AgentServerTest, RemovesDiscoveryOnStop) {
    FakeViewModel model;
    AgentServerConfig cfg;
    cfg.enabled = true;
    AgentServer server(model, cfg);

    const std::filesystem::path discovery_path =
        tmp_dir_ / "viewer" / std::format("{}.json", current_pid());
    ASSERT_TRUE(std::filesystem::exists(discovery_path));

    server.stop();

    EXPECT_FALSE(std::filesystem::exists(discovery_path));
}

#ifndef _WIN32
TEST_F(AgentServerTest, RejectsSymlinkedBaseDir) {
    // The predictable base discovery dir (the viewer dir's parent) is hardened
    // too, not just the leaf: a symlinked base -- which could redirect the
    // token-bearing discovery file under an attacker-controlled location --
    // must be rejected.
    const std::filesystem::path target = tmp_dir_ / "real-target";
    const std::filesystem::path link = tmp_dir_ / "symlinked-base";
    std::filesystem::create_directory(target);
    std::filesystem::create_directory_symlink(target, link);
    ScopedEnvVar base_override("OID_AGENT_DIR", link.string());

    FakeViewModel model;
    AgentServerConfig cfg;
    cfg.enabled = true;
    EXPECT_THROW(AgentServer(model, cfg), DiscoveryError);
}
#endif

TEST_F(AgentServerTest, HelloRequiredFirst) {
    FakeViewModel model;
    AgentServerConfig cfg;
    cfg.enabled = true;
    AgentServer server(model, cfg);

    asio::ip::tcp::socket sock = connect_to(server);
    send_frame(sock, {{"method", "ping"}});

    drain_until_reply(server, sock);
    const DecodedFrame reply = read_frame(sock);

    EXPECT_EQ(reply.obj["error"]["code"], "bad_token");

    server.stop();
}

TEST_F(AgentServerTest, HelloThenListBuffers) {
    FakeViewModel model;
    model.add("frame", 4, 5, 1);
    AgentServerConfig cfg;
    cfg.enabled = true;
    AgentServer server(model, cfg);

    asio::ip::tcp::socket sock = connect_to(server);

    send_frame(sock, {{"method", "hello"}, {"token", server.token()}});
    drain_until_reply(server, sock);
    const DecodedFrame hello_reply = read_frame(sock);
    ASSERT_FALSE(hello_reply.obj.contains("error"));
    EXPECT_EQ(hello_reply.obj["kind"], "viewer");

    send_frame(sock, {{"method", "list_buffers"}});
    drain_until_reply(server, sock);
    const DecodedFrame list_reply = read_frame(sock);
    ASSERT_FALSE(list_reply.obj.contains("error"));
    ASSERT_EQ(list_reply.obj["buffers"].size(), 1u);
    EXPECT_EQ(list_reply.obj["buffers"][0]["name"], "frame");

    server.stop();
}

TEST_F(AgentServerTest, StopUnblocksIdleServingConnection) {
    // A connection whose serve thread is blocked in a post-auth read when
    // stop() is called must tear down without hanging: stop() only sets the
    // flag + stops the ctx (thread-safe), and run_async_op closes the socket on
    // the serve thread itself. Run stop() under a watchdog so a regression to
    // cross-thread teardown surfaces as a failure, not a frozen suite.
    FakeViewModel model;
    model.add("frame", 4, 5, 1);
    AgentServerConfig cfg;
    cfg.enabled = true;
    AgentServer server(model, cfg);

    asio::ip::tcp::socket sock = connect_to(server);
    send_frame(sock, {{"method", "hello"}, {"token", server.token()}});
    drain_until_reply(server, sock);
    (void)read_frame(sock); // hello reply; the serve thread now blocks on read

    std::packaged_task<void()> stop_task([&server] { server.stop(); });
    std::future<void> stopped = stop_task.get_future();
    std::jthread runner(std::move(stop_task)); // auto-joins on scope exit
    ASSERT_EQ(stopped.wait_for(std::chrono::seconds(5)),
              std::future_status::ready)
        << "stop() hung with an idle serving connection";
    stopped.get();

    EXPECT_ANY_THROW((void)read_frame(sock));
}

TEST_F(AgentServerTest, StopUnblocksIdlePreAuthConnection) {
    // The pre-auth twin of StopUnblocksIdleServingConnection: a connection
    // whose serve thread is blocked in the bounded (handshake-deadline) read
    // when stop() is called must also tear down without hanging. That read
    // takes run_async_op's timeout branch, whose drain has to survive stop()
    // calling ctx.stop() mid-run_for -- a regression there strands the aborted
    // read handler (use-after-free) or freezes the suite.
    FakeViewModel model;
    model.add("frame", 4, 5, 1);
    AgentServerConfig cfg;
    cfg.enabled = true;
    AgentServer server(model, cfg);

    asio::ip::tcp::socket sock = connect_to(server);
    // Drive one pre-auth frame and read its rejection (serviced via drain(),
    // as the real main loop would): this proves the serve thread is up and now
    // blocked in the *next* pre-auth read (run_async_op's timeout branch),
    // without ever authenticating, and rules out racing stop() against accept.
    send_frame(sock, {{"method", "ping"}});
    drain_until_reply(server, sock);
    const DecodedFrame denied = read_frame(sock);
    ASSERT_TRUE(denied.obj.contains("error")); // hello required first

    std::packaged_task<void()> stop_task([&server] { server.stop(); });
    std::future<void> stopped = stop_task.get_future();
    std::jthread runner(std::move(stop_task)); // auto-joins on scope exit
    ASSERT_EQ(stopped.wait_for(std::chrono::seconds(5)),
              std::future_status::ready)
        << "stop() hung with an idle pre-auth connection";
    stopped.get();

    EXPECT_ANY_THROW((void)read_frame(sock));
}

TEST_F(AgentServerTest, DrainContainsHandlerException) {
    // A ViewModel handler that throws (e.g. bad_alloc on a large get_buffer
    // copy) must be contained by drain(): it must not escape and crash the
    // render thread. The serve thread rethrows via future.get() and closes just
    // that connection.
    FakeViewModel model;
    model.add("frame", 4, 5, 1);
    model.set_bytes("frame", std::vector<std::byte>(4));
    model.throw_in_read_pixels = true;
    AgentServerConfig cfg;
    cfg.enabled = true;
    AgentServer server(model, cfg);

    asio::ip::tcp::socket sock = connect_to(server);
    send_frame(sock, {{"method", "hello"}, {"token", server.token()}});
    drain_until_reply(server, sock);
    (void)read_frame(sock); // hello reply

    send_frame(sock, {{"method", "get_buffer"}, {"symbol", "frame"}});
    // Drive drain() until the throwing handler has run; drain() itself must
    // never throw (that is the crash the barrier prevents).
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (model.read_pixels_calls == 0 &&
           std::chrono::steady_clock::now() < deadline) {
        EXPECT_NO_THROW(server.drain());
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    EXPECT_EQ(model.read_pixels_calls, 1);

    server.stop();
}

TEST_F(AgentServerTest, OversizeReplyReturnsStructuredError) {
    // A reply whose JSON serializes past MAX_FRAME_BYTES -- here list_buffers
    // on a session with a huge number of buffers -- must come back as a
    // structured error the client can read, not a dropped connection.
    FakeViewModel model;
    const std::string long_name(200, 'b');
    for (int i = 0; i < 6000; ++i) {
        model.add(std::format("{}{}", long_name, i), 4, 5, 1);
    }
    AgentServerConfig cfg;
    cfg.enabled = true;
    AgentServer server(model, cfg);

    asio::ip::tcp::socket sock = connect_to(server);
    send_frame(sock, {{"method", "hello"}, {"token", server.token()}});
    drain_until_reply(server, sock);
    (void)read_frame(sock); // hello reply

    send_frame(sock, {{"method", "list_buffers"}});
    drain_until_reply(server, sock);
    const DecodedFrame reply = read_frame(sock);

    ASSERT_TRUE(reply.obj.contains("error"));
    EXPECT_EQ(reply.obj["error"]["code"], "internal");

    server.stop();
}

TEST_F(AgentServerTest, DrainRunsOnCallingThread) {
    FakeViewModel model;
    AgentServerConfig cfg;
    cfg.enabled = true;
    AgentServer server(model, cfg);

    asio::ip::tcp::socket sock = connect_to(server);
    send_frame(sock, {{"method", "ping"}});

    // Give the serve thread ample time to have queued the request, then
    // prove nothing is sent back without drain() ever being called.
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    asio::error_code ec;
    EXPECT_EQ(sock.available(ec), 0u);

    // Loop drain() within the wait window (as HelloRequiredFirst/
    // HelloThenListBuffers do) rather than a single fixed-delay call: a
    // one-shot drain() right after a fixed sleep can race ahead of a slow
    // CI's serve thread, land before the request is queued, and then never
    // run again to service it once it does land.
    drain_until_reply(server, sock);
    EXPECT_GT(sock.available(ec), 0u);

    server.stop();
}
