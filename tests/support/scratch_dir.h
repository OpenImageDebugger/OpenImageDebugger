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

#ifndef TESTS_SUPPORT_SCRATCH_DIR_H_
#define TESTS_SUPPORT_SCRATCH_DIR_H_

#include <filesystem>
#include <random>
#include <sstream>
#include <stdexcept>

#include <gtest/gtest.h>

namespace oid::test {

// Thrown when scratch_dir() exhausts its unique-name attempts.
struct ScratchDirError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// Fresh, uniquely named directory for one test binary's temp files:
// portable mkdtemp semantics -- unpredictable name, atomic create that
// fails on a squatted name, owner-only permissions. Base comes from
// gtest's TempDir(), which honors TEST_TMPDIR when the CI runner
// provides a per-test private dir.
inline std::filesystem::path scratch_dir() {
    namespace fs = std::filesystem;
    const fs::path base{::testing::TempDir()};
    std::random_device rd;
    for (int attempt = 0; attempt < 16; ++attempt) {
        std::ostringstream name;
        name << "oid_scratch_" << std::hex << rd() << rd();
        const fs::path dir = base / name.str();
        if (fs::create_directory(dir)) { // atomic; false if name taken
            fs::permissions(
                dir, fs::perms::owner_all, fs::perm_options::replace);
            return dir;
        }
    }
    throw ScratchDirError{"scratch_dir: could not create a unique dir"};
}

} // namespace oid::test

#endif // TESTS_SUPPORT_SCRATCH_DIR_H_
