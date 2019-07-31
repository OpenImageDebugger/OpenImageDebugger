/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 GDB ImageWatch contributors
 * (github.com/csantosbh/gdb-imagewatch/)
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

enum class MessageType {
    GetObservedSymbols = 0,
    ReturnObservedSymbols = 1,
    SetAvailableSymbols = 2,
    PlotContents = 3,
    PlotRequest = 4
};

struct MessageBlock {
    virtual size_t size() const = 0;
    virtual const uint8_t* data() const = 0;
    virtual ~MessageBlock() {}
};

template<typename Primitive>
struct PrimitiveBlock: public MessageBlock {
    PrimitiveBlock(Primitive value)
        : data_(value) {}

    virtual size_t size() const {
        return sizeof(Primitive);
    }

    virtual const uint8_t* data() const {
        return reinterpret_cast<const uint8_t*>(&data_);
    }
private:
    Primitive data_;
};

struct StringBlock: public MessageBlock {
    StringBlock(const std::string& value)
        : data_(value) {}

    virtual size_t size() const {
        return data_.size();
    }

    virtual const uint8_t* data() const {
        return reinterpret_cast<const uint8_t*>(data_.data());
    }
private:
    std::string data_;
};

struct BufferBlock: public MessageBlock {
    BufferBlock(const uint8_t* buffer, size_t length)
        : buffer_(buffer)
        , length_(length) {}

    virtual size_t size() const {
        return length_;
    }

    virtual const uint8_t* data() const {
        return buffer_;
    }

private:
    const uint8_t* buffer_;
    size_t length_;
};

class MessageComposer {
public:
    template<typename PrimitiveType>
    MessageComposer& push(const PrimitiveType& value) {
        static_assert(std::is_same<PrimitiveType, MessageType>::value ||
                      std::is_same<PrimitiveType, size_t>::value,
                      "push must only be called with primitives");
        message_blocks_.emplace_back(new PrimitiveBlock<PrimitiveType>(value));

        return *this;
    }

    template<>
    MessageComposer& push<std::deque<std::string>>(const std::deque<std::string>& container) {
        message_blocks_.emplace_back(
            new PrimitiveBlock<size_t>(container.size())
        );
        for(const auto& value: container) {
            message_blocks_.emplace_back(
                new PrimitiveBlock<size_t>(value.size())
            );
            message_blocks_.emplace_back(
                new StringBlock(value)
            );
        }
        return *this;
    }

    MessageComposer& push(uint8_t* buffer, size_t size) {
        message_blocks_.emplace_back(new BufferBlock(buffer, size));

        return *this;
    }

    void send(QTcpSocket* socket) {
        for(const auto& block: message_blocks_) {
            socket->write(reinterpret_cast<const char*>(block->data()),
                          static_cast<qint64>(block->size()));
        }
        socket->waitForBytesWritten();
    }

private:
    std::deque<std::unique_ptr<MessageBlock>> message_blocks_;
};

#endif // IPC_MESSAGE_EXCHANGE_H_
