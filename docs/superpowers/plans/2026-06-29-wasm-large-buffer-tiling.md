# WASM Large-Buffer Tiling Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let the WASM viewer plot large buffers (e.g. 40000×2160, any dtype) that currently render blank, by chunking the buffer transport instead of sending one oversized frame.

**Architecture:** Approach A — chunk only the *transport*. Large buffers are sent as `PlotBufferBegin` + N row-strip `PlotBufferChunk` + `PlotBufferEnd`. A new C++ `BufferAssembler` reassembles the chunks into one contiguous buffer, then the viewer runs its existing GL-tiling / contrast / pixel-hover pipeline unchanged. Small buffers and the desktop TCP path keep using the single `PlotBufferContents` message.

**Tech Stack:** C++23 + Qt 6.11 + Emscripten (viewer, repo `OpenImageDebugger`); TypeScript ESM + `node:test` (extension, repo `openimagedebugger-vscode`); GoogleTest for C++ unit tests.

## Global Constraints

- Two repos: `OpenImageDebugger` (C++ viewer) and `openimagedebugger-vscode` (extension). Each task names its repo.
- New IPC message type values are **additive**: `PlotBufferBegin = 10`, `PlotBufferChunk = 11`, `PlotBufferEnd = 12` (last existing is `BufferRemoved = 9`). Use these exact names and numbers in both repos.
- Wire integers are little-endian; the extension encodes message type and lengths as `u32`, which matches the viewer's `wasm32` decode (`std::size_t` == 4 bytes under Emscripten). The chunked path is **only** used in `wasm32` mode.
- Chunks are **full-width row strips**: a chunk carries rows `[row_offset, row_offset + row_count)`, exactly `row_count × stride` tightly-packed bytes.
- Shared chunk budget: `8 MiB` (`8 * 1024 * 1024`). Name it `CHUNK_BUDGET` in the extension.
- Commit messages: plain, no `Co-Authored-By:` / AI-attribution trailer.
- Do not reformat or restructure unrelated code.

---

### Task 1: `BufferAssembler` (C++ core, repo `OpenImageDebugger`)

A pure, Qt/GL-free data structure that reassembles row-strip chunks. Unit-tested in isolation like `test_raw_data_decode`.

**Files:**
- Create: `src/ipc/buffer_assembler.h`
- Create: `src/ipc/buffer_assembler.cpp`
- Create: `tests/test_buffer_assembler.cpp`
- Modify: `tests/CMakeLists.txt` (add a test target after the `SessionStateCodec` block, line ~159)
- Modify: `src/ipc/CMakeLists.txt:32-36` (add `buffer_assembler.cpp` to the `oidipc` library sources)

**Interfaces:**
- Produces: `oid::BufferAssembler` with `void begin(BeginParams)`, `bool chunk(const std::string& name, std::size_t row_offset, std::size_t row_count, std::span<const std::byte> bytes)`, `std::optional<AssembledBuffer> end(const std::string& name)`. `AssembledBuffer` holds `{variable_name, display_name, pixel_layout, transpose, width, height, channels, stride, type, bytes}`. `BeginParams` holds the same fields minus `bytes`, plus `total_byte_size`.

- [ ] **Step 1: Write the header**

Create `src/ipc/buffer_assembler.h` (omit the license header for brevity here — copy the 24-line MIT header block from `src/ipc/postmessage_transport.cpp` lines 1-24 verbatim above the `#ifndef`):

```cpp
#ifndef IPC_BUFFER_ASSEMBLER_H_
#define IPC_BUFFER_ASSEMBLER_H_

#include <cstddef>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace oid {

// Geometry + completed bytes for a buffer reassembled from row-strip chunks.
struct AssembledBuffer {
    std::string variable_name;
    std::string display_name;
    std::string pixel_layout;
    bool transpose{};
    int width{};
    int height{};
    int channels{};
    int stride{};
    int type{};
    std::vector<std::byte> bytes;
};

// Reassembles a large buffer sent as row-strip chunks. Pure data structure:
// no Qt/GL/transport dependency, so it is unit-testable in isolation.
class BufferAssembler {
  public:
    struct BeginParams {
        std::string variable_name;
        std::string display_name;
        std::string pixel_layout;
        bool transpose{};
        int width{};
        int height{};
        int channels{};
        int stride{};
        int type{};
        std::size_t total_byte_size{};
    };

    // Start (or restart) a transfer, allocating total_byte_size zeroed bytes.
    void begin(BeginParams params);

    // Copy a row-strip into the in-progress buffer at row_offset. Returns false
    // (ignoring the data) if name is unknown, the strip size is wrong, or it
    // would exceed total_byte_size.
    [[nodiscard]] bool chunk(const std::string& name,
                             std::size_t row_offset,
                             std::size_t row_count,
                             std::span<const std::byte> bytes);

    // Finish the transfer, moving its bytes out and dropping the entry. Returns
    // nullopt if name is unknown.
    [[nodiscard]] std::optional<AssembledBuffer> end(const std::string& name);

  private:
    struct InProgress {
        BeginParams params;
        std::vector<std::byte> bytes;
    };
    std::map<std::string, InProgress, std::less<>> in_progress_{};
};

} // namespace oid

#endif // IPC_BUFFER_ASSEMBLER_H_
```

