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

#ifndef IPC_MESSAGE_EXCHANGE_H_
#define IPC_MESSAGE_EXCHANGE_H_

#include <bit>
#include <cstddef>
#include <deque>
#include <format>
#include <memory>
#include <source_location>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "raw_data_decode.h"
#include "transport.h"

namespace oid {

enum class MessageType {
    GET_OBSERVED_SYMBOLS = 0,
    GET_OBSERVED_SYMBOLS_RESPONSE = 1,
    SET_AVAILABLE_SYMBOLS = 2,
    PLOT_BUFFER_CONTENTS = 3,
    PLOT_BUFFER_REQUEST = 4,
    EXPORT_BUFFER_REQUEST = 5,
    APPLY_SESSION_STATE = 6,
    SESSION_STATE_CHANGED = 7,
    EXPORT_SELECTED_BUFFER = 8,
    BUFFER_REMOVED = 9,
    PLOT_BUFFER_BEGIN = 10,
    PLOT_BUFFER_CHUNK = 11,
    PLOT_BUFFER_END = 12
};

// Ceiling on a decoded string length. Names, pixel layouts and session JSON
// are the only strings on this wire; the bound exists so a peer-supplied
// length cannot drive an unbounded allocation, not to constrain real data.
constexpr std::size_t MAX_STRING_BYTES = 16ULL * 1024ULL * 1024ULL;

// C++20 concept to replace SFINAE for primitive type checking
template <typename T>
concept PrimitiveType =
    std::is_same_v<T, MessageType> || std::is_same_v<T, int> ||
    std::is_same_v<T, float> || std::is_same_v<T, unsigned char> ||
    std::is_same_v<T, BufferType> || std::is_same_v<T, bool> ||
    std::is_same_v<T, std::size_t>;

// Dedicated exception for socket timeout errors.
// NOTE: SocketTimeoutError is thrown from liboidipc (a shared library) and
// caught in oidwindow/oidbridge, which compile with -fvisibility=hidden. A
// hidden type_info is per-module, so a `catch (const SocketTimeoutError&)`
// across that boundary fails to match (RTTI compares type_info by address) and
// std::terminates. Callers therefore catch its base `std::runtime_error`
// instead (libc++'s type_info is a single shared symbol, so base-class RTTI
// matches across modules). Keep this exception derived from std::runtime_error.
class SocketTimeoutError final : public std::runtime_error {
  public:
    explicit SocketTimeoutError(
        const char* operation,
        const std::source_location& loc = std::source_location::current())
        : std::runtime_error(std::format("Socket {} timeout at {}:{}",
                                         operation,
                                         loc.file_name(),
                                         loc.line())) {}
};

// Helper function for error reporting with source location
[[noreturn]] inline void throw_socket_timeout_error(
    const char* operation,
    const std::source_location& loc = std::source_location::current()) {
    throw SocketTimeoutError{operation, loc};
}

// A message that cannot be decoded, as opposed to one that has not arrived
// yet. Derives from std::runtime_error for the same cross-module RTTI reason
// as SocketTimeoutError above: callers catch the base, never this type.
class MessageDecodeError final : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

struct MessageBlock {
    [[nodiscard]] virtual std::size_t size() const noexcept = 0;
    // Returns raw byte data - using std::byte* for semantic clarity that this
    // is raw byte storage, not numeric data. Conversion to uint8_t*/char* is
    // done at API boundaries (Qt, OpenGL, etc.)
    [[nodiscard]] virtual const std::byte* data() const noexcept = 0;
    virtual ~MessageBlock() noexcept;
};

template <typename Primitive> struct PrimitiveBlock final : MessageBlock {
    explicit PrimitiveBlock(Primitive value) : data_{value} {}

    [[nodiscard]] std::size_t size() const noexcept override {
        return sizeof(Primitive);
    }

    [[nodiscard]] const std::byte* data() const noexcept override {
        // Safe byte access to object representation using std::as_bytes
        return std::as_bytes(std::span{&data_, 1}).data();
    }

  private:
    Primitive data_{};
};

struct StringBlock final : MessageBlock {
    explicit StringBlock(std::string value);

    [[nodiscard]] std::size_t size() const noexcept override;

    [[nodiscard]] const std::byte* data() const noexcept override;

  private:
    std::string data_{};
};

/**
 * @brief Message block that references an external buffer without owning it.
 *
 * @warning Lifetime Requirements:
 *   - The buffer pointed to by `buffer_span` MUST remain valid for the entire
 *     lifetime of this BufferBlock object.
 *   - The buffer MUST NOT be deallocated or modified while this BufferBlock
 *     exists.
 *   - Typically, BufferBlock is used immediately in MessageComposer::send(),
 *     ensuring the buffer lifetime is managed by the caller.
 *
 * @note This class uses std::span for safer buffer access. The span provides
 *       bounds checking capabilities and does not own the buffer data.
 */
struct BufferBlock final : MessageBlock {
    /**
     * @brief Construct a BufferBlock from a std::span.
     *
     * @param span Span over the buffer data (the span's underlying buffer must
     *             remain valid for the lifetime of this BufferBlock)
     */
    explicit BufferBlock(std::span<const std::byte> span)
        : buffer_span_{span} {}

