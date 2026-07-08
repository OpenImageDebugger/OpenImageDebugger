/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2026 OpenImageDebugger contributors
 * (https://github.com/OpenImageDebugger/OpenImageDebugger)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

// macOS has no per-window title-bar icon (GLFW reports the feature as
// unavailable), so the only place an application icon can surface at runtime
// is the Dock tile. This translation unit sets it via
// -[NSApplication setApplicationIconImage:], which works for a bare
// executable (no .app bundle required) once GLFW has created the shared
// NSApplication during glfwInit(). Compiled only on Apple platforms and with
// ARC enabled (see src/CMakeLists.txt).

#import <AppKit/AppKit.h>

#include "host/ui/icons/app_icon_png.h"

namespace oid::host
{

void set_macos_dock_icon()
{
    @autoreleasepool {
        NSData* data = [NSData dataWithBytes:oid::host::icons::APP_ICON_PNG
                                      length:oid::host::icons::APP_ICON_PNG_SIZE];
        NSImage* source = [[NSImage alloc] initWithData:data];
        if (source == nil) {
            return;
        }

        // The raw PNG is a full-bleed square. macOS Dock/Launchpad icons follow
        // Apple's icon grid: on a 1024pt canvas the artwork occupies an inset
        // rounded square (~824pt, leaving ~100pt transparent margins) with a
        // continuous-corner radius of ~22.37% of the content edge. Compositing
        // the artwork into that shape makes the Dock tile match the size and
        // rounded silhouette of every other app instead of a raw square.
        constexpr CGFloat CANVAS  = 1024.0;
        constexpr CGFloat MARGIN  = 100.0;
        constexpr CGFloat CONTENT = CANVAS - (2.0 * MARGIN);
        constexpr CGFloat RADIUS  = CONTENT * 0.2237;

        NSImage* icon = [[NSImage alloc] initWithSize:NSMakeSize(CANVAS, CANVAS)];
        [icon lockFocus];
        const NSRect content_rect = NSMakeRect(MARGIN, MARGIN, CONTENT, CONTENT);
        NSBezierPath* clip = [NSBezierPath bezierPathWithRoundedRect:content_rect
                                                            xRadius:RADIUS
                                                            yRadius:RADIUS];
        [clip addClip];
        [source drawInRect:content_rect
                  fromRect:NSZeroRect
                 operation:NSCompositingOperationSourceOver
                  fraction:1.0];
        [icon unlockFocus];

        [NSApp setApplicationIconImage:icon];
    }
}

} // namespace oid::host
