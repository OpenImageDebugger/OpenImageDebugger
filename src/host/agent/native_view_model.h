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

#ifndef HOST_AGENT_NATIVE_VIEW_MODEL_H_
#define HOST_AGENT_NATIVE_VIEW_MODEL_H_

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "host/agent/view_model.h"
#include "host/ui/ipc_buffer_model.h"
#include "host/ui/stage_manager.h"
#include "host/ui/ui_state.h"
#include "visualization/render_canvas.h"
#include "visualization/stage.h"

namespace oid::host::agent {

// Every mutator MUST run on the GL thread; this class adds no locking.
class NativeViewModel final : public ViewModel {
  public:
    NativeViewModel(IpcBufferModel& model,
                    StageManager& stages,
                    UiState& ui,
                    std::shared_ptr<RenderCanvas> canvas);

    std::size_t buffer_count() override;
    std::optional<BufferInfo> buffer_at(std::size_t i) override;
    std::optional<BufferInfo> buffer_named(std::string_view name) override;
    bool read_pixels(std::string_view name,
                     std::vector<std::byte>& out) override;

    bool select(std::string_view name) override;
    std::optional<std::string> selected_name() override;
    std::optional<ViewState> view_of(std::string_view name) override;
    bool set_center(std::string_view name, double x, double y) override;
    bool set_zoom_power(std::string_view name, double power) override;
    bool set_rotation_rad(std::string_view name, double radians) override;
    bool set_channel(std::string_view name, int mode, int index) override;

    bool auto_contrast() override;
    void set_auto_contrast(bool enabled) override;
    std::pair<int, int> viewport_size() override;

  private:
    [[nodiscard]] Stage* stage_for_name(std::string_view name) const;

    IpcBufferModel& model_;
    StageManager& stages_;
    UiState& ui_;
    std::shared_ptr<RenderCanvas> canvas_;
};

} // namespace oid::host::agent

#endif // HOST_AGENT_NATIVE_VIEW_MODEL_H_
