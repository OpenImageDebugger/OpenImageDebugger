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

// Single source of truth for what counts as an isolation swizzle.
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
    // True only when a populated `layout` replaced an isolation swizzle.
    bool cleared_isolation = false;
};

// layout_for_channels() leaves any non-4-channel file empty and the file-open
// path never reaches resolve_pixel_layout; one channel always renders from red
// (shader_pixel_layout.h). Both are layout-less by right, so they default.
// resolve_pixel_layout leaves every other record valid or defaulted, so an
// invalid layout reaching the last branch is residue: nullopt leaves it alone.
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
