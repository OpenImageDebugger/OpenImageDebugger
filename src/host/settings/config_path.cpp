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

#include "host/settings/config_path.h"

#include <cstdlib>

namespace oid::host {

std::filesystem::path
config_file_path_from_env([[maybe_unused]] const char* xdg_config_home,
                          [[maybe_unused]] const char* home,
                          [[maybe_unused]] const char* appdata) {
    namespace fs = std::filesystem;
    fs::path base;
#if defined(_WIN32)
    base = (appdata && *appdata) ? fs::path{appdata} : fs::path{"."};
#elif defined(__APPLE__)
    base = home && *home ? fs::path{home} / "Library" / "Application Support"
                         : fs::path{"."};
#else // Linux / other POSIX
    if (xdg_config_home && *xdg_config_home) {
        base = fs::path{xdg_config_home};
    } else if (home && *home) {
        base = fs::path{home} / ".config";
    } else {
        base = fs::path{"."};
    }
#endif
    return base / "OpenImageDebugger" / "imgui_settings.json";
}

std::filesystem::path config_file_path() {
    return config_file_path_from_env(std::getenv("XDG_CONFIG_HOME"),
                                     std::getenv("HOME"),
                                     std::getenv("APPDATA"));
}

} // namespace oid::host
