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

#ifndef HOST_AGENT_VIEW_MODEL_H_
#define HOST_AGENT_VIEW_MODEL_H_

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace oid::host::agent {

// A snapshot of one buffer's identity and shape, enough to describe it to an
// agent without touching pixel data. `name` is the stable variable_name key
// used to address the buffer through every other ViewModel method; the
// remaining fields mirror what the visualization layer already tracks for
// its buffers.
struct BufferInfo {
    std::string name; // variable_name (stable key)
    std::string display_name;
    int width = 0;
    int height = 0;
    int channels = 0;
    int step = 0;
    int type = 0;             // oid::BufferType raw code: 0,2,3,4,5,6
    std::string pixel_layout; // e.g. "rgba"/"bgra"/"" (single-channel)
    bool transpose = false;
};

// An absolute view of one buffer, expressed entirely in agent-facing units
// (degrees, linear zoom multiplier, "all"/"0".."2" channel selector). Fields
// that have no meaningful value when the buffer set is empty (no selection
// yet) are carried as std::nullopt rather than an arbitrary default.
struct ViewState {
    std::optional<std::string>
        buffer;                     // selected buffer name, or nullopt if empty
    std::optional<double> center_x; // buffer pixels
    std::optional<double> center_y; // buffer pixels
    std::optional<double> zoom;     // linear multiplier
    std::optional<double> rotation_deg; // [0,360)
    std::string channel = "all";        // "all" or "0".."2"
    bool auto_contrast = true;          // global
    int viewport_w = 0;
    int viewport_h = 0;
};

// Abstract seam between AgentCore (unit conversion, request validation) and
// the native visualization engine (Stage/Camera/BufferValues). AgentCore
// only ever calls through this interface, in agent-facing units; a native
// adapter implementation translates each call into the engine's own units
// and performs the actual Stage/Camera mutation, while a test double
// (tests/host/agent/fake_view_model.h) implements it purely in-memory.
//
// Unit split: `view_of` returns a fully-formed ViewState already converted
// to agent units (rotation in degrees, zoom as a linear multiplier, channel
// as "all"/index). The setters below, by contrast, take engine units
// (radians, zoom power, mode/index) -- AgentCore performs the agent<->engine
// conversion at the boundary using ZOOM_FACTOR, and the native adapter
// simply forwards the already-converted engine value to Camera/Stage.
class ViewModel {
  public:
    virtual ~ViewModel() = default;

    // enumeration + readback
    virtual std::size_t buffer_count() = 0;
    virtual std::optional<BufferInfo> buffer_at(std::size_t i) = 0;
    virtual std::optional<BufferInfo> buffer_named(std::string_view name) = 0;
    virtual bool read_pixels(std::string_view name,
                             std::vector<std::byte>& out) = 0;

    // selection + view state (operate on the named buffer's Stage/Camera)
    virtual bool select(std::string_view name) = 0;
    virtual std::optional<std::string> selected_name() = 0;
    virtual std::optional<ViewState> view_of(std::string_view name) = 0;
    virtual bool set_center(std::string_view name, double x, double y) = 0;
    virtual bool set_zoom_power(std::string_view name, double power) = 0;
    virtual bool set_rotation_rad(std::string_view name, double radians) = 0;
    // channel: mode==-1 => "all" (restore natural layout); else index 0..2
    virtual bool set_channel(std::string_view name, int mode, int index) = 0;

    // global + viewport
    virtual bool auto_contrast() = 0;
    virtual void set_auto_contrast(bool enabled) = 0;
    virtual std::pair<int, int> viewport_size() = 0;

    // zoom math constant so AgentCore converts multiplier<->power identically
    static constexpr double ZOOM_FACTOR = 1.1; // mirrors Camera::ZOOM_FACTOR
};

} // namespace oid::host::agent

#endif // HOST_AGENT_VIEW_MODEL_H_
