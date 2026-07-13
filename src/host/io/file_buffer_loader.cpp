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
#include <fstream>
#include <utility>

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

// Decodes stb-supported image bytes into BufferRecordParams.
Expected<BufferRecordParams> decode_stb(std::span<const std::byte> bytes,
                                        std::string variable_name,
                                        std::string display_name) {
    if (bytes.size() > static_cast<std::size_t>(INT32_MAX)) {
        return make_error("image: file too large for decoder");
    }
    const auto* data = reinterpret_cast<const stbi_uc*>(bytes.data());
    const int len = static_cast<int>(bytes.size());

    int width = 0;
    int height = 0;
    int channels = 0;

    BufferRecordParams params;
    params.variable_name = std::move(variable_name);
    params.display_name = std::move(display_name);
    params.transpose = false;

    if (stbi_is_hdr_from_memory(data, len) != 0) {
        float* pixels =
            stbi_loadf_from_memory(data, len, &width, &height, &channels, 0);
        if (pixels == nullptr) {
            return make_error(std::string{"image: "} + stbi_failure_reason());
        }
        const std::size_t count =
            static_cast<std::size_t>(width) * height * channels;
        const auto* raw = reinterpret_cast<const std::byte*>(pixels);
        params.bytes.assign(raw, raw + count * sizeof(float));
        stbi_image_free(pixels);
        params.type = BufferType::FLOAT32;
    } else if (stbi_is_16_bit_from_memory(data, len) != 0) {
        stbi_us* pixels =
            stbi_load_16_from_memory(data, len, &width, &height, &channels, 0);
        if (pixels == nullptr) {
            return make_error(std::string{"image: "} + stbi_failure_reason());
        }
        const std::size_t count =
            static_cast<std::size_t>(width) * height * channels;
        const auto* raw = reinterpret_cast<const std::byte*>(pixels);
        params.bytes.assign(raw, raw + count * sizeof(std::uint16_t));
        stbi_image_free(pixels);
        params.type = BufferType::UNSIGNED_SHORT;
    } else {
        stbi_uc* pixels =
            stbi_load_from_memory(data, len, &width, &height, &channels, 0);
        if (pixels == nullptr) {
            return make_error(std::string{"image: "} + stbi_failure_reason());
        }
        const std::size_t count =
            static_cast<std::size_t>(width) * height * channels;
        const auto* raw = reinterpret_cast<const std::byte*>(pixels);
        params.bytes.assign(raw, raw + count);
        stbi_image_free(pixels);
        params.type = BufferType::UNSIGNED_BYTE;
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
        return make_error("file exceeds " +
                          std::to_string(max_bytes / (1024 * 1024)) +
                          " MB open limit: " + path);
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
