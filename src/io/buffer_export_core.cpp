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
                                  int width,
                                  int height,
                                  int channels_i,
                                  int step,
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
                        int width,
                        int height,
                        int channels_i,
                        int step,
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

} // namespace

RgbaImage normalize_to_rgba8_raw(const std::uint8_t* data,
                                 const BufferType type,
                                 const int width,
                                 const int height,
                                 const int channels,
                                 const int step,
                                 const std::array<float, 8>& bc_comp,
                                 const char* pixel_layout) {
    using enum BufferType;
    switch (type) {
    case UnsignedByte:
        return normalize_to_rgba8_impl<std::uint8_t>(
            data, width, height, channels, step, bc_comp, pixel_layout);
    case UnsignedShort:
        return normalize_to_rgba8_impl<std::uint16_t>(
            data, width, height, channels, step, bc_comp, pixel_layout);
    case Short:
        return normalize_to_rgba8_impl<std::int16_t>(
            data, width, height, channels, step, bc_comp, pixel_layout);
    case Int32:
        return normalize_to_rgba8_impl<std::int32_t>(
            data, width, height, channels, step, bc_comp, pixel_layout);
    case Float32:
        [[fallthrough]];
    case Float64:
        return normalize_to_rgba8_impl<float>(
            data, width, height, channels, step, bc_comp, pixel_layout);
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
    case UnsignedByte:
        return export_octave_impl<std::uint8_t>(
            data, width, height, channels, step, path);
    case UnsignedShort:
        return export_octave_impl<std::uint16_t>(
            data, width, height, channels, step, path);
    case Short:
        return export_octave_impl<std::int16_t>(
            data, width, height, channels, step, path);
    case Int32:
        return export_octave_impl<std::int32_t>(
            data, width, height, channels, step, path);
    case Float32:
        [[fallthrough]];
    case Float64:
        return export_octave_impl<float>(
            data, width, height, channels, step, path);
    }
    return false;
}

} // namespace oid::BufferExporter
