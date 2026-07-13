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

// macOS save-dialog implementation of the request_save_path() seam declared in
// platform/app_services.h. nativefiledialog-extended renders its filter list
// as a native format dropdown on Windows and Linux, but its cocoa save backend
// only collapses the extensions into NSSavePanel.allowedContentTypes and drops
// the friendly labels, so macOS users get no way to choose PNG vs Octave (vs a
// future .npy) -- the first type just wins. AppKit itself supports this via an
// NSSavePanel accessory view holding an NSPopUpButton (the standard idiom
// behind the "File Format" picker in Cocoa "Save As..." panels); this file adds
// it, driven by the export-format registry. Native (non-Emscripten) macOS only;
// every other platform keeps the nfd save dialog in native_file_dialog.cpp.
// Compiled with ARC (see src/CMakeLists.txt).
//
// TODO: this format-picker accessory view really belongs upstream in
// nativefiledialog-extended's cocoa save backend, next to the Windows/Linux
// dropdowns it already renders. Contributing it there would let macOS use the
// plain nfd save dialog like every other platform and retire this file.

#import <AppKit/AppKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include "platform/app_services.h"

#include "host/ui/export_dialog.h"

#include <optional>
#include <span>
#include <string>

// Target/action sink for the format dropdown: when the selection changes, swap
// the panel's single allowed content type so NSSavePanel live-updates the
// extension it enforces and appends. Held alive by the strong local in
// request_save_path() for the duration of the modal run; NSControl.target is a
// non-owning reference.
@interface OidSaveFormatController : NSObject
@property(nonatomic, weak) NSSavePanel* panel;
@property(nonatomic, strong) NSArray<UTType*>* contentTypes;
- (void)formatChanged:(NSPopUpButton*)sender;
@end

@implementation OidSaveFormatController
- (void)formatChanged:(NSPopUpButton*)sender {
    const NSInteger index = [sender indexOfSelectedItem];
    if (index < 0 ||
        static_cast<NSUInteger>(index) >= [self.contentTypes count]) {
        return;
    }
    self.panel.allowedContentTypes = @[ self.contentTypes[index] ];
}
@end

namespace oid::platform {

std::optional<std::string> request_save_path(const std::string& default_dir,
                                             const std::string& default_name) {
    @autoreleasepool {
        // Build the dropdown rows from the export-format registry: friendly
        // labels drive the popup, the parallel content-type and extension
        // lists drive the enforced/appended extension.
        NSMutableArray<NSString*>* labels = [NSMutableArray array];
        NSMutableArray<UTType*>* types = [NSMutableArray array];
        NSMutableArray<NSString*>* extensions = [NSMutableArray array];
        for (const oid::host::ExportFormat& fmt : oid::host::export_formats()) {
            // fmt.extension has a leading dot (".png"); UTType wants "png".
            UTType* type =
                [UTType typeWithFilenameExtension:@(fmt.extension + 1)];
            if (type == nil) {
                continue;
            }
            [labels addObject:@(fmt.label)];
            [types addObject:type];
            [extensions addObject:@(fmt.extension)];
        }
        if ([types count] == 0) {
            return std::nullopt;
        }

        NSSavePanel* panel = [NSSavePanel savePanel];

        OidSaveFormatController* controller =
            [[OidSaveFormatController alloc] init];
        controller.panel = panel;
        controller.contentTypes = types;

        // Start on the first (default) format. Must precede
        // setNameFieldStringValue, which can otherwise override the extension.
        panel.allowedContentTypes = @[ types[0] ];
        panel.extensionHidden = NO;

        // A format picker is only meaningful when there is more than one
        // choice; with a single format the panel already enforces it.
        NSPopUpButton* popup = nil;
        if ([labels count] > 1) {
            popup =
                [[NSPopUpButton alloc] initWithFrame:NSMakeRect(60, 4, 150, 25)
                                           pullsDown:NO];
            [popup addItemsWithTitles:labels];
            popup.target = controller;
            popup.action = @selector(formatChanged:);

            NSTextField* caption =
                [[NSTextField alloc] initWithFrame:NSMakeRect(4, 7, 52, 22)];
            caption.editable = NO;
            caption.bordered = NO;
            caption.bezeled = NO;
            caption.drawsBackground = NO;
            caption.stringValue = @"Format:";

            NSView* accessory =
                [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 220, 34)];
            [accessory addSubview:caption];
            [accessory addSubview:popup];
            panel.accessoryView = accessory;
        }

        if (!default_dir.empty()) {
            panel.directoryURL = [NSURL fileURLWithPath:@(default_dir.c_str())
                                            isDirectory:YES];
        }
        if (!default_name.empty()) {
            panel.nameFieldStringValue = @(default_name.c_str());
        }

        if ([panel runModal] != NSModalResponseOK || panel.URL == nil) {
            return std::nullopt;
        }

        // Force the selected format's extension onto the result rather than
        // trusting the extension NSSavePanel appended: a dynamic UTType (any
        // format without a registered system type, e.g. .oct/.npy) is not
        // guaranteed to append a suffix, and classify_export_format() upstream
        // keys off the extension. The popup index maps 1:1 onto `extensions`.
        const NSUInteger selected =
            popup != nil ? static_cast<NSUInteger>([popup indexOfSelectedItem])
                         : 0;
        NSString* path = panel.URL.path;
        if (selected < [extensions count]) {
            NSString* ext = extensions[selected]; // ".png"
            // Case-insensitive suffix test: a user-typed "IMG.PNG" already
            // carries the extension, so don't append a second one. Anchoring
            // the backwards search to the end makes this a hasSuffix that
            // ignores case, matching classify_export_format() upstream.
            const NSRange tail =
                [path rangeOfString:ext
                            options:NSCaseInsensitiveSearch |
                                    NSBackwardsSearch | NSAnchoredSearch];
            if (tail.location == NSNotFound) {
                path = [path stringByAppendingString:ext];
            }
        }

        const char* utf8 = path.UTF8String;
        if (utf8 == nullptr) {
            return std::nullopt;
        }
        return std::string(utf8);
    }
}

} // namespace oid::platform