    [[nodiscard]] std::size_t size() const noexcept override {
        return buffer_span_.size();
    }

    [[nodiscard]] const std::byte* data() const noexcept override {
        return buffer_span_.data();
    }

  private:
    std::span<const std::byte> buffer_span_{};
};

class MessageComposer {
  public:
    template <PrimitiveType T> MessageComposer& push(const T& value) {

        message_blocks_.emplace_back(
            std::make_unique<PrimitiveBlock<T>>(value));

        return *this;
    }

    MessageComposer& push(std::span<const std::byte> buffer) {
        push(buffer.size());
        message_blocks_.emplace_back(std::make_unique<BufferBlock>(buffer));

        return *this;
    }

    void send(ITransport& transport) const {
        std::size_t total = 0;
        for (const auto& block : message_blocks_) {
            total += block->size();
        }
        std::vector<std::byte> frame;
        frame.reserve(total);
        for (const auto& block : message_blocks_) {
            const auto* data = block->data();
            frame.insert(frame.end(), data, data + block->size());
        }
        if (!frame.empty()) {
            transport.send(frame);
        }
    }

    void clear() {
        message_blocks_.clear();
    }

    // Overloads for non-primitive types
    MessageComposer& push(const std::string& value) {
        push(value.size());
        message_blocks_.emplace_back(std::make_unique<StringBlock>(value));
        return *this;
    }

    MessageComposer& push(const std::deque<std::string>& value) {
        push(value.size());
        for (const auto& element : value) {
            push(element);
        }
        return *this;
    }

  private:
    std::deque<std::unique_ptr<MessageBlock>> message_blocks_{};
};

class MessageDecoder {
  public:
    explicit MessageDecoder(ITransport& transport);

    template <PrimitiveType T> MessageDecoder& read(T& value) {
        // Safe mutable byte access to object representation
        read_impl(std::as_writable_bytes(std::span{&value, 1}));

        return *this;
    }

    // Overloads for non-primitive types - defined inline so templates can see
    // them
    MessageDecoder& read(std::vector<std::byte>& value) {
        const auto container_size = [&] {
            auto size = std::size_t{};
            read(size);
            return size;
        }();

        // The length comes from the peer and drives the allocation below.
        // Refuse an impossible one rather than ask the allocator for it: a
        // throw here has consumed only the length prefix, whereas a
        // bad_alloc from resize() would leave the payload unread and every
        // later message decoding from the wrong offset.
        if (exceeds_max_buffer_bytes(container_size)) {
            throw MessageDecodeError{"declared payload exceeds the maximum "
                                     "buffer size"};
        }
        // Reachable only where size_t is 32 bits, i.e. the wasm build: there
        // MAX_BUFFER_BYTES's 16 GiB ceiling can never be exceeded, so it
        // cannot catch a length past what the container can represent;
        // max_size() is the real limit there. resize() would otherwise throw
        // std::length_error, which is not a std::runtime_error and would
        // escape poll()'s catch. Keep this guard.
        if (container_size > value.max_size()) {
            throw MessageDecodeError{
                "declared payload exceeds the container limit"};
        }
        value.resize(container_size);
        read_impl(std::span{value.data(), container_size});

        return *this;
    }

    MessageDecoder& read(std::string& value) {
        const auto symbol_length = [&] {
            auto length = std::size_t{};
            read(length);
            return length;
        }();

        // As above. Strings on this wire are names, pixel layouts and session
        // JSON, none of which approach this bound.
        if (symbol_length > MAX_STRING_BYTES) {
            throw MessageDecodeError{"declared string exceeds the maximum "
                                     "length"};
        }
        // Reachable only where size_t is 32 bits, i.e. the wasm build: same
        // reasoning as the vector overload above. Keep this guard.
        if (symbol_length > value.max_size()) {
            throw MessageDecodeError{
                "declared string exceeds the container limit"};
        }
        value.resize(symbol_length);
        // std::string uses char* internally, convert to std::byte* for
        // read_impl
        read_impl(std::span{reinterpret_cast<std::byte*>(value.data()),
                            symbol_length});

        return *this;
    }

    template <typename StringContainer, typename StringType>
    MessageDecoder& read(StringContainer& symbol_container) {
        const auto number_symbols = [&] {
            auto num{std::size_t{}};
            read(num);
            return num;
        }();

        for (int s = 0; s < static_cast<int>(number_symbols); ++s) {
            const auto symbol_value = [&] {
                auto value{StringType{}};
                read(value);
                return value;
            }();

            symbol_container.push_back(symbol_value);
        }

        return *this;
    }

  private:
    ITransport& transport_;

    void read_impl(std::span<std::byte> dst) const {
        auto offset = std::size_t{0};
        const auto read_length = dst.size();
        while (offset < read_length) {
            const auto n = transport_.receive(dst.subspan(offset));
            if (n == 0) {
                throw_socket_timeout_error("read");
            }
            offset += n;
        }
    }
};

} // namespace oid

#endif // IPC_MESSAGE_EXCHANGE_H_
