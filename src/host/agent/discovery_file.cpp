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

#include "host/agent/discovery_file.h"

#include <cerrno>
#include <cstdlib>
#include <format>
#include <string_view>
#include <system_error>
#include <vector>

#ifndef _WIN32
#include <fcntl.h>
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>
#else
#include <fstream>
#include <windows.h> // NOSONAR
#endif

namespace oid::host::agent {

namespace {

// Password db, not $HOME: a stripped-env subprocess must resolve the same dir.
std::filesystem::path home_dir() {
#ifdef _WIN32
    if (const char* local = std::getenv("LOCALAPPDATA");
        local && *local) { // NOSONAR
        return std::filesystem::path{local};
    }
    if (const char* profile = std::getenv("USERPROFILE"); // NOSONAR
        profile != nullptr && *profile != '\0') {
        return std::filesystem::path{profile} / "AppData" / "Local";
    }
    return {};
#else
    // NOSONAR(cpp:S1912): getpwuid_r fills our buffer, not shared static state.
    passwd pwd{};
    passwd* result = nullptr;
    long n = ::sysconf(_SC_GETPW_R_SIZE_MAX);
    if (n <= 0) {
        n = 16384;
    }
    if (std::vector<char> buf(static_cast<std::size_t>(n));
        getpwuid_r(getuid(), &pwd, buf.data(), buf.size(), &result) == 0 &&
        result != nullptr && result->pw_dir != nullptr &&
        *result->pw_dir != '\0') {
        return std::filesystem::path{result->pw_dir};
    }
    // No passwd entry: fall back to $HOME, else signal "no home" so the caller
    // uses a per-uid temp dir.
    if (const char* home = std::getenv("HOME"); home && *home) { // NOSONAR
        return std::filesystem::path{home};
    }
    return {};
#endif
}

std::filesystem::path base_discovery_dir() {
    if (const char* override_dir = std::getenv("OID_AGENT_DIR"); // NOSONAR
        override_dir != nullptr && *override_dir != '\0') {
        return std::filesystem::path{override_dir};
    }
    // Stable across launch contexts, and agrees with oid-mcp's discovery.py.
#ifdef _WIN32
    if (const std::filesystem::path home = home_dir(); !home.empty()) {
        return home / "oid-agent";
    }
    // No LOCALAPPDATA/USERPROFILE: %TEMP% is already per-user.
    return std::filesystem::temp_directory_path() / "oid-agent"; // NOSONAR
#else
    if (const std::filesystem::path home = home_dir(); !home.empty()) {
        return home / ".oid-agent";
    }
    // No passwd entry and no $HOME: /tmp is shared, so key the fallback by uid
    // to avoid collisions (a dir owned by another uid would be refused).
    return std::filesystem::temp_directory_path() / // NOSONAR
           std::format("oid-agent-{}", getuid());
#endif
}

} // namespace

std::filesystem::path viewer_discovery_dir() {
    return base_discovery_dir() / "viewer";
}

void prepare_private_dir(const std::filesystem::path& dir,
                         [[maybe_unused]] const bool enforce_mode) {
    // enforce_mode is only consulted in the POSIX chmod path below; on Windows
    // there is no mode to enforce, so the parameter is unused there.
    std::error_code ec;
    if (std::filesystem::is_symlink(dir, ec)) {
        throw DiscoveryError("discovery directory is a symlink: " +
                             dir.string());
    }

    [[maybe_unused]] const bool created =
        std::filesystem::create_directories(dir, ec);
    if (std::error_code stat_ec;
        ec && !std::filesystem::is_directory(dir, stat_ec)) {
        throw DiscoveryError("failed to create discovery directory " +
                             dir.string() + ": " + ec.message());
    }

#ifndef _WIN32
    // Work on the dir's own fd, so a symlink swap cannot race it (CWE-362).
    const int dir_fd =
        open(dir.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (dir_fd < 0) {
        throw DiscoveryError(
            "failed to open discovery directory (or it is a symlink): " +
            dir.string());
    }
    struct stat info{};
    const bool stat_ok = fstat(dir_fd, &info) == 0;
    const bool owned_by_us = stat_ok && info.st_uid == getuid();
    // A dir this call created gets 0700 for oid-mcp's private-dir trust gate.
    // Skipping fchmod on a pre-existing dir spares a CWD given as the base.
    const bool chmod_ok = !(enforce_mode || created) ||
                          (owned_by_us && fchmod(dir_fd, S_IRWXU) == 0);
    close(dir_fd);
    if (!stat_ok) {
        throw DiscoveryError("failed to stat discovery directory: " +
                             dir.string());
    }
    if (!owned_by_us) {
        throw DiscoveryError("discovery directory is owned by another user: " +
                             dir.string());
    }
    if (!chmod_ok) {
        throw DiscoveryError("failed to chmod discovery directory: " +
                             dir.string());
    }
#endif
    // Windows enforces no mode model, so an OID_AGENT_DIR override at a
    // world-accessible path is NOT rejected; restricting the directory ACL to
    // the current user is a tracked Windows-hardening follow-up.
}

void write_discovery_atomic(const std::filesystem::path& path,
                            std::string_view contents) {
    const std::filesystem::path dir = path.parent_path();

#ifndef _WIN32
    std::string tmp_template =
        (dir / (path.filename().string() + ".XXXXXX")).string();
    std::vector name_buf(tmp_template.begin(), tmp_template.end());
    name_buf.push_back('\0');

    const int fd = ::mkstemp(name_buf.data());
    if (fd < 0) {
        throw DiscoveryError("failed to create temp discovery file in " +
                             dir.string());
    }
    // mkstemp leaks this token fd to a fork+exec; mkostemp is not on macOS.
    (void)fcntl(fd, F_SETFD, FD_CLOEXEC);
    const std::filesystem::path tmp_path{name_buf.data()};

    bool write_failed = false;
    std::size_t written = 0;
    while (written < contents.size()) {
        const ssize_t n =
            ::write(fd, contents.data() + written, contents.size() - written);
        if (n < 0 && errno == EINTR) {
            continue;
        }
        if (n <= 0) {
            write_failed = true;
            break;
        }
        written += static_cast<std::size_t>(n);
    }
    // fsync before rename: ext4 delayed alloc can publish a zero-length file.
    if (!write_failed && fsync(fd) != 0) {
        write_failed = true;
    }
    if (close(fd) != 0) {
        write_failed = true;
    }

    if (write_failed) {
        std::error_code remove_ec;
        std::filesystem::remove(tmp_path, remove_ec);
        throw DiscoveryError("failed to write temp discovery file: " +
                             tmp_path.string());
    }

    std::error_code rename_ec;
    std::filesystem::rename(tmp_path, path, rename_ec);
    if (rename_ec) {
        std::error_code remove_ec;
        std::filesystem::remove(tmp_path, remove_ec);
        throw DiscoveryError("failed to publish discovery file " +
                             path.string() + ": " + rename_ec.message());
    }
#else
    const std::filesystem::path tmp_path =
        dir / (path.filename().string() + ".tmp");
    {
        std::ofstream out(tmp_path, std::ios::binary | std::ios::trunc);
        if (!out) {
            throw DiscoveryError("failed to create temp discovery file: " +
                                 tmp_path.string());
        }
        out.write(contents.data(),
                  static_cast<std::streamsize>(contents.size()));
        if (!out) {
            out.close();
            std::error_code remove_ec;
            std::filesystem::remove(tmp_path, remove_ec);
            throw DiscoveryError("failed to write temp discovery file: " +
                                 tmp_path.string());
        }
    }
    if (MoveFileExW(
            tmp_path.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING) == 0) {
        // Don't leak the predictable-named temp file (which holds the token)
        // into the discovery directory when the publish rename fails.
        std::error_code remove_ec;
        std::filesystem::remove(tmp_path, remove_ec);
        throw DiscoveryError("failed to publish discovery file: " +
                             path.string());
    }
#endif
}

} // namespace oid::host::agent
