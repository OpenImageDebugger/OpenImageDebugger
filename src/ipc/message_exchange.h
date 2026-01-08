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
#include <deque>
#include <memory>
#include <span>

#include <QTcpSocket>

#include "raw_data_decode.h"

namespace oid
{

enum class MessageType {
    GetObservedSymbols         = 0,
    GetObservedSymbolsResponse = 1,
    SetAvailableSymbols        = 2,
    PlotBufferContents         = 3,
    PlotBufferRequest          = 4
};

struct MessageBlock
{
    [[nodiscard]] virtual std::size_t size() const    = 0;
    [[nodiscard]] virtual const uint8_t* data() const = 0;
    virtual ~MessageBlock();
};

template <typename Primitive>
struct PrimitiveBlock final : MessageBlock
{
    explicit PrimitiveBlock(Primitive value)
        : data_{value}
    {
    }

    [[nodiscard]] std::size_t size() const override
    {
        return sizeof(Primitive);
    }

    [[nodiscard]] const uint8_t* data() const override
    {
        return std::bit_cast<const uint8_t*>(&data_);
    }

  private:
    Primitive data_{};
};


struct StringBlock final : MessageBlock
{
    explicit StringBlock(std::string value);

    [[nodiscard]] std::size_t size() const override;

    [[nodiscard]] const uint8_t* data() const override;

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
struct BufferBlock final : MessageBlock
{
    /**
     * @brief Construct a BufferBlock from a std::span.
     *
     * @param span Span over the buffer data (the span's underlying buffer must
     *             remain valid for the lifetime of this BufferBlock)
     */
    explicit BufferBlock(std::span<const uint8_t> span)
        : buffer_span_{span}
    {
    }

    [[nodiscard]] std::size_t size() const override
    {
        return buffer_span_.size();
    }

    [[nodiscard]] const uint8_t* data() const override
    {
        return buffer_span_.data();
    }

  private:
    std::span<const uint8_t> buffer_span_{};
};

template <typename PrimitiveType>
void assert_primitive_type()
{
    static_assert(std::is_same_v<PrimitiveType, MessageType> ||
                      std::is_same_v<PrimitiveType, int> ||
                      std::is_same_v<PrimitiveType, unsigned char> ||
                      std::is_same_v<PrimitiveType, BufferType> ||
                      std::is_same_v<PrimitiveType, bool> ||
                      std::is_same_v<PrimitiveType, std::size_t>,
                  "this function must only be called with primitives");
}

class MessageComposer
{
  public:
    template <typename PrimitiveType>
    MessageComposer& push(const PrimitiveType& value)
    {
        assert_primitive_type<PrimitiveType>();

        message_blocks_.emplace_back(new PrimitiveBlock<PrimitiveType>(value));

        return *this;
    }

    MessageComposer& push(const uint8_t* buffer, const std::size_t size)
    {
        push(size);
        message_blocks_.emplace_back(
            new BufferBlock(std::span<const uint8_t>{buffer, size}));

        return *this;
    }

    void send(QTcpSocket* socket) const
    {
        for (const auto& block : message_blocks_) {
            auto offset = qint64{0};
            do {
                offset +=
                    socket->write(reinterpret_cast<const char*>(block->data()),
                                  static_cast<qint64>(block->size()));

                if (offset < static_cast<qint64>(block->size())) {
                    socket->waitForBytesWritten();
                }
            } while (offset < static_cast<qint64>(block->size()));
        }

        socket->waitForBytesWritten();
    }

    void clear()
    {
        message_blocks_.clear();
    }

  private:
    std::deque<std::unique_ptr<MessageBlock>> message_blocks_{};
};

class MessageDecoder
{
  public:
    explicit MessageDecoder(QTcpSocket* socket)
        : socket_{socket}
    {
    }

    template <typename PrimitiveType>
    MessageDecoder& read(PrimitiveType& value)
    {
        assert_primitive_type<PrimitiveType>();

        read_impl(std::bit_cast<char*>(&value), sizeof(PrimitiveType));

        return *this;
    }

    template <typename StringContainer, typename StringType>
    MessageDecoder& read(StringContainer& symbol_container)
    {
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
    QTcpSocket* socket_{};

    void read_impl(char* dst, const std::size_t read_length) const
    {
        auto offset = std::size_t{0};
        do {
            offset += socket_->read(dst + offset,
                                    static_cast<qint64>(read_length - offset));

            if (offset < read_length) {
                socket_->waitForReadyRead();
            }
        } while (offset < read_length);
    }
};

template <>
inline MessageComposer&
MessageComposer::push<std::string>(const std::string& value)
{
    push(value.size());
    message_blocks_.emplace_back(new StringBlock(value));
    return *this;
}

template <>
inline MessageComposer& MessageComposer::push<std::deque<std::string>>(
    const std::deque<std::string>& value)
{
    push(value.size());
    for (const auto& element : value) {
        push(element);
    }
    return *this;
}

template <>
inline MessageDecoder&
MessageDecoder::read<std::vector<uint8_t>>(std::vector<uint8_t>& value)
{
    const auto container_size = [&] {
        auto size = std::size_t{};
        read(size);
        return size;
    }();

    value.resize(container_size);
    read_impl(reinterpret_cast<char*>(value.data()), container_size);

    return *this;
}

template <>
inline MessageDecoder& MessageDecoder::read<std::string>(std::string& value)
{
    const auto symbol_length = [&] {
        auto length = std::size_t{};
        read(length);
        return length;
    }();

    value.resize(symbol_length);
    read_impl(&value.front(), symbol_length);

    return *this;
}

template <>
inline MessageDecoder& MessageDecoder::read<QString>(QString& value)
{
    const auto symbol_length = [&] {
        auto length = std::size_t{};
        read(length);
        return length;
    }();

    const auto temp_string = [&] {
        auto string = std::vector<char>{};
        string.resize(symbol_length + 1, '\0');
        read_impl(string.data(), symbol_length);
        return string;
    }();

    value = QString{temp_string.data()};

    return *this;
}

} // namespace oid

#endif // IPC_MESSAGE_EXCHANGE_H_
