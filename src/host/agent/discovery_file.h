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

// Directory holding per-viewer discovery files: <OID_AGENT_DIR> (if set)
// or <home>/.oid-agent, plus a "viewer" subdirectory. That
// subdirectory is what keeps the shipped flat *.json glob (which only
// scans the parent directory for debugger sessions) from ever seeing a
// viewer's discovery file, even though it never looks here regardless.
std::filesystem::path viewer_discovery_dir();

// Ensures `dir` (and its parents) exist as a private directory suitable
// for discovery files. Rejects a symlinked `dir` outright. POSIX: creates
// it mode 0700, then verifies the resulting directory is owned by the
// calling user before chmod'ing it to 0700 (in case it pre-existed with
// looser permissions). Windows: best-effort directory creation only --
// there is no POSIX ownership/mode model to enforce. Throws
// DiscoveryError if any check fails. Only `dir` itself is symlink/owner
// checked -- a caller that needs a hardened parent (e.g. the predictable
// base dir under a shared tempdir) must prepare that parent separately first.
// enforce_mode=false keeps the symlink/owner checks and still chmods to 0700
// a directory this call newly created, but skips the chmod for a pre-existing
// caller-chosen base dir whose (possibly shared, e.g. the CWD) mode must not
// be changed.
void prepare_private_dir(const std::filesystem::path& dir,
                         bool enforce_mode = true);

// Atomically publishes `contents` at `path`. POSIX: writes a mode-0600
// temporary file in path's parent directory (a mkstemp-equivalent unique
// name, so a stale temp file from a crashed run can never collide) and
// renames it into place. Windows: writes a temporary file alongside
// `path` and replaces it with MoveFileEx. `path`'s parent directory must
// already exist (see prepare_private_dir). Throws DiscoveryError on
// failure.
void write_discovery_atomic(const std::filesystem::path& path,
                            std::string_view contents);

} // namespace oid::host::agent

#endif // HOST_AGENT_DISCOVERY_FILE_H_
