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

#ifndef HOST_UI_SYMBOL_FILTER_H_
#define HOST_UI_SYMBOL_FILTER_H_

#include <string>
#include <string_view>
#include <vector>

namespace oid::host {

// Qt-free reproduction of the legacy Qt frontend's symbol completer
// matching (see tag legacy-qt), which filtered with
// QStringList::filter(word, Qt::CaseInsensitive) and displayed results
// through a CaseInsensitivelySortedModel. Returns the subset of `candidates`
// that contain `query` as a case-insensitive substring, sorted
// case-insensitively. An empty `query` matches every candidate.
std::vector<std::string>
filter_symbols(const std::vector<std::string>& candidates,
               std::string_view query);

// Highlight-movement navigation for autocomplete dropdowns. Pure, testable
// helper (no ImGui/Qt/GLFW dependencies). Given a current index, returns the
// new index after one Up/Down step over `match_count` items. Movement stops at
// the ends (no wrap), matching QCompleter's default popup navigation. With no
// matches the result is 0. `up`/`down` are this frame's key-pressed flags; if
// both or neither are set, the index is only clamped, not moved. Drives the
// symbol-search autocomplete highlight (see symbol_search_panel.cpp).
int symbol_completion_nav(int current, int match_count, bool up, bool down);

} // namespace oid::host

#endif // HOST_UI_SYMBOL_FILTER_H_