- [ ] **Step 2: Write the failing test**

Create `tests/test_buffer_assembler.cpp` (copy the MIT header block from `tests/test_raw_data_decode.cpp` lines 1-24 above the includes):

```cpp
#include "ipc/buffer_assembler.h"

#include <gtest/gtest.h>

using namespace oid;

namespace {

BufferAssembler::BeginParams make_begin(const std::string& name,
                                        const int width,
                                        const int height,
                                        const int stride,
                                        const std::size_t total) {
    return BufferAssembler::BeginParams{.variable_name = name,
                                        .display_name = name,
                                        .pixel_layout = "rgba",
                                        .transpose = false,
                                        .width = width,
                                        .height = height,
                                        .channels = 1,
                                        .stride = stride,
                                        .type = 0,
                                        .total_byte_size = total};
}

std::vector<std::byte> iota_bytes(const std::size_t n) {
    std::vector<std::byte> v(n);
    for (std::size_t i = 0; i < n; ++i) {
        v[i] = static_cast<std::byte>(i & 0xff);
    }
    return v;
}

} // namespace

TEST(BufferAssembler, ReassemblesRowStripsIntoContiguousBuffer) {
    constexpr int stride = 8;
    constexpr int height = 4;
    constexpr int width = 8;
    constexpr std::size_t total = stride * height;
    const auto full = iota_bytes(total);

    BufferAssembler a;
    a.begin(make_begin("buf", width, height, stride, total));
    ASSERT_TRUE(a.chunk("buf", 0, 2, std::span{full.data(), 2 * stride}));
    ASSERT_TRUE(
        a.chunk("buf", 2, 2, std::span{full.data() + 2 * stride, 2 * stride}));

    const auto done = a.end("buf");
    ASSERT_TRUE(done.has_value());
    EXPECT_EQ(done->width, width);
    EXPECT_EQ(done->height, height);
    EXPECT_EQ(done->stride, stride);
    EXPECT_EQ(done->bytes, full);
}

TEST(BufferAssembler, RejectsOverflowingChunk) {
    constexpr int stride = 8;
    constexpr int height = 2;
    constexpr std::size_t total = stride * height;
    BufferAssembler a;
    a.begin(make_begin("buf", 8, height, stride, total));
    const auto strip = iota_bytes(2 * stride);
    // rows [1,3) do not fit in a 2-row buffer.
    EXPECT_FALSE(a.chunk("buf", 1, 2, std::span{strip.data(), strip.size()}));
}

TEST(BufferAssembler, RejectsWrongSizedChunk) {
    constexpr int stride = 8;
    constexpr int height = 2;
    constexpr std::size_t total = stride * height;
    BufferAssembler a;
    a.begin(make_begin("buf", 8, height, stride, total));
    const auto strip = iota_bytes(stride + 1); // not a multiple of stride
    EXPECT_FALSE(a.chunk("buf", 0, 1, std::span{strip.data(), strip.size()}));
}

TEST(BufferAssembler, RejectsChunkAndEndForUnknownBuffer) {
    BufferAssembler a;
    const auto strip = iota_bytes(8);
    EXPECT_FALSE(a.chunk("missing", 0, 1, std::span{strip.data(), strip.size()}));
    EXPECT_FALSE(a.end("missing").has_value());
}

TEST(BufferAssembler, InterleavedBuffersDoNotCorruptEachOther) {
    constexpr int stride = 4;
    constexpr int height = 1;
    constexpr std::size_t total = stride * height;
    BufferAssembler a;
    a.begin(make_begin("a", 4, height, stride, total));
    a.begin(make_begin("b", 4, height, stride, total));
    const std::vector<std::byte> da{
        std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
    const std::vector<std::byte> db{
        std::byte{5}, std::byte{6}, std::byte{7}, std::byte{8}};
    ASSERT_TRUE(a.chunk("a", 0, 1, std::span{da.data(), da.size()}));
    ASSERT_TRUE(a.chunk("b", 0, 1, std::span{db.data(), db.size()}));
    EXPECT_EQ(a.end("a")->bytes, da);
    EXPECT_EQ(a.end("b")->bytes, db);
}
```

- [ ] **Step 3: Register the test target and library source**

In `tests/CMakeLists.txt`, append after line 159 (`add_test(NAME SessionStateCodec ...)`):

```cmake
# Test buffer assembler (row-strip reassembly)
add_executable(test_buffer_assembler
    test_buffer_assembler.cpp
)

target_include_directories(test_buffer_assembler
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src/ipc
)

target_link_libraries(test_buffer_assembler
    PRIVATE
    GTest::gtest_main
    GTest::gtest
)

target_sources(test_buffer_assembler PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ipc/buffer_assembler.cpp
)

add_test(NAME BufferAssemblerTests COMMAND test_buffer_assembler)
```

