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

#include "buffer_exporter.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>

#include <QPixmap>

namespace oid::BufferExporter
{

template <typename T>
float get_multiplier()
{
    return 255.0f / static_cast<float>((std::numeric_limits<T>::max)());
}


template <>
float get_multiplier<float>()
{
    return 255.0f;
}


template <typename T>
T get_max_intensity()
{
    return (std::numeric_limits<T>::max)();
}


template <>
float get_max_intensity<float>()
{
    return 1.0f;
}


void repeat_first_channel_into_g_and_b(
    std::array<std::uint8_t, 4>& unformatted_pixel,
    std::size_t& c)
{
    for (; c < 3; ++c) {
        unformatted_pixel[c] = unformatted_pixel[0];
    }
}


template <typename T>
void export_bitmap(const std::string& fname, const Buffer* buffer)
{
    const auto width_i  = static_cast<std::size_t>(buffer->buffer_width_f);
    const auto height_i = static_cast<std::size_t>(buffer->buffer_height_f);

    auto processed_buffer = std::vector<std::uint8_t>(4 * width_i * height_i);

    auto out_ptr = processed_buffer.data();

    const auto bc_comp     = buffer->auto_buffer_contrast_brightness();
    const auto color_scale = get_multiplier<T>();

    const auto max_intensity = get_max_intensity<T>();

    auto pixel_layout = std::array<std::uint8_t, 4>{};
    for (int c = 0; c < 4; ++c) {
        switch (buffer->get_pixel_layout()[c]) {
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

    auto in_ptr             = std::bit_cast<const T*>(buffer->buffer);
    const auto input_stride = buffer->channels * buffer->step;
    auto unformatted_pixel  = std::array<std::uint8_t, 4>{};
    const auto channels     = static_cast<std::size_t>(buffer->channels);

    for (std::size_t y = 0; y < height_i; ++y) {
        for (std::size_t x = 0; x < width_i; ++x) {
            const auto col_offset = x * buffer->channels;
            auto c                = std::size_t{0};

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

    const auto bytes_per_line = static_cast<int>(width_i * 4);
    const auto output_image   = QImage{processed_buffer.data(),
                                     static_cast<int>(width_i),
                                     static_cast<int>(height_i),
                                     bytes_per_line,
                                     QImage::Format_RGBA8888};
    if (!output_image.save(fname.c_str(), "png")) {
        std::cerr << "Failed to save image" << std::endl;
    }
}


template <typename T>
std::string get_type_descriptor();


template <>
std::string get_type_descriptor<std::uint8_t>()
{
    return "uint8";
}


template <>
std::string get_type_descriptor<std::uint16_t>()
{
    return "uint16";
}


template <>
std::string get_type_descriptor<std::int16_t>()
{
    return "int16";
}


template <>
std::string get_type_descriptor<std::int32_t>()
{
    return "int32";
}


template <>
std::string get_type_descriptor<float>()
{
    return "float";
}


template <typename T>
void export_binary(const std::string& fname, const Buffer* buffer)
{
    const auto width_i  = static_cast<std::size_t>(buffer->buffer_width_f);
    const auto height_i = static_cast<std::size_t>(buffer->buffer_height_f);

    const auto in_ptr = std::bit_cast<const T*>(buffer->buffer);

    const auto output_path = std::filesystem::path{fname};
    auto ofs               = std::ofstream{output_path};

    ofs << get_type_descriptor<T>() << height_i << width_i << buffer->channels;
    for (std::size_t y = 0; y < height_i; ++y) {
        ofs << in_ptr + y * buffer->step * buffer->channels;
    }
}


void export_buffer(const Buffer* buffer,
                   const std::string& path,
                   const OutputType type)
{
    using enum BufferType;
    if (type == OutputType::Bitmap) {
        switch (buffer->type) {
        case UnsignedByte:
            export_bitmap<std::uint8_t>(path, buffer);
            break;
        case UnsignedShort:
            export_bitmap<std::uint16_t>(path, buffer);
            break;
        case Short:
            export_bitmap<std::int16_t>(path, buffer);
            break;
        case Int32:
            export_bitmap<std::int32_t>(path, buffer);
            break;
        case Float32:
            [[fallthrough]];
        case Float64:
            export_bitmap<float>(path, buffer);
            break;
        }
    } else {
        // Matlab/Octave matrix (load with the oid_load.m function)
        switch (buffer->type) {
        case UnsignedByte:
            export_binary<std::uint8_t>(path, buffer);
            break;
        case UnsignedShort:
            export_binary<std::uint16_t>(path, buffer);
            break;
        case Short:
            export_binary<std::int16_t>(path, buffer);
            break;
        case Int32:
            export_binary<std::int32_t>(path, buffer);
            break;
        case Float32:
            [[fallthrough]];
        case Float64:
            export_binary<float>(path, buffer);
            break;
        }
    }
}

} // namespace oid::BufferExporter
