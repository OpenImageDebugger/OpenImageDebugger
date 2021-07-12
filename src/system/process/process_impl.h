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

#ifndef SYSTEM_PROCESS_IMPL_H_
#define SYSTEM_PROCESS_IMPL_H_

#include <string>
#include <vector>

/**
 * Interface to process calling classes
 */
class ProcessImpl {
public:
    virtual ~ProcessImpl() noexcept = default;
    /**
     * Start the process represented by its path and arguments
     * @param command binary and path and its arguments
     */
    virtual void start(const std::vector<std::string>& command) = 0;

    /**
     * Check if the process is running
     * @return true if running, false otherwise
     */
    virtual bool isRunning() const = 0;

    /**
     * Kill the process
     */
    virtual void kill() = 0;
};

#endif // #ifndef SYSTEM_PROCESS_IMPL_H_
