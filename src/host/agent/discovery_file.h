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

#ifndef HOST_AGENT_DISCOVERY_FILE_H_
#define HOST_AGENT_DISCOVERY_FILE_H_

#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>

namespace oid::host::agent {

// Raised for every discovery-file preparation/publication failure: a
// symlinked directory, an ownership mismatch, or a filesystem I/O error.
class DiscoveryError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

// The "viewer" subdir keeps the flat *.json debugger glob from seeing these.
std::filesystem::path viewer_discovery_dir();

// Only `dir` is checked: a caller needing a hardened parent prepares it too.
// enforce_mode=false still chmods 0700 a dir this call created but spares a
// pre-existing caller-chosen base (the CWD via OID_AGENT_DIR="."). Any failed
// check throws DiscoveryError.
void prepare_private_dir(const std::filesystem::path& dir,
                         bool enforce_mode = true);

// `path`'s parent directory must already exist (see prepare_private_dir).
void write_discovery_atomic(const std::filesystem::path& path,
                            std::string_view contents);

} // namespace oid::host::agent

#endif // HOST_AGENT_DISCOVERY_FILE_H_
