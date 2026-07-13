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

#include "host/cli_options.h"

#include <cstdlib>
#include <cstring>

namespace oid::host {

namespace {

bool matches(const char* arg, const char* a, const char* b) {
    return std::strcmp(arg, a) == 0 || std::strcmp(arg, b) == 0;
}

} // namespace

CliOptions parse_cli(int argc, const char* const* argv) {
    CliOptions options;

    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        const bool has_next = (i + 1) < argc;

        if (matches(arg, "-o", "--open")) {
            if (has_next) {
                options.open_files.emplace_back(argv[++i]);
            }
        } else if (std::strcmp(arg, "--host") == 0) {
            if (has_next) {
                options.hostname = argv[++i];
            }
        } else if (matches(arg, "-p", "--port")) {
            if (has_next) {
                const int parsed = std::atoi(argv[++i]);
                if (parsed > 0) {
                    options.port = parsed;
                }
            }
        }
        // Unknown arguments are ignored.
    }

    return options;
}

} // namespace oid::host
