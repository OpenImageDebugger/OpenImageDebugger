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

#if defined(_WIN32)

#include "process.h"
#include "process_impl.h"

#include <memory>

#include <QProcess>
#include <QString>

using namespace std;

class ProcessImplWin32 final: public ProcessImpl
{
public:
    ProcessImplWin32()
        : proc_()
    {}

    void start(const std::vector<std::string> &command) override
    {
        const auto program = QString::fromStdString(command[0]);
        QStringList args;
        for (size_t i = 1; i < command.size(); i++) {
            args.append(QString::fromStdString(command[i]));
        }

        proc_.start(program, args);
        proc_.waitForStarted();
    }

    bool isRunning() override
    {
        return proc_.state() == QProcess::Running;
    }

    void kill() override
    {
        proc_.kill();
    }

private:
    QProcess proc_;
};

void Process::createImpl()
{
    impl_ = make_shared<ProcessImplWin32>();
}

#endif
