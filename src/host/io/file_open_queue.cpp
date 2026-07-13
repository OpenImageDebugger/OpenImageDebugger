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

#include "host/io/file_open_queue.h"

#include <utility>

namespace oid::host {

void FileOpenQueue::push(std::string path) {
    pending_.push_back(std::move(path));
}

void FileOpenQueue::push_all(const std::vector<std::string>& paths) {
    for (const std::string& path : paths) {
        pending_.push_back(path);
    }
}

bool FileOpenQueue::empty() const {
    return pending_.empty();
}

FileOpenOutcome FileOpenQueue::drain(const Loader& loader,
                                     const Upsert& upsert) {
    FileOpenOutcome outcome;
    while (!pending_.empty()) {
        const std::string path = std::move(pending_.front());
        pending_.pop_front();

        oid::Expected<BufferRecord> record = loader(path);
        if (record) {
            outcome.last_success = record->display_name;
            upsert(std::move(*record));
            ++outcome.succeeded;
        } else {
            outcome.last_error = record.error();
            ++outcome.failed;
        }
    }
    return outcome;
}

} // namespace oid::host
