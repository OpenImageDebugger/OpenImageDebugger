/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2025 OpenImageDebugger
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

using namespace std;
using namespace oid;

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

        vector<char*> argv;
        argv.reserve(command.size());
        for (auto& arg : command) {
            argv.push_back(arg.data());
        }
        argv.push_back(nullptr);

        extern char** environ;

        posix_spawn(&pid_,
                    windowBinaryPath.c_str(),
                    nullptr, // TODO consider passing something here
                    nullptr, // and here
                    argv.data(),
                    environ);
    }

    [[nodiscard]] bool isRunning() const override
    {
        return pid_ != 0 && ::kill(pid_, 0) == 0;
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
    impl_ = make_shared<ProcessImplUnix>();
}
