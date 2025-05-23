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

#ifndef SYSTEM_PROCESS_H_
#define SYSTEM_PROCESS_H_

#include <memory>
#include <string>
#include <vector>

namespace oid
{

class ProcessImpl;

class Process final
{
  public:
    Process();

    /**
     * Start the process represented by its path and arguments
     * @param command binary and path and its arguments
     */
    void start(std::vector<std::string>& command) const;

    /**
     * Check if the process is running
     * @return true if running, false otherwise
     */
    [[nodiscard]] bool isRunning() const;

    /**
     * Kill the process
     */
    void kill() const;

    /**
     * Busy waiting until the process starts
     */
    void waitForStart() const;

  private:
    /**
     * Initialize pimpl according to platform
     */
    void createImpl();

    // pimpl idiom
    std::shared_ptr<ProcessImpl> impl_{};
};

} // namespace oid

#endif // #ifndef SYSTEM_PROCESS_H_
