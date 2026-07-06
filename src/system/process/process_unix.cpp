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

#include <csignal>
#include <spawn.h>
#include <sys/wait.h>

extern char** environ;

namespace oid {

class ProcessImplUnix final : public ProcessImpl {
  public:
    ProcessImplUnix() = default;

    ProcessImplUnix(const ProcessImplUnix&) = delete;

    ProcessImplUnix(ProcessImplUnix&& other) noexcept = delete;

    ProcessImplUnix& operator=(const ProcessImplUnix&) = delete;

    ProcessImplUnix& operator=(ProcessImplUnix&& other) noexcept = delete;

    ~ProcessImplUnix() noexcept override {
        kill();
    }

    void start(std::vector<std::string>& command) override {
        const auto& windowBinaryPath = command[0];

        auto argv = std::vector<char*>{};
        argv.reserve(command.size());
        for (auto& arg : command) {
            argv.push_back(arg.data());
        }
        argv.push_back(nullptr);

        posix_spawn(&pid_,
                    windowBinaryPath.c_str(),
                    nullptr, // TODO consider passing something here
                    nullptr, // and here
                    argv.data(),
                    environ);
    }

    [[nodiscard]] bool isRunning() const override {
        if (pid_ == 0) {
            return false;
        }
        // The spawned window is our child: when it exits (e.g. the user
        // closes it) it stays a zombie until waited on, and ::kill(pid, 0)
        // "succeeds" on a zombie. Without reaping here, a closed window would
        // be reported as running forever and the bridge would never relaunch
        // it. WNOHANG keeps this non-blocking.
        int status = 0;
        const auto reaped = ::waitpid(pid_, &status, WNOHANG);
        if (reaped == pid_) {
            pid_ = 0; // exited; reaped just now
            return false;
        }
        if (reaped == 0) {
            return true; // still running
        }
        // -1: not reapable by us (e.g. ECHILD when SIGCHLD is SIG_IGN and the
        // system auto-reaps). Fall back to the signal-0 liveness probe.
        if (::kill(pid_, 0) != 0) {
            pid_ = 0;
            return false;
        }
        return true;
    }

    void kill() override {
        // Never signal pid 0: that would SIGTERM our whole process group
        // (including the debugger the bridge is loaded into).
        if (pid_ > 0) {
            ::kill(pid_, SIGTERM);
        }
    }

  private:
    // mutable: isRunning() is const but must record the reaped/exited state
    // so a recycled pid is never probed again.
    mutable pid_t pid_{0};
};

void Process::createImpl() {
    impl_ = std::make_shared<ProcessImplUnix>();
}

} // namespace oid
