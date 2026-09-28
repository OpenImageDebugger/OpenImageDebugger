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

#ifndef TESTS_HOST_IO_NPY_TEST_BLOB_H_
#define TESTS_HOST_IO_NPY_TEST_BLOB_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace oid::test {

// Build a minimal v1 .npy blob: magic, version 1.0, header, payload.
inline std::vector<std::byte> make_npy(const std::string& descr,
                                       const bool fortran_order,
                                       const std::vector<int>& shape,
                                       const std::vector<std::byte>& payload) {
    std::string shape_str = "(";
    for (std::size_t i = 0; i < shape.size(); ++i) {
        shape_str += std::to_string(shape[i]);
        shape_str += ",";
        if (i + 1 < shape.size()) {
            shape_str += " ";
        }
    }
    shape_str += ")";

    std::string dict = "{'descr': '" + descr + "', 'fortran_order': " +
                       (fortran_order ? "True" : "False") +
                       ", 'shape': " + shape_str + ", }";

    // Pad so that (10 + header_len) is a multiple of 64; header ends in '\n'.
    const std::size_t unpadded = 10 + dict.size() + 1;
    const std::size_t padded = (unpadded + 63) / 64 * 64;
    dict.append(padded - unpadded, ' ');
    dict.push_back('\n');

    const auto header_len = static_cast<std::uint16_t>(dict.size());

    std::vector<std::byte> blob;
    constexpr std::array<unsigned char, 6> magic = {
        0x93, 'N', 'U', 'M', 'P', 'Y'};
    for (unsigned char c : magic) {
        blob.push_back(static_cast<std::byte>(c));
    }
    blob.push_back(static_cast<std::byte>(1)); // major
    blob.push_back(static_cast<std::byte>(0)); // minor
    blob.push_back(static_cast<std::byte>(header_len & 0xFF));
    blob.push_back(static_cast<std::byte>(header_len >> 8 & 0xFF));
    for (const char c : dict) {
        blob.push_back(static_cast<std::byte>(static_cast<unsigned char>(c)));
    }
    blob.insert(blob.end(), payload.begin(), payload.end());
    return blob;
}

inline std::vector<std::byte> u8_payload(const std::size_t n) {
    std::vector<std::byte> p(n);
    for (std::size_t i = 0; i < n; ++i) {
        p[i] = static_cast<std::byte>(i & 0xFF);
    }
    return p;
}

} // namespace oid::test

#endif // TESTS_HOST_IO_NPY_TEST_BLOB_H_
