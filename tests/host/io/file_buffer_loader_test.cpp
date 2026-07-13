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
#include <string>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <gtest/gtest.h>

using namespace oid::host;

namespace {

// stb write callback: appends bytes to the std::vector passed as `context`.
void sink_write(void* context, void* data, int size) {
    auto* sink = static_cast<std::vector<std::byte>*>(context);
    const auto* p = static_cast<const std::byte*>(data);
    sink->insert(sink->end(), p, p + size);
}

// Encode an 8-bit RGB PNG (width x height) in memory.
std::vector<std::byte> make_png_rgb(int width, int height) {
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * height *
                                      3);
    for (std::size_t i = 0; i < pixels.size(); ++i) {
        pixels[i] = static_cast<unsigned char>(i & 0xFF);
    }
    std::vector<std::byte> sink;
    stbi_write_png_to_func(
        sink_write, &sink, width, height, 3, pixels.data(), width * 3);
    return sink;
}

// Encode an 8-bit RGBA PNG (width x height) in memory.
std::vector<std::byte> make_png_rgba(int width, int height) {
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * height *
                                      4);
    for (std::size_t i = 0; i < pixels.size(); ++i) {
        pixels[i] = static_cast<unsigned char>(i & 0xFF);
    }
    std::vector<std::byte> sink;
    stbi_write_png_to_func(
        sink_write, &sink, width, height, 4, pixels.data(), width * 4);
    return sink;
}

// A PNG signature + IHDR claiming the given dimensions, followed by an empty
// IDAT header and no pixel data -- what a decompression bomb looks like: a tiny
// file whose header advertises an enormous image. stbi_info reads the claimed
// dimensions from IHDR and stops at the IDAT without decoding, which is what
// the size preflight inspects. (stb keeps scanning past IHDR for a tRNS chunk,
// so the IDAT is required for the header scan to terminate; stb does not verify
// the zero CRCs.)
std::vector<std::byte> make_png_header(std::uint32_t w, std::uint32_t h) {
    std::vector<std::byte> b;
    auto push = [&b](std::initializer_list<int> xs) {
        for (const int x : xs) {
            b.push_back(static_cast<std::byte>(x & 0xFF));
        }
    };
    auto push_be32 = [&push](std::uint32_t v) {
        push({static_cast<int>((v >> 24) & 0xFF),
              static_cast<int>((v >> 16) & 0xFF),
              static_cast<int>((v >> 8) & 0xFF),
              static_cast<int>(v & 0xFF)});
    };
    push({0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A}); // signature
    push_be32(13);                                       // IHDR data length
    push({'I', 'H', 'D', 'R'});
    push_be32(w);
    push_be32(h);
    push({8, 2, 0, 0, 0}); // 8-bit, RGB, deflate, no filter, no interlace
    push_be32(0);          // IHDR CRC placeholder (stb does not validate it)
    push_be32(0);          // empty IDAT data length
    push({'I', 'D', 'A', 'T'}); // header scan stops here with the claimed dims
    return b;
}

// Encode an HDR (float RGB) image in memory.
std::vector<std::byte> make_hdr_rgb(int width, int height) {
    std::vector<float> pixels(static_cast<std::size_t>(width) * height * 3,
                              0.5f);
    std::vector<std::byte> sink;
    stbi_write_hdr_to_func(sink_write, &sink, width, height, 3, pixels.data());
    return sink;
}

// Build a minimal v1 .npy blob: magic, version 1.0, header, payload.
std::vector<std::byte> make_npy(const std::string& descr,
                                bool fortran_order,
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
    const std::size_t padded = ((unpadded + 63) / 64) * 64;
    dict.append(padded - unpadded, ' ');
    dict.push_back('\n');

    const auto header_len = static_cast<std::uint16_t>(dict.size());

    std::vector<std::byte> blob;
    const std::array<unsigned char, 6> magic = {0x93, 'N', 'U', 'M', 'P', 'Y'};
    for (unsigned char c : magic) {
        blob.push_back(static_cast<std::byte>(c));
    }
    blob.push_back(static_cast<std::byte>(1)); // major
    blob.push_back(static_cast<std::byte>(0)); // minor
    blob.push_back(static_cast<std::byte>(header_len & 0xFF));
    blob.push_back(static_cast<std::byte>((header_len >> 8) & 0xFF));
    for (char c : dict) {
        blob.push_back(static_cast<std::byte>(static_cast<unsigned char>(c)));
    }
    blob.insert(blob.end(), payload.begin(), payload.end());
    return blob;
}