In `src/ipc/CMakeLists.txt`, change lines 32-36 to add the new source:

```cmake
add_library(${PROJECT_NAME} ${OID_IPC_LIBRARY_TYPE}
            buffer_assembler.cpp
            message_exchange.cpp
            raw_data_decode.cpp
            tcp_transport.cpp
            postmessage_transport.cpp)
```

- [ ] **Step 4: Run the test to verify it fails**

Run (from repo root, using the existing CMake test build dir):
```bash
cmake --build cmake-build-debug --target test_buffer_assembler 2>&1 | tail -20
```
Expected: **compile failure** — `buffer_assembler.cpp` does not exist yet (no such file / unresolved `oid::BufferAssembler` members).

- [ ] **Step 5: Write the implementation**

Create `src/ipc/buffer_assembler.cpp` (copy the MIT header block from `src/ipc/postmessage_transport.cpp` lines 1-24):

```cpp
#include "buffer_assembler.h"

#include <cstring>
#include <utility>

namespace oid {

void BufferAssembler::begin(BeginParams params) {
    const auto total = params.total_byte_size;
    auto& entry = in_progress_[params.variable_name];
    entry.params = std::move(params);
    entry.bytes.assign(total, std::byte{});
}

bool BufferAssembler::chunk(const std::string& name,
                            const std::size_t row_offset,
                            const std::size_t row_count,
                            const std::span<const std::byte> bytes) {
    const auto it = in_progress_.find(name);
    if (it == in_progress_.end()) {
        return false;
    }
    auto& entry = it->second;
    const auto stride = static_cast<std::size_t>(entry.params.stride);
    const auto offset = row_offset * stride;
    const auto expected = row_count * stride;
    if (bytes.size() != expected ||
        offset + bytes.size() > entry.bytes.size()) {
        return false;
    }
    std::memcpy(entry.bytes.data() + offset, bytes.data(), bytes.size());
    return true;
}

std::optional<AssembledBuffer> BufferAssembler::end(const std::string& name) {
    const auto it = in_progress_.find(name);
    if (it == in_progress_.end()) {
        return std::nullopt;
    }
    auto& entry = it->second;
    AssembledBuffer out{.variable_name = std::move(entry.params.variable_name),
                        .display_name = std::move(entry.params.display_name),
                        .pixel_layout = std::move(entry.params.pixel_layout),
                        .transpose = entry.params.transpose,
                        .width = entry.params.width,
                        .height = entry.params.height,
                        .channels = entry.params.channels,
                        .stride = entry.params.stride,
                        .type = entry.params.type,
                        .bytes = std::move(entry.bytes)};
    in_progress_.erase(it);
    return out;
}

} // namespace oid
```

- [ ] **Step 6: Run the test to verify it passes**

```bash
cmake --build cmake-build-debug --target test_buffer_assembler 2>&1 | tail -5
ctest --test-dir cmake-build-debug -R BufferAssemblerTests --output-on-failure
```
Expected: build succeeds; `5 tests passed`.

- [ ] **Step 7: Commit**

```bash
git add src/ipc/buffer_assembler.h src/ipc/buffer_assembler.cpp tests/test_buffer_assembler.cpp tests/CMakeLists.txt src/ipc/CMakeLists.txt
git commit -m "feat(ipc): add BufferAssembler for row-strip chunk reassembly"
```

---

### Task 2: Add chunked message types to the viewer enum (C++, repo `OpenImageDebugger`)

**Files:**
- Modify: `src/ipc/message_exchange.h` (the `enum class MessageType`, lines ~50-65)

**Interfaces:**
- Produces: `MessageType::PlotBufferBegin = 10`, `PlotBufferChunk = 11`, `PlotBufferEnd = 12`.

- [ ] **Step 1: Read the current enum**

Run:
```bash
sed -n '50,65p' src/ipc/message_exchange.h
```
Confirm the last entry is `BufferRemoved = 9` (or note the exact existing tail to edit precisely).

- [ ] **Step 2: Add the three values**

In `src/ipc/message_exchange.h`, add the new enumerators immediately after `BufferRemoved = 9,` inside `enum class MessageType`:

```cpp
    BufferRemoved = 9,
    PlotBufferBegin = 10,
    PlotBufferChunk = 11,
    PlotBufferEnd = 12,
```

- [ ] **Step 3: Verify it still compiles**

```bash
cmake --build cmake-build-debug --target test_message_exchange 2>&1 | tail -5
```
Expected: builds clean (no behavior change yet).

- [ ] **Step 4: Commit**

```bash
git add src/ipc/message_exchange.h
git commit -m "feat(ipc): reserve PlotBufferBegin/Chunk/End message types"
```

---

### Task 3: Extract `plot_buffer_from_fields` helper (C++ refactor, repo `OpenImageDebugger`)

