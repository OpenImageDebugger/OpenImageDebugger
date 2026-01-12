/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2026 OpenImageDebugger
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

#include "process_impl.h"

#include "process.h"

#include <cerrno>
#include <chrono>
#include <csignal>
#include <fstream>
#include <spawn.h>

extern char** environ;

namespace oid
{

class ProcessImplUnix final : public ProcessImpl
{
  public:
    ProcessImplUnix() = default;

    ProcessImplUnix(const ProcessImplUnix&) = delete;

    ProcessImplUnix(ProcessImplUnix&& other) noexcept = delete;

    ProcessImplUnix& operator=(const ProcessImplUnix&) = delete;

    ProcessImplUnix& operator=(ProcessImplUnix&& other) noexcept = delete;

    ~ProcessImplUnix() noexcept override
    {
        kill();
    }

    void start(std::vector<std::string>& command) override
    {
        const auto& windowBinaryPath = command[0];

        // #region agent log
        std::ofstream log_file_spawn_entry(
            "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
            std::ios::app);
        if (log_file_spawn_entry.is_open()) {
            log_file_spawn_entry
                << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H12","location":"process_unix.cpp:56","message":"ProcessImplUnix_start_entry","data":{"path":")"
                << windowBinaryPath << R"("},"timestamp":)"
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()
                << "}\n";
            log_file_spawn_entry.close();
        }
        // #endregion

        auto argv = std::vector<char*>{};
        argv.reserve(command.size());
        for (auto& arg : command) {
            argv.push_back(arg.data());
        }
        argv.push_back(nullptr);

        int spawn_result = posix_spawn(&pid_,
                                        windowBinaryPath.c_str(),
                                        nullptr, // TODO consider passing something here
                                        nullptr, // and here
                                        argv.data(),
                                        environ);

        // #region agent log
        std::ofstream log_file_spawn_result(
            "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
            std::ios::app);
        if (log_file_spawn_result.is_open()) {
            log_file_spawn_result
                << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H12","location":"process_unix.cpp:75","message":"ProcessImplUnix_start_result","data":{"spawn_result":)"
                << spawn_result << R"(,"pid":)" << pid_
                << R"(,"errno":)" << errno << R"(},"timestamp":)"
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()
                << "}\n";
            log_file_spawn_result.close();
        }
        // #endregion
    }

    [[nodiscard]] bool isRunning() const override
    {
        bool result = pid_ != 0 && ::kill(pid_, 0) == 0;

        // #region agent log
        std::ofstream log_file_isRunning(
            "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
            std::ios::app);
        if (log_file_isRunning.is_open()) {
            log_file_isRunning
                << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H12","location":"process_unix.cpp:90","message":"ProcessImplUnix_isRunning","data":{"pid":)"
                << pid_ << R"(,"kill_result":)" << (pid_ != 0 ? ::kill(pid_, 0) : -1)
                << R"(,"is_running":)" << (result ? "true" : "false")
                << R"(,"errno":)" << (pid_ != 0 && ::kill(pid_, 0) != 0 ? errno : 0)
                << R"(},"timestamp":)"
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()
                << "}\n";
            log_file_isRunning.close();
        }
        // #endregion

        return result;
    }

    void kill() override
    {
        ::kill(pid_, SIGTERM);
    }

  private:
    pid_t pid_{0};
};

void Process::createImpl()
{
    impl_ = std::make_shared<ProcessImplUnix>();
}

} // namespace oid
