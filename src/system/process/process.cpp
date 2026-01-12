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
#include <fstream>
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
    // #region agent log
    std::ofstream log_file_waitForStart_entry(
        "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
        std::ios::app);
    if (log_file_waitForStart_entry.is_open()) {
        log_file_waitForStart_entry
            << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H11","location":"process.cpp:60","message":"Process_waitForStart_entry","data":{},"timestamp":)"
            << std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count()
            << "}\n";
        log_file_waitForStart_entry.close();
    }
    // #endregion

    // Add timeout to prevent infinite hang - especially important in LLDB's
    // embedded Python where there may be no event loop
    const int timeout_ms  = 5000; // 5 second timeout
    const auto start_time = std::chrono::steady_clock::now();
    int iteration_count = 0;

    while (!impl_->isRunning()) {
        iteration_count++;
        const auto elapsed = std::chrono::steady_clock::now() - start_time;
        const auto elapsed_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(elapsed)
                .count();

        // #region agent log
        std::ofstream log_file_waitForStart_loop(
            "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
            std::ios::app);
        if (log_file_waitForStart_loop.is_open()) {
            log_file_waitForStart_loop
                << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H11","location":"process.cpp:75","message":"Process_waitForStart_loop","data":{"iteration":)"
                << iteration_count << R"(,"elapsed_ms":)" << elapsed_ms
                << R"(,"timeout_ms":)" << timeout_ms << R"(},"timestamp":)"
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()
                << "}\n";
            log_file_waitForStart_loop.close();
        }
        // #endregion

        if (elapsed_ms > timeout_ms) {
            // #region agent log
            std::ofstream log_file_waitForStart_timeout(
                "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
                std::ios::app);
            if (log_file_waitForStart_timeout.is_open()) {
                log_file_waitForStart_timeout
                    << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H11","location":"process.cpp:95","message":"Process_waitForStart_timeout","data":{"elapsed_ms":)"
                    << elapsed_ms << R"(,"timeout_ms":)" << timeout_ms
                    << R"(},"timestamp":)"
                    << std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count()
                    << "}\n";
                log_file_waitForStart_timeout.close();
            }
            // #endregion

            // Timeout - process didn't start in time
            // This can happen in LLDB's embedded Python context where there's
            // no event loop to process process signals
            std::cerr
                << "[OpenImageDebugger] Warning: Process start timeout after "
                << timeout_ms << "ms. Continuing anyway." << std::endl;
            break;
        }

        // Small sleep to avoid busy-waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // #region agent log
    std::ofstream log_file_waitForStart_exit(
        "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
        std::ios::app);
    if (log_file_waitForStart_exit.is_open()) {
        log_file_waitForStart_exit
            << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H11","location":"process.cpp:110","message":"Process_waitForStart_exit","data":{"iteration_count":)"
            << iteration_count << R"(,"is_running":)"
            << (impl_->isRunning() ? "true" : "false") << R"(},"timestamp":)"
            << std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count()
            << "}\n";
        log_file_waitForStart_exit.close();
    }
    // #endregion
}

} // namespace oid
