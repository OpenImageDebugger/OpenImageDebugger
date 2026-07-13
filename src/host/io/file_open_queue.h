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

#ifndef HOST_IO_FILE_OPEN_QUEUE_H_
#define HOST_IO_FILE_OPEN_QUEUE_H_

#include <deque>
#include <string>
#include <utility>
#include <vector>

#include "host/io/expected.h"
#include "host/ui/buffer_model.h"

namespace oid::host {

struct FileOpenOutcome {
    int succeeded{0};
    int failed{0};
    std::string last_error;
    std::string last_success; // display_name of the last successful load
};

// FIFO of file paths waiting to be decoded and inserted into the model.
// Not thread-safe: push from and drain on the render thread.
class FileOpenQueue {
  public:
    void push(std::string path);
    void push_all(const std::vector<std::string>& paths);
    [[nodiscard]] bool empty() const;

    // Load and upsert every pending path, clearing the queue. `loader` maps a
    // path to an oid::Expected<BufferRecord>; `upsert` consumes each
    // successfully loaded record. Loader failures are counted and their message
    // retained but do not stop the drain.
    template <typename Loader, typename Upsert>
    FileOpenOutcome drain(const Loader& loader, const Upsert& upsert) {
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

  private:
    std::deque<std::string> pending_;
};

} // namespace oid::host

#endif // HOST_IO_FILE_OPEN_QUEUE_H_
