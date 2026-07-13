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

#ifndef HOST_CLI_OPTIONS_H_
#define HOST_CLI_OPTIONS_H_

#include <string>
#include <vector>

namespace oid::host {

// Command-line options accepted by the native ImGui frontend.
struct CliOptions {
    std::string hostname{"127.0.0.1"};
    int port{9588};
    std::vector<std::string> open_files;
};

// Parses argv into CliOptions. Recognized flags: `--host H`; `--port N` /
// `-p N` (via std::atoi -- invalid or non-positive input leaves the
// default); repeatable `-o PATH` / `--open PATH` (each occurrence appends to
// open_files). Unknown arguments are ignored; a trailing `-o`/`--open`/
// `--host`/`--port`/`-p` with no following value is ignored.
[[nodiscard]] CliOptions parse_cli(int argc, const char* const* argv);

} // namespace oid::host

#endif // HOST_CLI_OPTIONS_H_
