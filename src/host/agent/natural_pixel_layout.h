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

#ifndef HOST_AGENT_NATURAL_PIXEL_LAYOUT_H_
#define HOST_AGENT_NATURAL_PIXEL_LAYOUT_H_

#include <optional>
#include <string>

#include "host/ui/buffer_model.h"

namespace oid::host::agent {

// The layout set_channel(-1, _) ("all") should restore on the live buffer, or
// nullopt when there is nothing valid to restore and the caller must leave
// the buffer's current layout untouched instead of guessing one.
//
// A record's own declared layout is used whenever it is valid. Otherwise,
// the record's kind is what tells a legitimate "no layout to declare" apart
// from corruption, not its channel count alone:
//
// - A LOCAL_FILE record (opened directly from a file, never wire-validated)
//   gets DEFAULT_PIXEL_LAYOUT whatever its channel count. layout_for_channels()
//   (file_buffer_loader.cpp) legitimately returns an empty layout for any
//   non-4-channel image, not only single-channel ones (a plain 3-channel RGB
//   file is the common case), and main.cpp's file-open path upserts straight
//   into the model, bypassing IpcClient::resolve_pixel_layout entirely, so
//   nothing else ever fills this in. The default is exactly what such a
//   buffer already renders with, not a guess.
// - A DEBUGGER_SYMBOL single-channel record gets the same default: channel
//   order is meaningless for one channel (shader_pixel_layout.h always
//   renders a single-channel buffer from red), so an invalid (typically
//   empty) layout here is not a corruption either.
// - Only a DEBUGGER_SYMBOL record with more than one channel and an invalid
//   layout is corrupt data, not a format choice: post-fix,
//   IpcClient::resolve_pixel_layout guarantees a multi-channel
//   DEBUGGER_SYMBOL record is always either valid or already defaulted, so
//   one reaching here with an invalid layout means the live buffer may
//   already be rendering a valid layout of its own (set explicitly via the
//   Format combo, or preserved across a replot by Buffer::configure()'s own
//   guard) that the record just fails to reflect. Returning nullopt tells
//   the caller to leave it alone rather than stamp a guessed default over
//   it.
//
// Kept as a free function, independent of BufferRecord's Buffer/Stage/GL
// neighbors, so it is testable without the render-thread-bound
// NativeViewModel (mirrors wire_buffer_type.h's rationale).
[[nodiscard]] inline std::optional<std::string>
natural_pixel_layout(const BufferRecord& record) {
    if (is_valid_pixel_layout(record.pixel_layout)) {
        return record.pixel_layout;
    }
    if (record.kind == BufferKind::LOCAL_FILE || record.channels == 1) {
        return std::string(DEFAULT_PIXEL_LAYOUT);
    }
    return std::nullopt;
}

} // namespace oid::host::agent

#endif // HOST_AGENT_NATURAL_PIXEL_LAYOUT_H_
