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

#include "process.h"
#include "process_impl.h"

#include <iostream>
#include <memory>

#include <QByteArray>
#include <QFileInfo>
#include <QIODevice>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

namespace oid
{


class ProcessImplWin32 final : public ProcessImpl
{
  public:
    ProcessImplWin32() = default;

    void start(std::vector<std::string>& command) override
    {
        const auto program = QString::fromStdString(command[0]);
        QStringList args;
        for (size_t i = 1; i < command.size(); i++) {
            args.append(QString::fromStdString(command[i]));
        }

        // Set working directory to executable's directory (critical for DLL loading on Windows)
        QFileInfo exeInfo(program);
        const QString workingDir = exeInfo.absolutePath();
        proc_.setWorkingDirectory(workingDir);

        // Capture stderr separately to diagnose startup failures
        proc_.setProcessChannelMode(QProcess::SeparateChannels);

        // Connect readyReadStandardError to capture any stderr output
        QObject::connect(&proc_, &QProcess::readyReadStandardError, [this]() {
            proc_.readAllStandardError();
        });

        // Start process with error checking
        proc_.start(program, args, QIODevice::ReadWrite);
        if (proc_.error() != QProcess::UnknownError) {
            std::cerr << "[OpenImageDebugger] Failed to start process: " 
                      << proc_.errorString().toStdString() << std::endl;
            return;
        }

        // Wait for start with timeout and error checking
        const int timeout_ms = 5000;
        if (!proc_.waitForStarted(timeout_ms)) {
            std::cerr << "[OpenImageDebugger] Process failed to start within " 
                      << timeout_ms << "ms: " << proc_.errorString().toStdString() << std::endl;
            return;
        }
    }

    [[nodiscard]] bool isRunning() const override
    {
        return proc_.state() == QProcess::Running;
    }

    void kill() override
    {
        proc_.kill();
    }

  private:
    QProcess proc_{};
};

void Process::createImpl()
{
    impl_ = std::make_shared<ProcessImplWin32>();
}

} // namespace oid
