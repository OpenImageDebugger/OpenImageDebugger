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

#ifndef HOST_UI_BUFFER_MODEL_H_
#define HOST_UI_BUFFER_MODEL_H_

#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ipc/raw_data_decode.h"

namespace oid::host {

// One buffer as the UI chrome sees it: enough metadata to render a
// buffer-list row and build a Stage, plus the raw pixel bytes ready for
// GlCanvas upload. IpcBufferModel populates these from IPC-decoded
// buffers; MockBufferModel below fabricates them deterministically.
struct BufferRecord {
    std::string variable_name; // stable key (e.g. debugger symbol name)
    std::string display_name;  // human label shown in the buffer list
    std::string pixel_layout;  // e.g. "rgba"; empty for single-channel
    bool transpose{false};
    int width{};
    int height{};
    int channels{};
    int step{};
    oid::BufferType type{oid::BufferType::UnsignedByte};
    std::vector<std::byte> bytes;
};

// Read-only view over the set of buffers the chrome should list.
// IpcBufferModel is backed by live IPC state; MockBufferModel below is the
// deterministic stand-in consumed by UiState and StageManager.
class BufferModel {
  public:
    virtual ~BufferModel() = default;

    virtual std::size_t size() const = 0;
    virtual const BufferRecord& at(std::size_t i) const = 0;

    // Stable key identifying the buffer at slot `i` (its variable_name),
    // used by StageManager to re-key Stages by identity rather than
    // by index, since a dynamic model (IpcBufferModel) may insert/remove
    // slots at runtime.
    virtual const std::string& variable_name_of(std::size_t i) const = 0;

    // Monotonic per-slot revision, bumped whenever the BufferRecord at slot
    // `i` is replaced (e.g. a re-plot). StageManager compares this against
    // the revision it last observed to decide whether to rebuild the
    // Stage's GL buffer via Stage::buffer_update() rather than reuse it.
    virtual std::uint64_t revision_of(std::size_t i) const = 0;
};

// Deterministic, IPC-free BufferModel: holds a fixed vector<BufferRecord>
// supplied at construction (typically via make_default_mock_model()), so
// chrome development and tests never depend on randomness or live IPC.
//
// Storage is vector<unique_ptr<BufferRecord>> rather than vector<BufferRecord>
// so each BufferRecord has a stable heap address: erase() must not
// move surviving records, because each oid::Stage holds a
// std::span<const std::byte> pointing straight into a BufferRecord::bytes --
// moving that record on erase would dangle every other Stage's span. This
// makes MockBufferModel move-only, which is fine: nothing copies it.
class MockBufferModel : public BufferModel {
  public:
    explicit MockBufferModel(std::vector<std::unique_ptr<BufferRecord>> records)
        : storage_(std::move(records)) {}

    std::size_t size() const override {
        return storage_.size();
    }

    const BufferRecord& at(std::size_t i) const override {
        return *storage_.at(i);
    }

    const std::string& variable_name_of(std::size_t i) const override {
        return storage_.at(i)->variable_name;
    }

    // MockBufferModel's records never re-plot, so every slot's revision is
    // permanently 0: StageManager's sync() will never see it change and so
    // will never call Stage::buffer_update() for a mock buffer.
    std::uint64_t revision_of(std::size_t /*i*/) const override {
        return 0;
    }

    // Bounds-guarded erase: no-op if `i >= size()`. Erasing storage_[i]
    // (a unique_ptr) never touches the addresses of surviving BufferRecords,
    // so any Stage still holding a span into a surviving record's bytes
    // stays valid. Callers that also own per-index Stage state must drop
    // the Stage referencing the erased record *before* calling this, since
    // the Stage's span becomes dangling once the record is destroyed.
    void erase(std::size_t i) {
        if (i >= storage_.size()) {
            return;
        }
        storage_.erase(storage_.begin() + static_cast<std::ptrdiff_t>(i));
    }

