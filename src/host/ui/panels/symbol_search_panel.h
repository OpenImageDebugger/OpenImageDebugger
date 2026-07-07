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

#ifndef HOST_UI_PANELS_SYMBOL_SEARCH_PANEL_H_
#define HOST_UI_PANELS_SYMBOL_SEARCH_PANEL_H_

namespace oid::host {

class UiState;
class IpcClient;

// Draws the symbol search box (parity with the legacy Qt frontend's
// SymbolSearchInput and SymbolCompleter; see tag legacy-qt): a single-line
// text field whose contents drive UiState::set_query(), and,
// while the query is non-empty, a list of matching symbols below it (one
// Selectable per UiState::filtered_symbols() entry -- the debugger's full
// set of observable symbols, whether or not they're currently plotted, not
// just the already-loaded buffers).
//
// Clicking a result -- or pressing Enter/Return in the field while at least
// one result matches -- commits that symbol and clears the query, closing
// the results list (parity with the Qt completer committing a highlighted
// match and clearing the input). If the symbol is already loaded
// (UiState::model_index_of() has a value), it just selects that buffer via
// UiState::select(); otherwise it requests the debugger plot it via
// `ipc.request_plot()`, and the buffer will appear once the
// PLOT_BUFFER_CONTENTS/Chunk response arrives on a later IpcClient::poll()`.
//
// `focus_request` mirrors the Qt app's Ctrl+K shortcut
// (symbolList->setFocus()): when true, the field grabs keyboard focus this
// frame via ImGui::SetKeyboardFocusHere(). Callers (main.cpp) compute
// it once per frame from the Ctrl+K key press.
//
// The query edit buffer is panel-local `static` state (not part of
// UiState): it is pure ImGui widget plumbing, mirrored into UiState via
// set_query() so filtered_symbols() stays the single source of truth for
// *which* symbols match.
void draw_symbol_search(UiState& ui, IpcClient& ipc, bool focus_request);

} // namespace oid::host

#endif // HOST_UI_PANELS_SYMBOL_SEARCH_PANEL_H_
