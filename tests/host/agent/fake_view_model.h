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

#ifndef TESTS_HOST_AGENT_FAKE_VIEW_MODEL_H_
#define TESTS_HOST_AGENT_FAKE_VIEW_MODEL_H_

#include <cmath>
#include <cstddef>
#include <functional>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "host/agent/view_model.h"
#include "host/util/transparent_string_hash.h"

namespace oid::host::agent {

// In-memory ViewModel double: no Stage/Camera/BufferValues behind it. Tests
// populate `buffers` (and `pixel_data`, for read_pixels()) directly.
class FakeViewModel : public ViewModel { // NOSONAR
  public:
    // Held in the real engine's (Camera's) internal units: zoom as a power
    // (multiplier == ZOOM_FACTOR^power), rotation in radians, index -1 == all.
    struct Record {
        double center_x = 0.0;
        double center_y = 0.0;
        double zoom_power = 0.0;
        double rotation_rad = 0.0;
        int index = -1;
    };

    std::vector<BufferInfo> buffers;
    std::unordered_map<std::string,
                       std::vector<std::byte>,
                       TransparentStringHash,
                       std::equal_to<>>
        pixel_data;
    std::unordered_map<std::string,
                       Record,
                       TransparentStringHash,
                       std::equal_to<>>
        records;
    int viewport_w = 0;
    int viewport_h = 0;

    // Hooks: make a mutator report failure without touching its record, and
    // count read_pixels() calls so a test can assert it was (or wasn't) run.
    bool fail_set_zoom_power = false;
    bool throw_in_read_pixels = false;
    int read_pixels_calls = 0;

    void add(const std::string_view name,
             const int width,
             const int height,
             const int channels) {
        BufferInfo info;
        info.name = std::string(name);
        info.display_name = info.name;
        info.width = width;
        info.height = height;
        info.channels = channels;
        // step is a row width in *pixels* (GL_UNPACK_ROW_LENGTH), so `width`
        // for a dense buffer; width*channels would over-declare the row.
        info.step = width;
        info.type = 0;
        const char* wide_layout = channels == 3 ? "rgb" : "";
        info.pixel_layout = channels == 4 ? "rgba" : wide_layout;
        buffers.push_back(std::move(info));
    }

    // Convenience for get_buffer tests: stashes the bytes read_pixels()
    // returns for `name`, independent of that buffer's declared shape.
    void set_bytes(const std::string_view name, std::vector<std::byte> bytes) {
        pixel_data[std::string(name)] = std::move(bytes);
    }

    std::size_t buffer_count() override {
        return buffers.size();
    }

    std::optional<BufferInfo> buffer_at(const std::size_t i) override {
        if (i >= buffers.size()) {
            return std::nullopt;
        }
        return buffers[i];
    }

    std::optional<BufferInfo>
    buffer_named(const std::string_view name) override {
        for (const auto& info : buffers) {
            if (info.name == name) {
                return info;
            }
        }
        return std::nullopt;
    }

    bool read_pixels(const std::string_view name,
                     std::vector<std::byte>& out) override {
        ++read_pixels_calls;
        if (throw_in_read_pixels) {
            throw std::runtime_error( // NOSONAR test hook, generic is fine
                "read_pixels failure");
        }
        const auto it = pixel_data.find(name);
        if (it == pixel_data.end()) {
            return false;
        }
        out = it->second;
        return true;
    }

    bool select(const std::string_view name) override {
        for (std::size_t i = 0; i < buffers.size(); ++i) {
            if (buffers[i].name == name) {
                selected_index_ = i;
                return true;
            }
        }
        return false;
    }

    std::optional<std::string> selected_name() override {
        if (!selected_index_.has_value() ||
            *selected_index_ >= buffers.size()) {
            return std::nullopt;
        }
        return buffers[*selected_index_].name;
    }

    std::optional<ViewState> view_of(const std::string_view name) override {
        if (!buffer_named(name)) {
            return std::nullopt;
        }
        const std::string key{name};
        const auto& [center_x, center_y, zoom_power, rotation_rad, index] =
            records[key];

        ViewState state;
        state.buffer = key;
        state.center_x = center_x;
        state.center_y = center_y;
        state.zoom = std::pow(ZOOM_FACTOR, zoom_power);
        state.rotation_deg =
            normalize_degrees(rotation_rad * (180.0 / std::numbers::pi));
        state.channel = index < 0 ? "all" : std::to_string(index);
        return state;
    }

    bool set_center(const std::string_view name, double x, double y) override {
        return with_record(name, [x, y](Record& record) {
            record.center_x = x;
            record.center_y = y;
        });
    }

    bool set_zoom_power(const std::string_view name, double power) override {
        if (fail_set_zoom_power) {
            return false;
        }
        return with_record(
            name, [power](Record& record) { record.zoom_power = power; });
    }

    bool set_rotation_rad(const std::string_view name,
                          double radians) override {
        return with_record(
            name, [radians](Record& record) { record.rotation_rad = radians; });
    }

    bool set_channel(const std::string_view name, int index) override {
        return with_record(name,
                           [index](Record& record) { record.index = index; });
    }

    bool auto_contrast() override {
        return auto_contrast_flag_;
    }

    void set_auto_contrast(const bool enabled) override {
        auto_contrast_flag_ = enabled;
    }

    std::pair<int, int> viewport_size() override {
        return {viewport_w, viewport_h};
    }

  private:
    std::optional<std::size_t> selected_index_;
    bool auto_contrast_flag_ = true;

    template <typename Mutator>
    bool with_record(const std::string_view name, Mutator&& mutate) {
        if (!buffer_named(name)) {
            return false;
        }
        Record& record = records[std::string(name)];
        mutate(record);
        return true;
    }
};

} // namespace oid::host::agent

#endif // TESTS_HOST_AGENT_FAKE_VIEW_MODEL_H_
