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

#ifndef PLATFORM_APP_NAP_H_
#define PLATFORM_APP_NAP_H_

namespace oid::platform {

#if defined(__APPLE__)
// Registers an NSProcessInfo activity while the agent endpoint is up, so
// App Nap cannot stretch the FramePacer's timers when the viewer is
// backgrounded. Idempotent; end without begin is a no-op. Not thread-safe:
// both functions are for main()'s startup/shutdown path on the GL thread --
// add external synchronization before calling them from anywhere else.
void begin_agent_activity();
void end_agent_activity();
#else
inline void begin_agent_activity() {
    // No-op: App Nap is a macOS-only mechanism; no other platform throttles
    // the process in a way this assertion could lift.
}
inline void end_agent_activity() {
    // No-op: nothing is ever registered on non-macOS platforms.
}
#endif

} // namespace oid::platform

#endif // PLATFORM_APP_NAP_H_