Pure refactor, no behavior change: split `decode_plot_buffer_contents` into a wire-reading half and a reusable "plot from owned bytes" half that the chunked-End handler (Task 4) will also call.

**Files:**
- Modify: `src/ui/messaging/message_handler.h` (add private method declaration)
- Modify: `src/ui/messaging/message_handler.cpp:149-318`

**Interfaces:**
- Produces: `void MessageHandler::plot_buffer_from_fields(const std::string& variable_name_str, const std::string& display_name_str, const std::string& pixel_layout_str, bool transpose_buffer, int buff_width, int buff_height, int buff_channels, int buff_stride, BufferType buff_type, std::vector<std::byte> buff_contents);`

- [ ] **Step 1: Declare the helper**

In `src/ui/messaging/message_handler.h`, after the `void decode_plot_buffer_contents();` declaration (line ~90), add:

```cpp
    void plot_buffer_from_fields(const std::string& variable_name_str,
                                 const std::string& display_name_str,
                                 const std::string& pixel_layout_str,
                                 bool transpose_buffer,
                                 int buff_width,
                                 int buff_height,
                                 int buff_channels,
                                 int buff_stride,
                                 BufferType buff_type,
                                 std::vector<std::byte> buff_contents);
```

- [ ] **Step 2: Move the body into the helper**

In `src/ui/messaging/message_handler.cpp`, change `decode_plot_buffer_contents` so it only reads the wire and delegates. Replace the current function body (lines 149-318) so it reads:

```cpp
void MessageHandler::decode_plot_buffer_contents() {
    auto variable_name_str = std::string{};
    auto display_name_str = std::string{};
    auto pixel_layout_str = std::string{};
    auto transpose_buffer = bool{};
    auto buff_width = int{};
    auto buff_height = int{};
    auto buff_channels = int{};
    auto buff_stride = int{};
    auto buff_type = BufferType{};
    auto buff_contents = std::vector<std::byte>{};

    auto message_decoder = MessageDecoder{deps_.transport};
    message_decoder.read(variable_name_str)
        .read(display_name_str)
        .read(pixel_layout_str)
        .read(transpose_buffer)
        .read(buff_width)
        .read(buff_height)
        .read(buff_channels)
        .read(buff_stride)
        .read(buff_type)
        .read(buff_contents);

    plot_buffer_from_fields(variable_name_str,
                            display_name_str,
                            pixel_layout_str,
                            transpose_buffer,
                            buff_width,
                            buff_height,
                            buff_channels,
                            buff_stride,
                            buff_type,
                            std::move(buff_contents));
}

void MessageHandler::plot_buffer_from_fields(
    const std::string& variable_name_str,
    const std::string& display_name_str,
    const std::string& pixel_layout_str,
    const bool transpose_buffer,
    const int buff_width,
    const int buff_height,
    const int buff_channels,
    const int buff_stride,
    const BufferType buff_type,
    std::vector<std::byte> buff_contents) {
```

Then leave the existing code that was at lines 173-318 (starting `const auto lock = std::unique_lock{deps_.ui_mutex};` through the final `deps_.state.request_render_update = true; }`) **exactly as-is** as the body of `plot_buffer_from_fields`, with one change: the float64 branch consumes the moved parameter —

```cpp
    const auto lock = std::unique_lock{deps_.ui_mutex};

    if (buff_type == BufferType::Float64) {
        deps_.buffer_data.held_buffers[variable_name_str] =
            make_float_buffer_from_double(buff_contents);
    } else {
        deps_.buffer_data.held_buffers[variable_name_str] =
            std::move(buff_contents);
    }
```

(The `BufferParams` built later already passes `.pixel_layout = pixel_layout_str` and `.step = buff_stride`; those names are unchanged.)

- [ ] **Step 3: Build the viewer to verify the refactor compiles**

```bash
cmake --build cmake-build-debug --target openimagedebugger 2>&1 | tail -15
```
Expected: builds clean. (If the viewer target name differs, list targets with `cmake --build cmake-build-debug --target help | grep -i oid`.)

- [ ] **Step 4: Commit**

```bash
git add src/ui/messaging/message_handler.h src/ui/messaging/message_handler.cpp
git commit -m "refactor(messaging): extract plot_buffer_from_fields from decode"
```

---

### Task 4: Wire chunked decode into `MessageHandler` (C++, repo `OpenImageDebugger`)

**Files:**
- Modify: `src/ui/messaging/message_handler.h` (include, member, three method declarations)
- Modify: `src/ui/messaging/message_handler.cpp` (three decode methods + three dispatch cases)

**Interfaces:**
- Consumes: `oid::BufferAssembler` (Task 1); `MessageType::PlotBufferBegin/Chunk/End` (Task 2); `MessageHandler::plot_buffer_from_fields` (Task 3).

- [ ] **Step 1: Add the assembler member and method declarations**

