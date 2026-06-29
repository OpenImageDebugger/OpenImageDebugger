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
#include <memory>
#include <source_location>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include <QString>

#include "transport.h"
#include "tcp_transport.h"
#include "raw_data_decode.h"

class QTcpSocket;

namespace oid {

enum class MessageType {
    GetObservedSymbols = 0,
    GetObservedSymbolsResponse = 1,
    SetAvailableSymbols = 2,
    PlotBufferContents = 3,
    PlotBufferRequest = 4,
    ExportBufferRequest = 5,
    ApplySessionState = 6,
    SessionStateChanged = 7,
    ExportSelectedBuffer = 8,
    BufferRemoved = 9
};

// C++20 concept to replace SFINAE for primitive type checking
template <typename T>
concept PrimitiveType =
    std::is_same_v<T, MessageType> || std::is_same_v<T, int> ||
    std::is_same_v<T, float> || std::is_same_v<T, unsigned char> ||
    std::is_same_v<T, BufferType> || std::is_same_v<T, bool> ||
    std::is_same_v<T, std::size_t>;

// Dedicated exception for socket timeout errors
class SocketTimeoutError final : public std::runtime_error {
  public:
    explicit SocketTimeoutError(
        const char* operation,
        const std::source_location& loc = std::source_location::current())
        : std::runtime_error(std::string{"Socket "} + operation +
                             " timeout at " + loc.file_name() + ":" +
                             std::to_string(loc.line())) {}
};

// Helper function for error reporting with source location
[[noreturn]] inline void throw_socket_timeout_error(
    const char* operation,
    const std::source_location& loc = std::source_location::current()) {
    throw SocketTimeoutError{operation, loc};
}

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

    void send(QTcpSocket* socket) const;

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

    explicit MessageDecoder(QTcpSocket* socket);

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

        value.resize(symbol_length);
        // std::string uses char* internally, convert to std::byte* for
        // read_impl
        read_impl(std::span{reinterpret_cast<std::byte*>(value.data()),
                            symbol_length});

        return *this;
    }

    MessageDecoder& read(QString& value) {
        const auto symbol_length = [&] {
            auto length = std::size_t{};
            read(length);
            return length;
        }();

        const auto temp_string = [&] {
            auto string = std::vector<char>{};
            string.resize(symbol_length + 1, '\0');
            read_impl(std::span{reinterpret_cast<std::byte*>(string.data()),
                                symbol_length});
            return string;
        }();

        value = QString{temp_string.data()};

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
    std::unique_ptr<TcpTransport> owned_tcp_transport_;
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