std::vector<std::byte> u8_payload(std::size_t n) {
    std::vector<std::byte> p(n);
    for (std::size_t i = 0; i < n; ++i) {
        p[i] = static_cast<std::byte>(i & 0xFF);
    }
    return p;
}

} // namespace

TEST(FileBufferLoaderTest, DecodesPng8bit) {
    const auto bytes = make_png_rgb(4, 3);
    const auto result = decode_file_bytes(bytes, "img.png", "img.png");
    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_EQ(result->width, 4);
    EXPECT_EQ(result->height, 3);
    EXPECT_EQ(result->channels, 3);
    EXPECT_EQ(result->step, 4);
    EXPECT_EQ(result->type, oid::BufferType::UNSIGNED_BYTE);
    EXPECT_EQ(result->kind, BufferKind::LOCAL_FILE);
    EXPECT_EQ(result->pixel_layout, ""); // 3ch -> empty, not "rgb"
}

TEST(FileBufferLoaderTest, DecodesHdrAsFloat) {
    const auto bytes = make_hdr_rgb(4, 3);
    const auto result = decode_file_bytes(bytes, "img.hdr", "img.hdr");
    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_EQ(result->width, 4);
    EXPECT_EQ(result->height, 3);
    EXPECT_EQ(result->channels, 3);
    EXPECT_EQ(result->type, oid::BufferType::FLOAT32);
    EXPECT_EQ(result->kind, BufferKind::LOCAL_FILE);
}

TEST(FileBufferLoaderTest, DecodesNpyThroughLoader) {
    const auto blob = make_npy("|u1", false, {2, 2}, u8_payload(4));
    const auto result = decode_file_bytes(blob, "a.npy", "a.npy");
    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_EQ(result->width, 2);
    EXPECT_EQ(result->height, 2);
    EXPECT_EQ(result->channels, 1);
    EXPECT_EQ(result->kind, BufferKind::LOCAL_FILE);
}

TEST(FileBufferLoaderTest, RejectsGarbageBytes) {
    const std::vector<std::byte> junk(32, std::byte{0x7F});
    const auto result = decode_file_bytes(junk, "x.bin", "x.bin");
    EXPECT_FALSE(result.has_value());
}

TEST(FileBufferLoaderTest, DecodesNarrowRgba) {
    // A 2x2 RGBA image has width (2) < channels (4). The decoder must produce a
    // valid record (step == width), which the renderer must accept.
    const auto bytes = make_png_rgba(2, 2);
    const auto result = decode_file_bytes(bytes, "n.png", "n.png");
    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_EQ(result->width, 2);
    EXPECT_EQ(result->channels, 4);
    EXPECT_EQ(result->step, 2); // pixels-per-row == width, i.e. step < channels
    EXPECT_EQ(result->pixel_layout, "rgba");
}

TEST(FileBufferLoaderTest, RejectsOversizeImageDimensions) {
    // A tiny file claiming a width past the decode cap (a decompression bomb)
    // must be rejected up front from the header, not attempted and crashed.
    // (Dimensions stay within stb's own overflow guard so stbi_info reports
    // them and our cap is what does the rejecting.)
    const auto bytes = make_png_header(200000, 100);
    const auto result = decode_file_bytes(bytes, "bomb.png", "bomb.png");
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("dimension"), std::string::npos)
        << result.error();
}

TEST(FileBufferLoaderTest, RejectsMissingFile) {
    const auto result =
        load_buffer_from_file("/nonexistent/path/does/not/exist.png");
    EXPECT_FALSE(result.has_value());
}
