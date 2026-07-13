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

#include "host/io/npy_decode.h"

#include <array>
#include <charconv>
#include <cstdint>
#include <string_view>

namespace oid {

namespace {

std::uint16_t read_u16le(std::span<const std::byte> data, std::size_t offset) {
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(
            std::to_integer<std::uint8_t>(data[offset])) |
        (static_cast<std::uint16_t>(
             std::to_integer<std::uint8_t>(data[offset + 1]))
         << 8));
}

std::uint32_t read_u32le(std::span<const std::byte> data, std::size_t offset) {
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < 4; ++i) {
        value |= static_cast<std::uint32_t>(
                     std::to_integer<std::uint8_t>(data[offset + i]))
                 << (8 * i);
    }
    return value;
}

// Map a NumPy dtype descriptor (e.g. "<f4", "|u1") to a BufferType and
// itemsize. Returns nullopt for anything unsupported.
struct DType {
    BufferType type;
    int itemsize;
};

Expected<DType> map_dtype(std::string_view descr) {
    if (descr.size() < 3) {
        return make_error("npy: dtype descriptor too short: '" +
                          std::string{descr} + "'");
    }
    const char byteorder = descr[0];
    const char kind = descr[1];
    const std::string_view size_text = descr.substr(2);

    if (byteorder == '>') {
        return make_error("npy: big-endian dtypes are not supported");
    }
    // '<', '|', '=' are all treated as little-endian on our LE targets.

    int itemsize = 0;
    const auto [ptr, ec] = std::from_chars(
        size_text.data(), size_text.data() + size_text.size(), itemsize);
    if (ec != std::errc{} || ptr != size_text.data() + size_text.size()) {
        return make_error("npy: unparseable dtype itemsize in '" +
                          std::string{descr} + "'");
    }

    if (kind == 'u' && itemsize == 1) {
        return DType{BufferType::UNSIGNED_BYTE, 1};
    }
    if (kind == 'u' && itemsize == 2) {
        return DType{BufferType::UNSIGNED_SHORT, 2};
    }
    if (kind == 'i' && itemsize == 2) {
        return DType{BufferType::SHORT, 2};
    }
    if (kind == 'i' && itemsize == 4) {
        return DType{BufferType::INT32, 4};
    }
    if (kind == 'f' && itemsize == 4) {
        return DType{BufferType::FLOAT32, 4};
    }
    if (kind == 'f' && itemsize == 8) {
        return DType{BufferType::FLOAT64, 8};
    }
    return make_error("npy: unsupported dtype '" + std::string{descr} + "'");
}

// Find value of key like 'descr' between quotes.
Expected<std::string_view> extract_quoted(std::string_view header,
                                          std::string_view key) {
    const std::size_t key_pos = header.find(key);
    if (key_pos == std::string_view::npos) {
        return make_error("npy: missing key '" + std::string{key} + "'");
    }
    std::size_t quote = header.find('\'', key_pos + key.size());
    if (quote == std::string_view::npos) {
        return make_error("npy: malformed value for '" + std::string{key} +
                          "'");
    }
    const std::size_t end = header.find('\'', quote + 1);
    if (end == std::string_view::npos) {
        return make_error("npy: unterminated value for '" + std::string{key} +
                          "'");
    }
    return header.substr(quote + 1, end - (quote + 1));
}

Expected<bool> extract_fortran(std::string_view header) {
    const std::size_t key_pos = header.find("'fortran_order'");
    if (key_pos == std::string_view::npos) {
        return make_error("npy: missing 'fortran_order'");
    }
    if (header.find("True", key_pos) != std::string_view::npos &&
        header.find("True", key_pos) < header.find(',', key_pos)) {
        return true;
    }
    return false;
}

