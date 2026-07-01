/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2026 OpenImageDebugger contributors
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

#ifndef PLATFORM_RENDER_SCHEDULER_H_
#define PLATFORM_RENDER_SCHEDULER_H_

#include <functional>
#include <memory>

namespace oid {

class GLCanvas;

class RenderScheduler {
  public:
    virtual ~RenderScheduler() = default;

    // Runs GL work on the GL thread/context: immediately on native,
    // deferred via QRhi render callback on WASM.
    virtual void run_gl(std::function<void()> task) = 0;
    virtual void run_icon_gl(std::function<void()> task) = 0;
};

namespace platform {
std::unique_ptr<RenderScheduler> make_render_scheduler(GLCanvas& canvas);
} // namespace platform

} // namespace oid

#endif // PLATFORM_RENDER_SCHEDULER_H_
