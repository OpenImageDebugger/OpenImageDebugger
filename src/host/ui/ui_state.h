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

#ifndef HOST_UI_UI_STATE_H_
#define HOST_UI_UI_STATE_H_

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "host/ui/buffer_model.h"

namespace oid::host {

// Pure (no GL, no ImGui runtime) UI-logic layer that chrome panels
// read/write: current buffer selection, the symbol-search
// query and its filtered results, go-to text parsing, and the toolbar
// flags mirrored from the Qt app (auto-contrast, link views). Holds no
// GL/ImGui state itself -- StageManager turns `selected()` into a
// renderable Stage, and panels own the actual ImGui widgets.
class UiState {
  public:
    explicit UiState(const BufferModel& model);

    // Currently selected buffer index, clamped to
    // [0, model.size() - 1]; 0 when the model is empty.
    std::size_t selected() const;

    // Selects buffer `i`; clamped lazily by selected().
    void select(std::size_t i);

    bool has_selection() const;

    // Symbol search.
    void set_query(std::string q);
    const std::string& query() const;

    // Model indices whose display_name matches query(), filtered and
    // ordered by filter_symbols(). An empty query matches every
    // record, in filter_symbols' case-insensitive-sorted order.
    std::vector<std::size_t> filtered_indices() const;

    // Debugger-side symbol list: every variable the debugger
    // reports as observable, whether or not it's currently plotted. Set
    // once per frame from IpcClient::available_symbols() in main.cpp.
    void set_available_symbols(std::vector<std::string> symbols);

    // available_symbols(), filtered and ordered by filter_symbols() against
    // query(); the symbol-search panel lists these instead of
    // filtered_indices() so unplotted symbols show up too.
    std::vector<std::string> filtered_symbols() const;

    // Index of the loaded buffer whose variable_name matches `variable_name`,
    // if any; used by the symbol-search panel to tell "already plotted --
    // just select it" apart from "not loaded yet -- request a plot".
    std::optional<std::size_t>
    model_index_of(std::string_view variable_name) const;

    // Toolbar flags mirrored from the Qt app.
    bool contrast_enabled() const;
    void set_contrast_enabled(bool enabled);

    bool link_views() const;
    void set_link_views(bool enabled);

    // Qt parity: acEdit toolbar toggle (see tag legacy-qt) shows/hides the
    // min/max intensity editor; default checked.
    bool ac_editor_visible() const;
    void set_ac_editor_visible(bool visible);

    // Go-to text staging: parses `x`/`y` with parse_int_field()
    // and returns {x, y} only if both fields parse; std::nullopt
    // otherwise. Parsing only -- moving the camera is the panel's job.
    std::optional<std::pair<int, int>> parse_goto(std::string_view x,
                                                  std::string_view y) const;

    // Status-bar message: a transient, ui-level notice -- e.g. the
    // most recent export's outcome -- shown appended to the status bar
    // (see draw_status_bar()). No expiry/timing logic here: it stays until
    // a later set_status_message() call replaces or clears it.
    void set_status_message(std::string msg);
    const std::string& status_message() const;

  private:
    const BufferModel& model_;
    std::size_t selected_{0};
    std::string query_{};
    std::vector<std::string> available_symbols_{};
    bool contrast_{true};
    bool link_views_{false};
    bool ac_editor_visible_{true};
    std::string status_message_{};
};

} // namespace oid::host

#endif // HOST_UI_UI_STATE_H_
