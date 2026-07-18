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

#include "buffer_export_core.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <limits>

namespace oid::BufferExporter {

namespace {

template <typename T> float get_multiplier() {
    return 255.0f / static_cast<float>((std::numeric_limits<T>::max)());
}

template <> float get_multiplier<float>() {
    return 255.0f;
}

template <typename T> T get_max_intensity() {
    return (std::numeric_limits<T>::max)();
}

template <> float get_max_intensity<float>() {
    return 1.0f;
}

void repeat_first_channel_into_g_and_b(
    std::array<std::uint8_t, 4>& unformatted_pixel, std::size_t& c) {
    for (; c < 3; ++c) {
        unformatted_pixel[c] = unformatted_pixel[0];
    }
}

template <typename T>
RgbaImage normalize_to_rgba8_impl(const std::uint8_t* data,
                                  const int width,
                                  const int height,
                                  const int channels_i,
                                  const int step,
                                  const std::array<float, 8>& bc_comp,
                                  const char* pixel_layout_str) {
    const auto width_i = static_cast<std::size_t>(width);
    const auto height_i = static_cast<std::size_t>(height);

    auto result = RgbaImage{};
    result.width = width;
    result.height = height;
    result.pixels = std::vector<std::uint8_t>(4 * width_i * height_i);

    auto out_ptr = result.pixels.data();

    const auto color_scale = get_multiplier<T>();
    const auto max_intensity = get_max_intensity<T>();

    auto pixel_layout = std::array<std::uint8_t, 4>{};
    for (int c = 0; c < 4; ++c) {
        switch (pixel_layout_str[c]) {
        case 'r':
            pixel_layout[c] = 0;
            break;
        case 'g':
            pixel_layout[c] = 1;
            break;
        case 'b':
            pixel_layout[c] = 2;
            break;
        case 'a':
            pixel_layout[c] = 3;
            break;
        default:
            assert(!"Unknown pixel layout");
            break;
        }
    }

    auto in_ptr = std::bit_cast<const T*>(data);
    const auto input_stride = channels_i * step;
    auto unformatted_pixel = std::array<std::uint8_t, 4>{};
    const auto channels = static_cast<std::size_t>(channels_i);

    for (std::size_t y = 0; y < height_i; ++y) {
        for (std::size_t x = 0; x < width_i; ++x) {
            const auto col_offset = x * static_cast<std::size_t>(channels_i);
            auto c = std::size_t{0};

            // Perform contrast normalization
            for (; c < channels; ++c) {
                const auto in_val = static_cast<float>(in_ptr[col_offset + c]);

                unformatted_pixel[c] = static_cast<std::uint8_t>(
                    std::clamp((in_val * bc_comp[c] +
                                bc_comp[4 + c] *
                                    static_cast<std::uint8_t>(max_intensity)) *
                                   color_scale,
                               0.f,
                               255.f));
            }

            // Grayscale: Repeat first channel into G and B
            if (channels == 1) {
                repeat_first_channel_into_g_and_b(unformatted_pixel, c);
            }

            // The remaining, non-filled channels will be set to a default value
            for (; c < 4; ++c) {
                constexpr auto default_channel_vals =
                    std::array<std::uint8_t, 4>{0, 0, 0, 255};
                unformatted_pixel[c] = default_channel_vals[c];
            }

            // Reorganize pixel layout according to user provided format
            for (c = 0; c < 4; ++c) {
                out_ptr[pixel_layout[c]] = unformatted_pixel[c];
            }
            out_ptr += 4;
        }

        in_ptr += input_stride;
    }

    return result;
}

template <typename T> std::string get_type_descriptor();

template <> std::string get_type_descriptor<std::uint8_t>() {
    return "uint8";
}

template <> std::string get_type_descriptor<std::uint16_t>() {
    return "uint16";
}

template <> std::string get_type_descriptor<std::int16_t>() {
    return "int16";
}

template <> std::string get_type_descriptor<std::int32_t>() {
    return "int32";
}

template <> std::string get_type_descriptor<float>() {
    return "float";
}

template <typename T>
bool export_octave_impl(const std::uint8_t* data,
                        const int width,
                        const int height,
                        const int channels_i,
                        const int step,
                        const std::string& path) {
    const auto width_i = static_cast<std::size_t>(width);
    const auto height_i = static_cast<std::size_t>(height);

    const auto in_ptr = std::bit_cast<const T*>(data);

    const auto output_path = std::filesystem::path{path};
    auto ofs = std::ofstream{output_path, std::ios::binary};

    ofs << get_type_descriptor<T>() << '\n';

    const auto height_o = static_cast<std::int32_t>(height_i);
    const auto width_o = static_cast<std::int32_t>(width_i);
    const auto channels_o = static_cast<std::int32_t>(channels_i);

    ofs.write(reinterpret_cast<const char*>(&height_o), sizeof(height_o));
    ofs.write(reinterpret_cast<const char*>(&width_o), sizeof(width_o));
    ofs.write(reinterpret_cast<const char*>(&channels_o), sizeof(channels_o));

    const auto row_bytes =
        sizeof(T) * static_cast<std::size_t>(channels_i) * width_i;

    for (std::size_t y = 0; y < height_i; ++y) {
        ofs.write(reinterpret_cast<const char*>(
                      in_ptr + y * static_cast<std::size_t>(step) *
                                   static_cast<std::size_t>(channels_i)),
                  row_bytes);
    }

    return ofs.good();
}

std::string npy_descr(const BufferType type) {
    using enum BufferType;
    switch (type) {
    case UNSIGNED_BYTE:
        return "|u1";
    case UNSIGNED_SHORT:
        return "<u2";
    case SHORT:
        return "<i2";
    case INT32:
        return "<i4";
    case FLOAT32:
    case FLOAT64: // OID stores doubles as float32
        return "<f4";
    }
    return "|u1";
}

// Writes the .npy v1.0 header: magic, version, uint16-LE length, then a
// space-padded, '\n'-terminated dict so that (10 + header_len) % 64 == 0.
void write_npy_header(std::ofstream& ofs,
                      const std::string& descr,
                      const int height,
                      const int width,
                      const int channels) {
    const std::string shape =
        channels == 1 ? std::format("({}, {})", height, width)
                      : std::format("({}, {}, {})", height, width, channels);
    std::string dict =
        std::format("{{'descr': '{}', 'fortran_order': False, 'shape': {}, }}",
                    descr,
                    shape);

    // 10 bytes precede the dict (6 magic + 2 version + 2 length); the dict
    // carries a trailing '\n'. Pad with spaces up to the next 64-byte boundary.
    const std::size_t unpadded = 10 + dict.size() + 1;
    const std::size_t padded = (unpadded + 63) / 64 * 64;
    dict.append(padded - unpadded, ' ');
    dict.push_back('\n');

    // The .npy spec fixes the header length as two little-endian bytes,
    // independent of host byte order, so emit them explicitly.
    const auto header_len = static_cast<std::uint16_t>(dict.size());
    const std::array len_le = {static_cast<char>(header_len & 0xFF),
                               static_cast<char>((header_len >> 8) & 0xFF)};
    constexpr std::array<char, 2> version = {1, 0};
    ofs.write("\x93NUMPY", 6);
    ofs.write(version.data(), version.size());
    ofs.write(len_le.data(), len_le.size());
    ofs.write(dict.data(), static_cast<std::streamsize>(dict.size()));
}

template <typename T>
bool export_npy_impl(const std::uint8_t* data,
                     const BufferType type,
                     const int width,
                     const int height,
                     const int channels_i,
                     const int step,
                     const std::string& path) {
    const auto width_i = static_cast<std::size_t>(width);
    const auto height_i = static_cast<std::size_t>(height);
    const auto channels = static_cast<std::size_t>(channels_i);
    const auto step_i = static_cast<std::size_t>(step);

    auto ofs = std::ofstream{std::filesystem::path{path}, std::ios::binary};
    write_npy_header(ofs, npy_descr(type), height, width, channels_i);

    // step is measured in pixels; each pixel spans channels * sizeof(T) bytes.
    // Offsetting the byte pointer directly avoids assuming the input is an
    // aligned, live T[] sequence.
    const auto row_bytes = sizeof(T) * channels * width_i;
    const auto row_stride = sizeof(T) * channels * step_i;
    for (std::size_t y = 0; y < height_i; ++y) {
        ofs.write(reinterpret_cast<const char*>(data + y * row_stride),
                  static_cast<std::streamsize>(row_bytes));
    }
    return ofs.good();
}

} // namespace

RgbaImage normalize_to_rgba8_raw(const std::uint8_t* data,
                                 const RawBufferDesc& desc,
                                 const std::array<float, 8>& bc_comp) {
    using enum BufferType;
    switch (desc.type) {
    case UNSIGNED_BYTE:
        return normalize_to_rgba8_impl<std::uint8_t>(data,
                                                     desc.width,
                                                     desc.height,
                                                     desc.channels,
                                                     desc.step,
                                                     bc_comp,
                                                     desc.pixel_layout);
    case UNSIGNED_SHORT:
        return normalize_to_rgba8_impl<std::uint16_t>(data,
                                                      desc.width,
                                                      desc.height,
                                                      desc.channels,
                                                      desc.step,
                                                      bc_comp,
                                                      desc.pixel_layout);
    case SHORT:
        return normalize_to_rgba8_impl<std::int16_t>(data,
                                                     desc.width,
                                                     desc.height,
                                                     desc.channels,
                                                     desc.step,
                                                     bc_comp,
                                                     desc.pixel_layout);
    case INT32:
        return normalize_to_rgba8_impl<std::int32_t>(data,
                                                     desc.width,
                                                     desc.height,
                                                     desc.channels,
                                                     desc.step,
                                                     bc_comp,
                                                     desc.pixel_layout);
    case FLOAT32:
        [[fallthrough]];
    case FLOAT64:
        return normalize_to_rgba8_impl<float>(data,
                                              desc.width,
                                              desc.height,
                                              desc.channels,
                                              desc.step,
                                              bc_comp,
                                              desc.pixel_layout);
    }
    return RgbaImage{};
}

bool export_octave_raw(const std::uint8_t* data,
                       const BufferType type,
                       const int width,
                       const int height,
                       const int channels,
                       const int step,
                       const std::string& path) {
    using enum BufferType;
    switch (type) {
    case UNSIGNED_BYTE:
        return export_octave_impl<std::uint8_t>(
            data, width, height, channels, step, path);
    case UNSIGNED_SHORT:
        return export_octave_impl<std::uint16_t>(
            data, width, height, channels, step, path);
    case SHORT:
        return export_octave_impl<std::int16_t>(
            data, width, height, channels, step, path);
    case INT32:
        return export_octave_impl<std::int32_t>(
            data, width, height, channels, step, path);
    case FLOAT32:
        [[fallthrough]];
    case FLOAT64:
        return export_octave_impl<float>(
            data, width, height, channels, step, path);
    }
    return false;
}

bool export_npy_raw(const std::uint8_t* data,
                    const BufferType type,
                    const int width,
                    const int height,
                    const int channels,
                    const int step,
                    const std::string& path) {
    using enum BufferType;
    switch (type) {
    case UNSIGNED_BYTE:
        return export_npy_impl<std::uint8_t>(
            data, type, width, height, channels, step, path);
    case UNSIGNED_SHORT:
        return export_npy_impl<std::uint16_t>(
            data, type, width, height, channels, step, path);
    case SHORT:
        return export_npy_impl<std::int16_t>(
            data, type, width, height, channels, step, path);
    case INT32:
        return export_npy_impl<std::int32_t>(
            data, type, width, height, channels, step, path);
    case FLOAT32:
        [[fallthrough]];
    case FLOAT64:
        return export_npy_impl<float>(
            data, type, width, height, channels, step, path);
    }
    return false;
}

} // namespace oid::BufferExporter
