/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2020 OpenImageDebugger
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

#if defined(__unix__) || defined(__APPLE__)

#include "process_impl.h"
#include "process.h"

#include <signal.h>
#include <spawn.h>

#include <memory>

using namespace std;

class ProcessImplUnix final : public ProcessImpl
{
public:
    ProcessImplUnix()
        : pid_{0}
    {}

    ~ProcessImplUnix()
    {
        kill();
    }

    void start(const std::vector<std::string>& command) override
    {
        const auto windowBinaryPath = command[0];

        vector<char*> argv;
        argv.reserve(command.size());
        for (size_t i = 1; i < command.size(); i++) {
            argv.push_back(const_cast<char*>(command[i].c_str()));
        }
        argv.push_back(nullptr);

        extern char** environ;

        posix_spawn(&pid_,
                    windowBinaryPath.c_str(),
                    nullptr, // TODO consider passing something here
                    nullptr, // and here
                    &argv[0],
                    environ);
    }

    bool isRunning() override
    {
        return pid_ != 0 && ::kill(pid_, 0) == 0;
    }

    void kill() override
    {
        ::kill(pid_, SIGTERM);
    }

private:
    pid_t pid_;
};

void Process::createImpl()
{
    impl_ = make_shared<ProcessImplUnix>();
}

#endif // defined(__unix__) || defined(__APPLE__)
