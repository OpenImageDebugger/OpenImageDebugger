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

#include "platform/app_services.h"

#include <array>

#include <nfd.h>

namespace oid::platform {

namespace {

// Copies every path out of a successful NFD multi-select result, freeing each
// NFD-owned string as it goes. Kept separate so request_open_files stays flat.
std::vector<std::string> collect_paths(const nfdpathset_t* path_set) {
    std::vector<std::string> paths;

    nfdpathsetsize_t count = 0;
    if (NFD_PathSet_GetCount(path_set, &count) != NFD_OKAY) {
        return paths;
    }

    for (nfdpathsetsize_t i = 0; i < count; ++i) {
        nfdu8char_t* path = nullptr;
        if (NFD_PathSet_GetPathU8(path_set, i, &path) == NFD_OKAY &&
            path != nullptr) {
            paths.emplace_back(path);
            NFD_PathSet_FreePathU8(path);
        }
    }

    return paths;
}

} // namespace

// Deliberately unparented: nfd-extended's parented dialogs need a
// platform-specific window handle (nfd_glfw3.h on GLFW builds), which this
// seam does not thread through. `window` is accepted for interface symmetry
// with callers that hold a GLFWwindow* (and possible future use), but the
// dialog is shown without a parent either way.
std::vector<std::string> request_open_files(GLFWwindow* /*window*/) {
    if (NFD_Init() != NFD_OKAY) {
        return {};
    }

    const std::array<nfdu8filteritem_t, 2> filters{{
        {"Images", "png,jpg,jpeg,bmp,tga,gif,psd,hdr,ppm,pgm,pnm"},
        {"NumPy array", "npy"},
    }};

    nfdopendialogu8args_t args{};
    args.filterList = filters.data();
    args.filterCount = static_cast<nfdfiltersize_t>(filters.size());
    args.defaultPath = nullptr;
    args.parentWindow = {};

    std::vector<std::string> paths;
    if (const nfdpathset_t* path_set = nullptr;
        NFD_OpenDialogMultipleU8_With(&path_set, &args) == NFD_OKAY &&
        path_set != nullptr) {
        paths = collect_paths(path_set);
        NFD_PathSet_Free(path_set);
    }

    NFD_Quit();

    return paths;
}

void register_file_open_sink(oid::host::IpcBufferModel& /*model*/) {
    // Native opens files itself (OS dialog + the frame-loop file-open queue);
    // there is no external byte sink to register.
}

} // namespace oid::platform
