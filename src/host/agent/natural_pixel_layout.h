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

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <string_view>

#include "host/ui/buffer_model.h"

namespace oid::host::agent {

// The three pixel layouts set_channel(name, index, _) installs to isolate a
// single channel for display (index 0/1/2 -> R/G/B; Buffer::set_pixel_layout()
// then swizzles the shader to read only that duplicated component). The
// single source of truth for what counts as an isolation swizzle:
// isolated_layout() (native_view_model.cpp, which installs these) and
// is_isolation_layout() below (which recognizes them) both read this same
// array, so the two can never drift apart.
inline constexpr std::array<const char*, 3> ISOLATION_LAYOUTS{
    "rrra", "ggga", "bbba"};

// Whether `layout` is one of the three isolation swizzles above.
[[nodiscard]] inline bool is_isolation_layout(const std::string_view layout) {
    return std::ranges::any_of(
        ISOLATION_LAYOUTS, [layout](const char* iso) { return layout == iso; });
}

// The outcome of deciding what set_channel(-1, _) ("all") should do to the
// live buffer.
struct NaturalPixelLayoutResult {
    // The layout to apply, or nullopt to leave the buffer's current layout
    // untouched.
    std::optional<std::string> layout;
    // True only when `layout` is populated because the buffer's current
    // layout was itself an isolation swizzle: worth logging on its own,
    // since clearing that swizzle (not just avoiding a guess) is what makes
    // "all channels" actually exit isolation for a record with no valid
    // layout of its own.
    bool cleared_isolation = false;
};

// Decides what set_channel(-1, _) ("all") should do to the live buffer,
// given its model record and the layout the buffer currently renders with.
//
// A record's own declared layout is used whenever it is valid. Otherwise,
// the record's kind (and, for a DEBUGGER_SYMBOL record whose declared
// layout is invalid, the buffer's current layout) is what tells a
// legitimate "no layout to declare" apart from corruption:
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
// - A DEBUGGER_SYMBOL record with more than one channel and an invalid
//   layout is corrupt data, not a format choice, unless the buffer's
//   current layout is itself an isolation swizzle: leaving that installed
//   would contradict the very "all channels" switch being requested, so the
//   default is restored instead, loudly (the caller names which swizzle was
//   cleared). Otherwise, the live buffer may already be rendering a valid
//   layout of its own (set explicitly via the Format combo, or preserved
//   across a replot by Buffer::configure()'s own guard) that the record
//   just fails to reflect: post-fix, IpcClient::resolve_pixel_layout
//   guarantees a multi-channel DEBUGGER_SYMBOL record is always either
//   valid or already defaulted, so an invalid one reaching here with a
//   non-isolated current layout really is residue, and nullopt tells the
//   caller to leave it alone rather than stamp a guessed default over it.
//
// Kept as a free function, independent of BufferRecord's Buffer/Stage/GL
// neighbors, so it is testable without the render-thread-bound
// NativeViewModel (mirrors wire_buffer_type.h's rationale).
[[nodiscard]] inline NaturalPixelLayoutResult
natural_pixel_layout(const BufferRecord& record,
                     const std::string_view current_layout) {
    if (is_valid_pixel_layout(record.pixel_layout)) {
        return {record.pixel_layout, false};
    }
    if (record.kind == BufferKind::LOCAL_FILE || record.channels == 1) {
        return {std::string(DEFAULT_PIXEL_LAYOUT), false};
    }
    if (is_isolation_layout(current_layout)) {
        return {std::string(DEFAULT_PIXEL_LAYOUT), true};
    }
    return {std::nullopt, false};
}

} // namespace oid::host::agent

#endif // HOST_AGENT_NATURAL_PIXEL_LAYOUT_H_
