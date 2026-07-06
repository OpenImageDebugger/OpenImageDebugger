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

#ifndef HOST_UI_IPC_BUFFER_MODEL_H_
#define HOST_UI_IPC_BUFFER_MODEL_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "host/ui/buffer_model.h"

namespace oid::host {

// Dynamic, IPC-fed BufferModel: buffers are added/replaced/removed at
// runtime as IpcClient observes them arrive, get re-plotted, or leave.
// Unlike MockBufferModel's fixed set, this model mutates in place via
// upsert()/remove().
//
// Storage is vector<unique_ptr<BufferRecord>>, mirroring MockBufferModel,
// so each live BufferRecord keeps a stable heap address: a Stage
// holds a std::span into a BufferRecord::bytes, and other slots must not
// move when one slot is replaced or erased. Re-plotting a buffer (upsert
// with a name that already exists) does NOT mutate the existing
// BufferRecord's bytes in place -- it swaps in a fresh unique_ptr for that
// slot, so a consumer holding a reference from at(i) across an upsert must
// re-fetch. A monotonic model-wide revision() plus a per-slot
// revision_of(i) let a consumer detect "something changed" and "which slot
// changed" cheaply, without diffing bytes.
class IpcBufferModel final : public BufferModel {
  public:
    std::size_t size() const override;
    const BufferRecord& at(std::size_t i) const override;

    // Inserts a new buffer, or replaces an existing one matched by
    // `record.variable_name`. On replace, the matching slot's unique_ptr is
    // swapped for a fresh one built from `record` (the old BufferRecord is
    // destroyed), and that slot's per-slot revision advances so consumers
    // can tell a re-plot from an untouched buffer. On insert, the record is
    // appended. Either way, the model-wide revision() advances.
    void upsert(BufferRecord record);

    // Removes the buffer matched by `variable_name`, if any. No-op (and
    // does not bump revision()) if no buffer with that name is present.
    void remove(std::string_view variable_name);

    // Monotonic counter bumped on every upsert/remove, for cheap
    // model-wide change detection.
    [[nodiscard]] std::uint64_t revision() const {
        return revision_;
    }

    // Per-slot revision, bumped only when that slot's BufferRecord is
    // replaced (insert or re-plot), so a consumer can tell a re-plot of
    // buffer i from an unrelated change elsewhere in the model.
    [[nodiscard]] std::uint64_t revision_of(std::size_t i) const override;

    [[nodiscard]] const std::string&
    variable_name_of(std::size_t i) const override;

  private:
    std::vector<std::unique_ptr<BufferRecord>> storage_;
    std::vector<std::uint64_t> slot_revision_; // parallel to storage_
    std::uint64_t revision_{0};
    std::uint64_t next_slot_rev_{1};
};

} // namespace oid::host

#endif // HOST_UI_IPC_BUFFER_MODEL_H_