In `src/ui/messaging/message_handler.h`, add near the top includes (after `#include "ipc/transport.h"`):

```cpp
#include "ipc/buffer_assembler.h"
```

Add three private method declarations after `void plot_buffer_from_fields(...);`:

```cpp
    void decode_plot_buffer_begin();
    void decode_plot_buffer_chunk();
    void decode_plot_buffer_end();
```

Add a private member alongside `Dependencies deps_;`:

```cpp
    BufferAssembler buffer_assembler_{};
```

- [ ] **Step 2: Implement the three decode methods**

In `src/ui/messaging/message_handler.cpp`, add after `decode_plot_buffer_contents` / `plot_buffer_from_fields`:

```cpp
void MessageHandler::decode_plot_buffer_begin() {
    auto p = BufferAssembler::BeginParams{};
    auto type_int = int{};
    auto message_decoder = MessageDecoder{deps_.transport};
    message_decoder.read(p.variable_name)
        .read(p.display_name)
        .read(p.pixel_layout)
        .read(p.transpose)
        .read(p.width)
        .read(p.height)
        .read(p.channels)
        .read(p.stride)
        .read(type_int)
        .read(p.total_byte_size);
    p.type = type_int;
    buffer_assembler_.begin(std::move(p));
}

void MessageHandler::decode_plot_buffer_chunk() {
    auto variable_name_str = std::string{};
    auto row_offset = std::size_t{};
    auto row_count = std::size_t{};
    auto bytes = std::vector<std::byte>{};
    auto message_decoder = MessageDecoder{deps_.transport};
    message_decoder.read(variable_name_str)
        .read(row_offset)
        .read(row_count)
        .read(bytes);
    if (!buffer_assembler_.chunk(
            variable_name_str, row_offset, row_count, std::span{bytes})) {
        std::cerr << "[Error] Dropped buffer chunk for: " << variable_name_str
                  << std::endl;
    }
}

void MessageHandler::decode_plot_buffer_end() {
    auto variable_name_str = std::string{};
    auto message_decoder = MessageDecoder{deps_.transport};
    message_decoder.read(variable_name_str);

    auto assembled = buffer_assembler_.end(variable_name_str);
    if (!assembled.has_value()) [[unlikely]] {
        std::cerr << "[Error] PlotBufferEnd for unknown buffer: "
                  << variable_name_str << std::endl;
        return;
    }
    plot_buffer_from_fields(assembled->variable_name,
                            assembled->display_name,
                            assembled->pixel_layout,
                            assembled->transpose,
                            assembled->width,
                            assembled->height,
                            assembled->channels,
                            assembled->stride,
                            static_cast<BufferType>(assembled->type),
                            std::move(assembled->bytes));
}
```

(`<iostream>` is already included in this file — `std::cerr` is used at line 227. `<span>` is included transitively via `buffer_assembler.h`.)

- [ ] **Step 3: Add the dispatch cases**

In `src/ui/messaging/message_handler.cpp`, inside the `switch (header)` in `decode_incoming_messages` (after the `PlotBufferContents` case, line ~369):

```cpp
    case MessageType::PlotBufferBegin:
        decode_plot_buffer_begin();
        break;
    case MessageType::PlotBufferChunk:
        decode_plot_buffer_chunk();
        break;
    case MessageType::PlotBufferEnd:
        decode_plot_buffer_end();
        break;
```

- [ ] **Step 4: Build the viewer**

```bash
cmake --build cmake-build-debug --target openimagedebugger 2>&1 | tail -15
```
Expected: builds clean.

- [ ] **Step 5: Commit**

```bash
git add src/ui/messaging/message_handler.h src/ui/messaging/message_handler.cpp
git commit -m "feat(messaging): decode chunked PlotBuffer Begin/Chunk/End"
```

---

### Task 5: Chunked encoder in the extension (TypeScript, repo `openimagedebugger-vscode`)

**Files:**
- Modify: `src/ipc/message-exchange.ts` (enum + four functions + constant)
- Test: `test/message-exchange.test.ts` (add cases)

**Interfaces:**
- Produces: `CHUNK_BUDGET: number`; `buildPlotBufferBegin(p, mode)`, `buildPlotBufferChunk(variableName, rowOffset, rowCount, bytes, mode)`, `buildPlotBufferEnd(variableName, mode)`, `buildPlotBufferFrames(p, mode, budget?)` → `Uint8Array[]`.

- [ ] **Step 1: Write the failing test**