  private:
    std::vector<std::unique_ptr<BufferRecord>> storage_;
};

// Human-readable type+channel label used by the buffer-list panel, e.g.
// "uint8x3", "float32x1".
inline std::string type_label(const oid::BufferType type, const int channels) {
    using enum oid::BufferType;
    std::string base;
    switch (type) {
    case UnsignedByte:
        base = "uint8";
        break;
    case UnsignedShort:
        base = "uint16";
        break;
    case Short:
        base = "int16";
        break;
    case Int32:
        base = "int32";
        break;
    case Float32:
        base = "float32";
        break;
    case Float64:
        base = "float64";
        break;
    }
    return base + "x" + std::to_string(channels);
}

// Builds the fixed set of mock buffers used by chrome development/tests:
// a 64x64 RGB8 gradient, a 32x32 RGBA8 checkerboard, and a 16x16
// single-channel Float32 ramp. All three are generated purely from pixel
// index -- no randomness -- so the result is reproducible across runs.
inline MockBufferModel make_default_mock_model() {
    std::vector<std::unique_ptr<BufferRecord>> records;

    // 64x64 RGB8 gradient: R ramps with x, G ramps with y, B constant.
    {
        constexpr int w = 64;
        constexpr int h = 64;
        constexpr int channels = 3;
        std::vector<std::byte> pixels(
            static_cast<std::size_t>(w * h * channels));
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                auto* p =
                    &pixels[static_cast<std::size_t>((y * w + x) * channels)];
                p[0] = std::byte(x * 4);
                p[1] = std::byte(y * 4);
                p[2] = std::byte(128);
            }
        }
        records.push_back(std::make_unique<BufferRecord>(BufferRecord{
            .variable_name = "gradient_rgb8",
            .display_name = "gradient (64x64 rgb8)",
            .pixel_layout = "rgb",
            .transpose = false,
            .width = w,
            .height = h,
            .channels = channels,
            .step = w,
            .type = oid::BufferType::UnsignedByte,
            .bytes = std::move(pixels),
        }));
    }

    // 32x32 RGBA8 checkerboard, 4x4-pixel cells.
    {
        constexpr int w = 32;
        constexpr int h = 32;
        constexpr int channels = 4;
        constexpr int cell = 4;
        std::vector<std::byte> pixels(
            static_cast<std::size_t>(w * h * channels));
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const bool light = ((x / cell) + (y / cell)) % 2 == 0;
                const auto v = static_cast<std::uint8_t>(light ? 220 : 40);
                auto* p =
                    &pixels[static_cast<std::size_t>((y * w + x) * channels)];
                p[0] = std::byte(v);
                p[1] = std::byte(v);
                p[2] = std::byte(v);
                p[3] = std::byte(255);
            }
        }
        records.push_back(std::make_unique<BufferRecord>(BufferRecord{
            .variable_name = "checker_rgba8",
            .display_name = "checker (32x32 rgba8)",
            .pixel_layout = "rgba",
            .transpose = false,
            .width = w,
            .height = h,
            .channels = channels,
            .step = w,
            .type = oid::BufferType::UnsignedByte,
            .bytes = std::move(pixels),
        }));
    }

    // 16x16 single-channel Float32 diagonal ramp, values in [0, 1].
    {
        constexpr int w = 16;
        constexpr int h = 16;
        constexpr int channels = 1;
        std::vector<std::byte> pixels(
            static_cast<std::size_t>(w * h * channels) * sizeof(float));
        auto* values = std::bit_cast<float*>(pixels.data());
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                values[y * w + x] =
                    static_cast<float>(x + y) / static_cast<float>(2 * (w - 1));
            }
        }
        records.push_back(std::make_unique<BufferRecord>(BufferRecord{
            .variable_name = "ramp_float32",
            .display_name = "ramp (16x16 float32)",
            .pixel_layout = "",
            .transpose = false,
            .width = w,
            .height = h,
            .channels = channels,
            .step = w,
            .type = oid::BufferType::Float32,
            .bytes = std::move(pixels),
        }));
    }

    return MockBufferModel(std::move(records));
}

} // namespace oid::host

#endif // HOST_UI_BUFFER_MODEL_H_
