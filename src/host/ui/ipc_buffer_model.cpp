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

#include "host/ui/ipc_buffer_model.h"

#include <utility>

namespace oid::host {

std::size_t IpcBufferModel::size() const {
    return storage_.size();
}

const BufferRecord& IpcBufferModel::at(const std::size_t i) const {
    return *storage_.at(i);
}

void IpcBufferModel::upsert(BufferRecord record) {
    for (std::size_t i = 0; i < storage_.size(); ++i) {
        if (storage_[i]->variable_name == record.variable_name) {
            storage_[i] = std::make_unique<BufferRecord>(std::move(record));
            slot_revision_[i] = next_slot_rev_++;
            ++revision_;
            return;
        }
    }
    storage_.push_back(std::make_unique<BufferRecord>(std::move(record)));
    slot_revision_.push_back(next_slot_rev_++);
    ++revision_;
}

void IpcBufferModel::remove(const std::string_view variable_name) {
    for (std::size_t i = 0; i < storage_.size(); ++i) {
        if (storage_[i]->variable_name == variable_name) {
            storage_.erase(storage_.begin() + static_cast<std::ptrdiff_t>(i));
            slot_revision_.erase(slot_revision_.begin() +
                                 static_cast<std::ptrdiff_t>(i));
            ++revision_;
            return;
        }
    }
}

std::uint64_t IpcBufferModel::revision_of(const std::size_t i) const {
    return slot_revision_.at(i);
}

const std::string& IpcBufferModel::variable_name_of(const std::size_t i) const {
    return storage_.at(i)->variable_name;
}

} // namespace oid::host