In `test/message-exchange.test.ts`, add (adjust the import to include the new symbols; reuse the file's existing `node:test`/`assert` style):

```ts
import {
  CHUNK_BUDGET,
  buildPlotBufferFrames,
  MessageType,
} from '../src/ipc/message-exchange.js';

function readU32(b: Uint8Array, off: number): number {
  return (b[off] | (b[off + 1] << 8) | (b[off + 2] << 16) | (b[off + 3] << 24)) >>> 0;
}

test('small buffer encodes as a single PlotBufferContents frame', () => {
  const pixels = new Uint8Array(64).map((_, i) => i);
  const frames = buildPlotBufferFrames(
    {
      variableName: 'a', displayName: 'a', pixelLayout: 'rgba', transpose: false,
      width: 8, height: 8, channels: 1, stride: 8, bufferType: 0, pixels,
    },
    'wasm32'
  );
  assert.equal(frames.length, 1);
  assert.equal(readU32(frames[0], 0), MessageType.PlotBufferContents);
});

test('large buffer encodes as Begin + row-strip Chunks + End covering all bytes', () => {
  const stride = 8, height = 4;
  const pixels = new Uint8Array(stride * height).map((_, i) => i);
  // budget 16 bytes => floor(16/8) = 2 rows per chunk => 2 chunks.
  const frames = buildPlotBufferFrames(
    {
      variableName: 'a', displayName: 'a', pixelLayout: 'rgba', transpose: false,
      width: 8, height, channels: 1, stride, bufferType: 0, pixels,
    },
    'wasm32',
    16
  );
  assert.equal(frames.length, 4);
  assert.equal(readU32(frames[0], 0), MessageType.PlotBufferBegin);
  assert.equal(readU32(frames[1], 0), MessageType.PlotBufferChunk);
  assert.equal(readU32(frames[2], 0), MessageType.PlotBufferChunk);
  assert.equal(readU32(frames[3], 0), MessageType.PlotBufferEnd);

  // Each chunk frame layout: u32 type, u32 nameLen, name, u32 rowOffset,
  // u32 rowCount, u32 byteLen, bytes. nameLen == 1 ('a').
  const collected: number[] = [];
  for (const f of [frames[1], frames[2]]) {
    let off = 4;
    const nameLen = readU32(f, off); off += 4 + nameLen;
    off += 4; // rowOffset
    off += 4; // rowCount
    const byteLen = readU32(f, off); off += 4;
    for (let i = 0; i < byteLen; i++) collected.push(f[off + i]);
  }
  assert.deepEqual(new Uint8Array(collected), pixels);
});

test('CHUNK_BUDGET is 8 MiB', () => {
  assert.equal(CHUNK_BUDGET, 8 * 1024 * 1024);
});
```

- [ ] **Step 2: Run the test to verify it fails**

```bash
cd /Users/bruno/ws/openimagedebugger-vscode && rm -rf out && npm run compile && npm test 2>&1 | tail -20
```
Expected: compile or test failure — `CHUNK_BUDGET` / `buildPlotBufferFrames` not exported.

- [ ] **Step 3: Implement the encoder**

In `src/ipc/message-exchange.ts`, add to the `MessageType` enum after `BufferRemoved = 9,`:

```ts
  PlotBufferBegin = 10,
  PlotBufferChunk = 11,
  PlotBufferEnd = 12,
```

Then add (after `buildPlotBufferContents`):

```ts
export const CHUNK_BUDGET = 8 * 1024 * 1024; // 8 MiB per chunk message

export function buildPlotBufferBegin(
  p: PlotBufferParams,
  mode: WasmLengthMode
): Uint8Array {
  const parts: number[] = [];
  pushU32(parts, MessageType.PlotBufferBegin);
  pushString(parts, p.variableName, mode);
  pushString(parts, p.displayName, mode);
  pushString(parts, p.pixelLayout, mode);
  parts.push(p.transpose ? 1 : 0);
  pushI32(parts, p.width);
  pushI32(parts, p.height);
  pushI32(parts, p.channels);
  pushI32(parts, p.stride);
  pushI32(parts, p.bufferType);
  pushU32(parts, p.pixels.length);
  return new Uint8Array(parts);
}

export function buildPlotBufferChunk(
  variableName: string,
  rowOffset: number,
  rowCount: number,
  bytes: Uint8Array,
  mode: WasmLengthMode
): Uint8Array {
  const parts: number[] = [];
  pushU32(parts, MessageType.PlotBufferChunk);
  pushString(parts, variableName, mode);
  pushU32(parts, rowOffset);
  pushU32(parts, rowCount);
  pushU32(parts, bytes.length);
  for (const b of bytes) {
    parts.push(b);
  }
  return new Uint8Array(parts);
}

export function buildPlotBufferEnd(
  variableName: string,
  mode: WasmLengthMode
): Uint8Array {
  const parts: number[] = [];
  pushU32(parts, MessageType.PlotBufferEnd);
  pushString(parts, variableName, mode);
  return new Uint8Array(parts);
}

/**
 * Encode a plot buffer as frames. Small buffers (<= budget) use a single
 * PlotBufferContents; larger buffers are split into full-width row strips
 * carried by PlotBufferBegin + N PlotBufferChunk + PlotBufferEnd.
 */
export function buildPlotBufferFrames(
  p: PlotBufferParams,
  mode: WasmLengthMode,
  budget: number = CHUNK_BUDGET
): Uint8Array[] {
  if (p.pixels.length <= budget) {
    return [buildPlotBufferContents(p, mode)];
  }
  const frames: Uint8Array[] = [buildPlotBufferBegin(p, mode)];
  const rowsPerChunk = Math.max(1, Math.floor(budget / p.stride));
  for (let row = 0; row < p.height; row += rowsPerChunk) {
    const rowCount = Math.min(rowsPerChunk, p.height - row);
    const start = row * p.stride;
    const end = start + rowCount * p.stride;
    frames.push(
      buildPlotBufferChunk(
        p.variableName,
        row,
        rowCount,
        p.pixels.subarray(start, end),
        mode
      )
    );
  }
  frames.push(buildPlotBufferEnd(p.variableName, mode));
  return frames;
}
```

- [ ] **Step 4: Run the test to verify it passes**

```bash
cd /Users/bruno/ws/openimagedebugger-vscode && npm run compile && npm test 2>&1 | tail -20
```
Expected: all tests pass, including the three new cases.

- [ ] **Step 5: Commit**

```bash
cd /Users/bruno/ws/openimagedebugger-vscode
git add src/ipc/message-exchange.ts test/message-exchange.test.ts
git commit -m "feat(ipc): chunk large plot buffers into Begin/Chunk/End frames"
```

---

### Task 6: Stop boxing bytes in the webview bridge (TypeScript, repo `openimagedebugger-vscode`)

**Files:**
- Modify: `src/webview/panel.ts:249` and `src/webview/panel.ts:295`

**Interfaces:** none new — same `postMessage` shape, payload becomes a `Uint8Array` instead of a boxed `number[]`. The inbound bridge already normalizes via `toUint8Array(...)` (panel.ts:44-52), which accepts `Uint8Array`.

- [ ] **Step 1: Replace `Array.from(bytes)` on the two large-payload sends**

In `src/webview/panel.ts`, line 249 (in `sendFrame`):

```ts
    this.panel?.webview.postMessage({ type: 'oid-ipc-forward', payload: bytes });
```

And line 295 (in `sendPlotBuffer`):

```ts
    this.panel?.webview.postMessage({ type: 'oid-ipc-forward', payload: bytes });
```

(Leave line 57 — the small viewer→host `oid-ipc-out` send — unchanged.)

- [ ] **Step 2: Compile and run the suite**

```bash
cd /Users/bruno/ws/openimagedebugger-vscode && npm run compile && npm test 2>&1 | tail -10
```
Expected: compiles, all tests pass (no test references the boxed form).

- [ ] **Step 3: Commit**

```bash
cd /Users/bruno/ws/openimagedebugger-vscode
git add src/webview/panel.ts
git commit -m "perf(webview): forward plot frames as Uint8Array, not boxed array"
```

---

### Task 7: Send frames from the session pipeline (TypeScript, repo `openimagedebugger-vscode`)

Switch `SessionManager` from "encode one frame, send it" to "encode N frames, send each in order".

**Files:**
- Modify: `src/session/session-manager.ts` (`SessionDeps.encode` → `encodeFrames`; `plotAtFrame` loop)
- Modify: `src/extension.ts:8` (import) and `src/extension.ts:59` (deps wiring)
- Test: `test/session-manager.test.ts` (update the fake deps)

**Interfaces:**
- Consumes: `buildPlotBufferFrames` (Task 5).
- Produces: `SessionDeps.encodeFrames: (p: PlotBufferParams) => Uint8Array[]`.

- [ ] **Step 1: Update the failing test**

In `test/session-manager.test.ts`, find where the fake `SessionDeps` is built and replace its `encode` with `encodeFrames`. Locate the current property:

```bash
cd /Users/bruno/ws/openimagedebugger-vscode && grep -n "encode" test/session-manager.test.ts
```

Replace the `encode: (p) => ...` line with one returning an array, e.g.:

```ts
    encodeFrames: (p) => [new Uint8Array([p.width & 0xff, p.height & 0xff])],
```

If the test asserts on `sink.sendPlotBuffer` call counts, the count is now "frames per plot" (1 here) — keep the fake returning a single-element array so existing count assertions hold.

- [ ] **Step 2: Run to verify it fails**

```bash
cd /Users/bruno/ws/openimagedebugger-vscode && rm -rf out && npm run compile 2>&1 | tail -20
```
Expected: TypeScript error — `encodeFrames` not in `SessionDeps` / `encode` missing.

- [ ] **Step 3: Update `SessionManager`**

In `src/session/session-manager.ts`, change the `SessionDeps` field (line 22):

```ts
  encodeFrames: (p: PlotBufferParams) => Uint8Array[];
```

And in `plotAtFrame` (lines 96-97), replace:

```ts
      await this.deps.sink.ensureReady();
      await this.deps.sink.sendPlotBuffer(this.deps.encode(p));
```

with:

```ts
      await this.deps.sink.ensureReady();
      for (const frame of this.deps.encodeFrames(p)) {
        await this.deps.sink.sendPlotBuffer(frame);
      }
```

- [ ] **Step 4: Update `extension.ts` wiring**

In `src/extension.ts`, change the import (line 8):

```ts
import { buildPlotBufferFrames } from './ipc/message-exchange.js';
```

And the deps wiring (line 59):

```ts
    encodeFrames: (p) => buildPlotBufferFrames(p, 'wasm32'),
```

- [ ] **Step 5: Run the suite**

```bash
cd /Users/bruno/ws/openimagedebugger-vscode && npm run compile && npm test 2>&1 | tail -15
```
Expected: compiles; all tests pass.

- [ ] **Step 6: Commit**

```bash
cd /Users/bruno/ws/openimagedebugger-vscode
git add src/session/session-manager.ts src/extension.ts test/session-manager.test.ts
git commit -m "feat(session): send plot buffers as chunked frame sequences"
```

---

### Task 8: Widen transport length types (C++ defensive cleanup, repo `OpenImageDebugger`)

Chunking already keeps every message ≤ 8 MiB, so the 2 GB `int` overflow can no longer trigger — but remove the latent footgun.

**Files:**
- Modify: `src/ipc/postmessage_transport.cpp:31-44` and the `oidEnqueueInbound` signature (lines 75-81)

**Interfaces:** internal only.

- [ ] **Step 1: Widen the length parameters**

In `src/ipc/postmessage_transport.cpp`, change the `EM_JS` binding (lines 31-33) and `send` (line 40) to use unsigned 32-bit lengths:

```cpp
EM_JS(void, oid_send_to_js, (const void* ptr, unsigned len), {
  window.oidSend(HEAPU8.slice(ptr, ptr + len));
});
```

```cpp
    oid_send_to_js(data.data(), static_cast<unsigned>(data.size()));
```

And `oidEnqueueInbound` (lines 75-81): change `const int len` to `const unsigned len` and the guard `len <= 0` to `len == 0`:

```cpp
extern "C" void oidEnqueueInbound(const std::uintptr_t ptr, const unsigned len) {
    if (g_transport == nullptr || len == 0) {
        return;
    }
    const auto* data = reinterpret_cast<const std::byte*>(ptr);
    g_transport->enqueue_inbound({data, data + len});
}
```

(If the WASM JS glue declares `oidEnqueueInbound`'s `len` argument type anywhere, it passes a JS number either way — no caller change needed. `unsigned` doubles the safe range to 4 GiB.)

- [ ] **Step 2: Build transport test + viewer**

```bash
cmake --build cmake-build-debug --target test_transport 2>&1 | tail -5
ctest --test-dir cmake-build-debug -R TransportTests --output-on-failure
```
Expected: builds; transport tests pass.

- [ ] **Step 3: Commit**

```bash
git add src/ipc/postmessage_transport.cpp
git commit -m "fix(ipc): widen postMessage transport length from int to unsigned"
```

---

### Task 9: Manual verification (both repos)

No code; confirm the feature end-to-end. Do this first if the blank-render cause was never confirmed from the console (see spec "Risks").

- [ ] **Step 1: Rebuild the WASM viewer and copy artifacts into the extension's `media/`**

Follow the project's existing WASM build path (the Emscripten toolchain that produces `oidwindow.js`/`oidwindow.wasm`), then place the artifacts where `openimagedebugger-vscode/media/` expects them, as in prior WASM updates.

- [ ] **Step 2: Launch the extension and plot a large buffer of each dtype**

In a debug session, plot a 40000×2160 buffer as uint8, uint16, float32 (and, if available, float64). For each:
- Confirm the image **renders** (no longer blank).
- Hover a pixel and confirm the **value readout is correct**.
- Toggle contrast and confirm **auto-contrast** behaves.
- Open the viewer DevTools console and confirm **no `[Error]` lines** (`Dropped buffer chunk`, `PlotBufferEnd for unknown buffer`, `Could not initialize OpenGL canvas`).

- [ ] **Step 3: Record the result**

If all dtypes render correctly, the feature is complete. If a multi-GB float64×4 buffer fails on memory (expected per the spec's Approach-A bound), note it as the known limitation rather than a regression. If a `[Error]` line *does* appear, capture it — it redirects to the GL/validation or DAP-read follow-ups noted in the spec.

---

## Notes for the implementer

- **Drain cadence:** `decode_incoming_messages` processes one message per call (driven by the render loop). A chunked buffer therefore arrives over several loop ticks; the assembler holds state across ticks, so this is correct. For very many chunks (e.g. float64×4) the spread-out delivery is a touch slow but not incorrect — batching the drain is an optional future optimization, not part of this plan.
- **Desktop/TCP untouched:** only the extension emits chunked frames (`wasm32` mode). The Python/TCP bridge keeps sending `PlotBufferContents`; the viewer still decodes it via the unchanged path.
- The `out/` directory caveat for the extension: `rm -rf out` before `npm run compile` when you need a clean test baseline.
