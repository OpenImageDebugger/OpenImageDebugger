/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2024 OpenImageDebugger contributors
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

#include <deque>
#include <memory>

#include <QTcpSocket>

#include "raw_data_decode.h"

enum class MessageType {
    GetObservedSymbols         = 0,
    GetObservedSymbolsResponse = 1,
    SetAvailableSymbols        = 2,
    PlotBufferContents         = 3,
    PlotBufferRequest          = 4
};

struct MessageBlock
{
    [[nodiscard]] virtual size_t size() const         = 0;
    [[nodiscard]] virtual const uint8_t* data() const = 0;
    virtual ~MessageBlock();
};

template <typename Primitive>
struct PrimitiveBlock final : MessageBlock
{
    explicit PrimitiveBlock(Primitive value)
        : data_(value)
    {
    }

    [[nodiscard]] size_t size() const override
    {
        return sizeof(Primitive);
    }

    [[nodiscard]] const uint8_t* data() const override
    {
        return reinterpret_cast<const uint8_t*>(&data_);
    }

  private:
    Primitive data_;
};


struct StringBlock final : MessageBlock
{
    explicit StringBlock(std::string value);

    [[nodiscard]] size_t size() const override;

    [[nodiscard]] const uint8_t* data() const override;

  private:
    std::string data_;
};

struct BufferBlock final : MessageBlock
{
    BufferBlock(const uint8_t* buffer, const size_t length)
        : buffer_(buffer)
        , length_(length)
    {
    }

    [[nodiscard]] size_t size() const override
    {
        return length_;
    }

    [[nodiscard]] const uint8_t* data() const override
    {
        return buffer_;
    }

  private:
    const uint8_t* buffer_;
    size_t length_;
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

    MessageComposer& push(const uint8_t* buffer, const size_t size)
    {
        push(size);
        message_blocks_.emplace_back(new BufferBlock(buffer, size));

        return *this;
    }

    void send(QTcpSocket* socket) const
    {
        for (const auto& block : message_blocks_) {
            qint64 offset = 0;
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
    std::deque<std::unique_ptr<MessageBlock>> message_blocks_;
};

class MessageDecoder
{
  public:
    explicit MessageDecoder(QTcpSocket* socket)
        : socket_(socket)
    {
    }

    template <typename PrimitiveType>
    MessageDecoder& read(PrimitiveType& value)
    {
        assert_primitive_type<PrimitiveType>();

        read_impl(reinterpret_cast<char*>(&value), sizeof(PrimitiveType));

        return *this;
    }

    template <typename StringContainer, typename StringType>
    MessageDecoder& read(StringContainer& symbol_container)
    {
        size_t number_symbols;
        read(number_symbols);

        for (int s = 0; s < static_cast<int>(number_symbols); ++s) {
            StringType symbol_value;
            read(symbol_value);

            symbol_container.push_back(symbol_value);
        }

        return *this;
    }

  private:
    QTcpSocket* socket_;

    void read_impl(char* dst, const size_t read_length) const
    {
        size_t offset = 0;
        do {
            offset += socket_->read(dst + offset,
                                    static_cast<qint64>(read_length - offset));

            if (offset < read_length) {
                socket_->waitForReadyRead();
            }
        } while (offset < read_length);
    }
};

template <> inline
MessageComposer& MessageComposer::push<std::string>(const std::string& value)
{
    push(value.size());
    message_blocks_.emplace_back(new StringBlock(value));
    return *this;
}

template <> inline
MessageComposer& MessageComposer::push<std::deque<std::string>>(const std::deque<std::string>& value)
{
    push(value.size());
    for (const auto& element : value) {
        push(element);
    }
    return *this;
}

template <> inline
MessageDecoder& MessageDecoder::read<std::vector<uint8_t>>(std::vector<uint8_t>& value)
{
    size_t container_size;
    read(container_size);

    value.resize(container_size);
    read_impl(reinterpret_cast<char*>(value.data()), container_size);

    return *this;
}

template <> inline
MessageDecoder& MessageDecoder::read<std::string>(std::string& value)
{
    size_t symbol_length;
    read(symbol_length);

    value.resize(symbol_length);
    read_impl(&value.front(), symbol_length);

    return *this;
}

template <> inline
MessageDecoder& MessageDecoder::read<QString>(QString& value)
{
    size_t symbol_length;
    read(symbol_length);

    std::vector<char> temp_string;
    temp_string.resize(symbol_length + 1, '\0');
    read_impl(temp_string.data(), symbol_length);
    value = QString(temp_string.data());

    return *this;
}


#endif // IPC_MESSAGE_EXCHANGE_H_