Expected<std::vector<int>> extract_shape(std::string_view header) {
    const std::size_t key_pos = header.find("'shape'");
    if (key_pos == std::string_view::npos) {
        return make_error("npy: missing 'shape'");
    }
    const std::size_t open = header.find('(', key_pos);
    const std::size_t close = header.find(')', open);
    if (open == std::string_view::npos || close == std::string_view::npos) {
        return make_error("npy: malformed shape tuple");
    }
    const std::string_view inner = header.substr(open + 1, close - (open + 1));

    std::vector<int> dims;
    std::size_t i = 0;
    while (i < inner.size()) {
        while (i < inner.size() && (inner[i] == ' ' || inner[i] == ',')) {
            ++i;
        }
        if (i >= inner.size()) {
            break;
        }
        int value = 0;
        const auto [ptr, ec] = std::from_chars(
            inner.data() + i, inner.data() + inner.size(), value);
        if (ec != std::errc{}) {
            return make_error("npy: unparseable shape dimension");
        }
        dims.push_back(value);
        i = static_cast<std::size_t>(ptr - inner.data());
    }
    return dims;
}

} // namespace

Expected<NpyArray> decode_npy(std::span<const std::byte> data) {
    static constexpr std::array<unsigned char, 6> kMagic = {
        0x93, 'N', 'U', 'M', 'P', 'Y'};

    if (data.size() < 10) {
        return make_error("npy: buffer too small for header");
    }
    for (std::size_t i = 0; i < kMagic.size(); ++i) {
        if (std::to_integer<std::uint8_t>(data[i]) != kMagic[i]) {
            return make_error("npy: bad magic");
        }
    }

    const std::uint8_t major = std::to_integer<std::uint8_t>(data[6]);

    std::size_t header_len = 0;
    std::size_t data_off = 0;
    if (major == 1) {
        header_len = read_u16le(data, 8);
        data_off = 10;
    } else if (major >= 2) {
        if (data.size() < 12) {
            return make_error("npy: buffer too small for v2 header");
        }
        header_len = read_u32le(data, 8);
        data_off = 12;
    } else {
        return make_error("npy: unsupported major version");
    }

    if (header_len > 65536) {
        return make_error("npy: header too large");
    }
    if (data_off + header_len > data.size()) {
        return make_error("npy: header extends past buffer");
    }

    const std::string_view header{
        reinterpret_cast<const char*>(data.data() + data_off), header_len};

    const auto descr = extract_quoted(header, "'descr'");
    if (!descr) {
        return make_error(descr.error());
    }
    const auto dtype = map_dtype(*descr);
    if (!dtype) {
        return make_error(dtype.error());
    }
    const auto fortran = extract_fortran(header);
    if (!fortran) {
        return make_error(fortran.error());
    }
    const auto shape = extract_shape(header);
    if (!shape) {
        return make_error(shape.error());
    }

    NpyArray out;
    out.type = dtype->type;

    const std::vector<int>& dims = *shape;
    if (dims.size() == 2) {
        if (*fortran) {
            out.width = dims[0];
            out.height = dims[1];
            out.step = dims[0];
            out.transpose = true;
        } else {
            out.height = dims[0];
            out.width = dims[1];
            out.step = dims[1];
            out.transpose = false;
        }
        out.channels = 1;
    } else if (dims.size() == 3) {
        if (*fortran) {
            return make_error(
                "npy: Fortran-order 3-D arrays are not supported");
        }
        out.height = dims[0];
        out.width = dims[1];
        out.channels = dims[2];
        out.step = dims[1];
        out.transpose = false;
    } else {
        return make_error("npy: only 2-D and 3-D arrays are supported");
    }

    if (out.width < 1 || out.width > 131072 || out.height < 1 ||
        out.height > 131072) {
        return make_error("npy: width/height out of range");
    }
    if (out.channels < 1 || out.channels > 4) {
        return make_error("npy: channel count out of range");
    }

    std::size_t element_count = 1;
    for (int d : dims) {
        if (d < 0) {
            return make_error("npy: negative shape dimension");
        }
        element_count *= static_cast<std::size_t>(d);
    }
    const std::size_t expected_bytes =
        element_count * static_cast<std::size_t>(dtype->itemsize);
    const std::size_t available = data.size() - (data_off + header_len);
    if (available != expected_bytes) {
        return make_error("npy: payload size mismatch");
    }

    const std::byte* payload = data.data() + data_off + header_len;
    out.bytes.assign(payload, payload + expected_bytes);
    return out;
}

} // namespace oid
