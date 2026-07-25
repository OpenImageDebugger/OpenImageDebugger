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

#ifndef RAW_DATA_DECODE_H_
#define RAW_DATA_DECODE_H_

#include <cstddef> // for std::size_t
#include <cstdint> // for std::uint8_t

#include <optional>
#include <vector> // for std::vector

namespace oid {

enum class BufferType {
    UNSIGNED_BYTE = 0,
    UNSIGNED_SHORT = 2,
    SHORT = 3,
    INT32 = 4,
    FLOAT32 = 5,
    FLOAT64 = 6
};

// Largest buffer, in bytes, that any path will accept. Bounds what an
// untrusted wire declaration can make the viewer allocate, and is the same
// ceiling Buffer::update() enforces before uploading a texture.
constexpr std::uint64_t MAX_BUFFER_BYTES = 16ULL * 1024ULL * 1024ULL * 1024ULL;

// What the viewer can actually display. They live here, below the renderer,
// so the wire layer can refuse a buffer it would otherwise allocate and then
// have Buffer::configure() drop.
constexpr int MIN_BUFFER_DIMENSION = 1;
constexpr int MAX_BUFFER_DIMENSION = 131072; // 2^17, ~the 100k practical max
constexpr int MIN_CHANNEL_COUNT = 1;
constexpr int MAX_CHANNEL_COUNT = 4;

// True if the geometry is one the renderer will accept. Deliberately separate
// from the sizing helpers below: those answer whether a payload can be
// measured, this answers whether the result is displayable, and the two
// refusals want different diagnostics.
[[nodiscard]] constexpr bool
within_display_limits(const int width, const int height, const int channels) {
    return width >= MIN_BUFFER_DIMENSION && width <= MAX_BUFFER_DIMENSION &&
           height >= MIN_BUFFER_DIMENSION && height <= MAX_BUFFER_DIMENSION &&
           channels >= MIN_CHANNEL_COUNT && channels <= MAX_CHANNEL_COUNT;
}

// True if `type` is a value this protocol defines. The wire carries a plain
// int, and an unknown one would otherwise be silently taken as UNSIGNED_BYTE
// by type_size()'s default while drawing no pixel label at all.
[[nodiscard]] constexpr bool is_known_buffer_type(const BufferType type) {
    using enum BufferType;
    return type == UNSIGNED_BYTE || type == UNSIGNED_SHORT || type == SHORT ||
           type == INT32 || type == FLOAT32 || type == FLOAT64;
}

[[nodiscard]] constexpr bool is_known_buffer_type(const int type) {
    return is_known_buffer_type(static_cast<BufferType>(type));
}

std::vector<std::byte>
make_float_buffer_from_double(const std::vector<std::byte>& buff_double);

// True if `byte_count` wire bytes can hold every pixel the declared geometry
// addresses. The last row needs only `width` pixels, not the full `stride`,
// so a producer that trims trailing row padding is still accepted. It is a
// floor for one contiguous payload, which is what the single-shot path
// needs, and it implies nothing about rows being uniformly sized.
[[nodiscard]] bool geometry_fits_payload(int width,
                                         int height,
                                         int channels,
                                         int stride,
                                         BufferType type,
                                         std::size_t byte_count);

// Bytes a fully padded buffer of this geometry occupies, every row carrying
// its whole stride. nullopt if the geometry is not renderable (non-positive
// dimensions, or a stride narrower than the width) or if the size overflows.
// Row-strip assembly needs this rather than the floor above, because it
// addresses rows by a single fixed size.
[[nodiscard]] std::optional<std::size_t> padded_payload_size(
    int width, int height, int channels, int stride, BufferType type);

[[nodiscard]] constexpr std::size_t type_size(const BufferType type) noexcept {
    using enum BufferType;
    switch (type) {
    case INT32:
        return sizeof(std::int32_t);
    case SHORT:
        [[fallthrough]];
    case UNSIGNED_SHORT:
        return sizeof(std::int16_t);
    case FLOAT32:
        return sizeof(float);
    case FLOAT64:
        return sizeof(double);
    case UNSIGNED_BYTE:
        [[fallthrough]];
    default:
        // All enum values are handled above, this should never be reached
        return sizeof(std::uint8_t);
    }
}

} // namespace oid

#endif // RAW_DATA_DECODE_H_
