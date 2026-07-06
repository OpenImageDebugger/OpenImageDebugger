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

#include "host/ui/panels/symbol_search_panel.h"

#include <cfloat>
#include <cstddef>
#include <string>
#include <vector>

#include <imgui.h>

#include "host/ipc/ipc_client.h"
#include "host/ui/symbol_filter.h"
#include "host/ui/text_input.h"
#include "host/ui/ui_state.h"

namespace oid::host {

namespace {

// Selects `name` if it's already a loaded buffer, otherwise asks the
// debugger to plot it; the buffer will show up once IpcClient::poll()
// decodes the response on a later frame.
void commit_symbol(UiState& ui, IpcClient& ipc, const std::string& name) {
    if (const std::optional<std::size_t> idx = ui.model_index_of(name);
        idx.has_value()) {
        ui.select(*idx);
    } else {
        ipc.request_plot(name);
    }
}

} // namespace

void draw_symbol_search(UiState& ui, IpcClient& ipc, bool focus_request) {
    // Panel-local ImGui edit buffer, not UiState: pure widget plumbing,
    // mirrored into UiState::set_query() below so filtered_symbols() stays
    // the single source of truth for which symbols match.
    static std::string query_buf;
    // Highlighted suggestion index, moved by Up/Down (symbol_completion_nav).
    static int highlight_index = 0;
    // Escape hides the suggestion list while keeping the typed text; cleared
    // when the user types again or re-focuses the field.
    static bool list_dismissed = false;

    // Ctrl+K (main.cpp): grab keyboard focus this frame, mirroring
    // the Qt app's symbolList->setFocus(). Must be called before InputText
    // so ImGui applies it to the item about to be drawn.
    if (focus_request) {
        ImGui::SetKeyboardFocusHere();
        list_dismissed = false;
    }

    // Snapshot the text before the widget runs: a single-line InputText
    // reverts its buffer to the focus-time value when Escape is pressed, so we
    // restore this below to keep the user's typed text (Qt parity: Escape
    // clears focus without clearing the field).
    const std::string query_before = query_buf;

    // Qt parity: the legacy Qt frontend's SymbolSearchInput placeholderText
    // (see tag legacy-qt), and the QLineEdit stretches to the pane width --
    // fill the available width
    // instead of ImGui's default ~2/3 item width.
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (input_text_line_with_hint(
            "##symbol_search", "add symbols (ctrl+k)", query_buf)) {
        ui.set_query(query_buf);
        highlight_index = 0;    // reset highlight on any edit
        list_dismissed = false; // typing re-opens the list
    }

    const bool input_active = ImGui::IsItemActive();
    // Re-focusing the field (click or Ctrl+K) re-enables the suggestion list.
    if (ImGui::IsItemActivated()) {
        list_dismissed = false;
    }

    // Escape dismisses the suggestion list but keeps the typed text. Like
    // Enter, a single-line InputText deactivates on the Escape frame (so
    // ImGui::IsItemActive() is already false); detect it via
    // IsItemDeactivated() gated on the Escape key, and undo ImGui's
    // revert-to-focus-value so the text the user typed stays in the box.
    if (ImGui::IsItemDeactivated() &&
        ImGui::IsKeyPressed(ImGuiKey_Escape, /*repeat=*/false)) {
        query_buf = query_before;
        ui.set_query(query_buf);
        highlight_index = 0;
        list_dismissed = true;
        return;
    }

    // Compute the matches once per frame and reuse for the arrow navigation,
    // the Enter shortcut, and the results list. Suppressed once the list is
    // dismissed so a kept query doesn't keep the dropdown open.
    const std::vector<std::string> matches =
        (query_buf.empty() || list_dismissed) ? std::vector<std::string>{}
                                              : ui.filtered_symbols();

    // Up/Down move highlight through suggestion list (repeat=true so
    // holding key keeps moving, like real completer popup).
    const bool up =
        input_active && ImGui::IsKeyPressed(ImGuiKey_UpArrow, /*repeat=*/true);
    const bool down = input_active &&
                      ImGui::IsKeyPressed(ImGuiKey_DownArrow, /*repeat=*/true);
    highlight_index = symbol_completion_nav(
        highlight_index, static_cast<int>(matches.size()), up, down);

    // Enter/Return commits the highlighted match, same as the Qt
    // SymbolCompleter accepting its highlighted suggestion. A single-line
    // InputText deactivates itself on the frame Enter is pressed, so
    // ImGui::IsItemActive() is already false here on that frame -- gating the
    // commit on it (as the arrow keys are) would silently drop every Enter.
    // Detect the commit via IsItemDeactivated() (fires whenever the field
    // loses active state, edited or not -- so it still works after Escape ->
    // refocus -> arrow with no re-typing) gated on the Enter key so clicking
    // away or tabbing out of the field does not commit. The Escape branch
    // above already returned, so it never double-fires here.
    if (const bool enter_pressed =
            ImGui::IsItemDeactivated() &&
            (ImGui::IsKeyPressed(ImGuiKey_Enter, /*repeat=*/false) ||
             ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, /*repeat=*/false));
        enter_pressed && !matches.empty()) {
        commit_symbol(
            ui, ipc, matches[static_cast<std::size_t>(highlight_index)]);
        query_buf.clear();
        ui.set_query("");
        highlight_index = 0;
        return;
    }

    // Results list: only while there's an active query, so an empty search
    // box doesn't dump every symbol underneath it. Stop after a click: the
    // selection clears the query, so continuing to draw the (now stale)
    // remaining rows this frame would be wrong.
    for (int i = 0; i < static_cast<int>(matches.size()); ++i) {
        const std::string& name = matches[static_cast<std::size_t>(i)];
        ImGui::PushID(name.c_str());
        const bool clicked =
            ImGui::Selectable(name.c_str(),
                              /*selected=*/i == highlight_index);
        ImGui::PopID();
        if (clicked) {
            commit_symbol(ui, ipc, name);
            query_buf.clear();
            ui.set_query("");
            highlight_index = 0;
            return;
        }
    }
}

} // namespace oid::host
