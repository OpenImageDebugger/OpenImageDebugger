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

#include "host/io/file_buffer_loader.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <new>
#include <utility>
#include <vector>

#include <stb_image.h>

#include "host/io/expected.h"
#include "host/io/npy_decode.h"
#include "host/ipc/buffer_decode.h"
#include "ipc/raw_data_decode.h"

namespace oid::host {

namespace {

bool has_npy_magic(std::span<const std::byte> bytes) {
    static constexpr std::array<unsigned char, 6> kMagic{
        0x93, 'N', 'U', 'M', 'P', 'Y'};
    if (bytes.size() < kMagic.size()) {
        return false;
    }
    for (std::size_t i = 0; i < kMagic.size(); ++i) {
        if (std::to_integer<unsigned char>(bytes[i]) != kMagic[i]) {
            return false;
        }
    }
    return true;
}

// "rgba" for 4-channel buffers; empty (single-channel-style, unlabeled) for
// everything else -- the renderer only recognizes the 4-char layout string.
std::string layout_for_channels(int channels) {
    return channels == 4 ? "rgba" : "";
}

// Maps a decoded NumPy array onto the wire-shaped BufferRecordParams that
// make_buffer_record() consumes.
BufferRecordParams params_from_npy(NpyArray npy,
                                   std::string variable_name,
                                   std::string display_name) {
    BufferRecordParams params;
    params.variable_name = std::move(variable_name);
    params.display_name = std::move(display_name);
    params.pixel_layout = layout_for_channels(npy.channels);
    params.transpose = npy.transpose;
    params.width = npy.width;
    params.height = npy.height;
    params.channels = npy.channels;
    params.stride = npy.step;
    params.type = npy.type;
    params.bytes = std::move(npy.bytes);
    return params;
}

// Preflight caps for decoded images, mirroring the renderer's BufferConstants.
// Kept local so this translation unit (compiled for the non-native build too)
// does not pull in the GL-backed buffer.h. A file whose header claims more than
// this is rejected before any pixel memory is allocated.
constexpr int kMaxImageDimension = 131072;                             // 2^17
constexpr std::uint64_t kMaxDecodedBytes = 16ULL * 1024 * 1024 * 1024; // 16 GB

// Number of decoded scalar elements (pixels * channels) in the image.
std::size_t element_count(int width, int height, int channels) {
    return static_cast<std::size_t>(width) * static_cast<std::size_t>(height) *
           static_cast<std::size_t>(channels);
}

// Copies `count` elements of type T (as returned by stb) into a byte vector.
// The end pointer is computed in T-space (pixels + count) and only then cast to
// bytes, so element size never enters the pointer arithmetic.
template <typename T>
std::vector<std::byte> pixels_to_bytes(const T* pixels, std::size_t count) {
    const auto* first = reinterpret_cast<const std::byte*>(pixels);
    const auto* last = reinterpret_cast<const std::byte*>(pixels + count);
    return std::vector<std::byte>(first, last);
}

// Decodes stb-supported image bytes into BufferRecordParams.
Expected<BufferRecordParams> decode_stb(std::span<const std::byte> bytes,
                                        std::string variable_name,
                                        std::string display_name) {
    if (bytes.size() > static_cast<std::size_t>(INT32_MAX)) {
        return make_error("image: file too large for decoder");
    }
    const auto* data = reinterpret_cast<const stbi_uc*>(bytes.data());
    const auto len = static_cast<int>(bytes.size());

    // Preflight the header before decoding: stbi_info reads the dimensions
    // without allocating pixel memory, so a small compressed file that claims
    // enormous dimensions (a decompression bomb) is rejected here instead of
    // triggering a huge allocation during the decode below.
    int width = 0;
    int height = 0;
    int channels = 0;
    if (stbi_info_from_memory(data, len, &width, &height, &channels) == 0) {
        return make_error(std::string{"image: "} + stbi_failure_reason());
    }
    if (width < 1 || height < 1 || channels < 1 || width > kMaxImageDimension ||
        height > kMaxImageDimension) {
        return make_error("image: dimensions out of range");
    }
    // Upper-bound the decode with the widest element (float, 4 bytes) using
    // 64-bit math so the check is well-formed on 32-bit builds.
    if (const std::uint64_t decoded_bytes =
            static_cast<std::uint64_t>(width) *
            static_cast<std::uint64_t>(height) *
            static_cast<std::uint64_t>(channels) * sizeof(float);
        decoded_bytes > kMaxDecodedBytes) {
        return make_error("image: decoded dimensions exceed the size limit");
    }

    BufferRecordParams params;
    params.variable_name = std::move(variable_name);
    params.display_name = std::move(display_name);
    params.transpose = false;

    // Even a bounded decode can fail to allocate; turn a bad_alloc into an
    // error instead of letting it escape and crash the viewer.
    try {
        if (stbi_is_hdr_from_memory(data, len) != 0) {
            float* pixels = stbi_loadf_from_memory(
                data, len, &width, &height, &channels, 0);
            if (pixels == nullptr) {
                return make_error(std::string{"image: "} +
                                  stbi_failure_reason());
            }
            params.bytes =
                pixels_to_bytes(pixels, element_count(width, height, channels));
            stbi_image_free(pixels);
            params.type = BufferType::FLOAT32;
        } else if (stbi_is_16_bit_from_memory(data, len) != 0) {
            stbi_us* pixels = stbi_load_16_from_memory(
                data, len, &width, &height, &channels, 0);
            if (pixels == nullptr) {
                return make_error(std::string{"image: "} +
                                  stbi_failure_reason());
            }
            params.bytes =
                pixels_to_bytes(pixels, element_count(width, height, channels));
            stbi_image_free(pixels);
            params.type = BufferType::UNSIGNED_SHORT;
        } else {
            stbi_uc* pixels =
                stbi_load_from_memory(data, len, &width, &height, &channels, 0);
            if (pixels == nullptr) {
                return make_error(std::string{"image: "} +
                                  stbi_failure_reason());
            }
            params.bytes =
                pixels_to_bytes(pixels, element_count(width, height, channels));
            stbi_image_free(pixels);
            params.type = BufferType::UNSIGNED_BYTE;
        }
    } catch (const std::bad_alloc&) {
        return make_error("image: not enough memory to decode");
    }

    params.width = width;
    params.height = height;
    params.channels = channels;
    params.stride = width;
    params.pixel_layout = layout_for_channels(channels);
    return params;
}

} // namespace

Expected<BufferRecord> decode_file_bytes(std::span<const std::byte> bytes,
                                         std::string variable_name,
                                         std::string display_name) {
    if (bytes.empty()) {
        return make_error("file is empty");
    }

    Expected<BufferRecordParams> params =
        has_npy_magic(bytes) ? [&]() -> Expected<BufferRecordParams> {
        Expected<NpyArray> npy = decode_npy(bytes);
        if (!npy) {
            return make_error(npy.error());
        }
        return params_from_npy(
            std::move(*npy), std::move(variable_name), std::move(display_name));
    }()
        : decode_stb(bytes, std::move(variable_name), std::move(display_name));
    if (!params) {
        return make_error(params.error());
    }

    BufferRecord record = make_buffer_record(std::move(*params));
    record.kind = BufferKind::LOCAL_FILE;
    return record;
}

Expected<BufferRecord> load_buffer_from_file(const std::string& path,
                                             std::size_t max_bytes) {
    std::error_code ec;
    const std::filesystem::path fs_path{path};

    const auto size = std::filesystem::file_size(fs_path, ec);
    if (ec) {
        return make_error("cannot stat file: " + path);
    }
    if (size > max_bytes) {
        return make_error(std::format("file exceeds {} MB open limit: {}",
                                      max_bytes / (1024 * 1024),
                                      path));
    }

    std::ifstream stream{fs_path, std::ios::binary};
    if (!stream) {
        return make_error("cannot open file: " + path);
    }
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    stream.read(reinterpret_cast<char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
    if (!stream) {
        return make_error("cannot read file: " + path);
    }

    const std::string canonical =
        std::filesystem::weakly_canonical(fs_path, ec).string();
    const std::string variable_name = ec ? path : canonical;
    const std::string display_name = fs_path.filename().string();

    return decode_file_bytes(bytes, variable_name, display_name);
}

} // namespace oid::host
