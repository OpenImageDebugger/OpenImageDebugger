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

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "process_impl.h"

namespace oid
{

Process::Process()
{
    createImpl();
}

void Process::start(std::vector<std::string>& command) const
{
    impl_->start(command);
}

bool Process::isRunning() const
{
    return impl_->isRunning();
}

void Process::kill() const
{
    impl_->kill();
}

void Process::waitForStart() const
{
    const int timeout_ms  = 5000;
    const auto start_time = std::chrono::steady_clock::now();

    while (!impl_->isRunning()) {
        const auto elapsed = std::chrono::steady_clock::now() - start_time;
        const auto elapsed_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(elapsed)
                .count();

        if (elapsed_ms > timeout_ms) {
            std::cerr
                << "[OpenImageDebugger] Warning: Process start timeout after "
                << timeout_ms << "ms. Continuing anyway." << std::endl;
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

} // namespace oid
