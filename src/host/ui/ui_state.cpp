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

#include "host/ui/ui_state.h"

#include <deque>
#include <unordered_map>

#include "host/ui/symbol_filter.h"
#include "host/ui/text_input.h"
#include "host/util/transparent_string_hash.h"

namespace oid::host {

UiState::UiState(const BufferModel& model) : model_(model) {}

std::size_t UiState::selected() const {
    if (model_.size() == 0) {
        return 0;
    }
    return selected_ < model_.size() ? selected_ : model_.size() - 1;
}

void UiState::select(std::size_t i) {
    selected_ = i;
}

bool UiState::has_selection() const {
    return model_.size() > 0;
}

void UiState::set_query(std::string q) {
    query_ = std::move(q);
}

const std::string& UiState::query() const {
    return query_;
}

std::vector<std::size_t> UiState::filtered_indices() const {
    std::vector<std::string> candidates;
    candidates.reserve(model_.size());
    for (std::size_t i = 0; i < model_.size(); ++i) {
        candidates.push_back(model_.at(i).display_name);
    }

    const std::vector<std::string> matches = filter_symbols(candidates, query_);

    // filter_symbols() only returns display_name strings, and display
    // names need not be unique across records, so map each match back to
    // a model index by display_name via a FIFO queue per name: since
    // filter_symbols() sorts with std::stable_sort, candidates sharing a
    // display_name keep their original relative (index) order, so
    // popping from the front of each name's queue in match order pairs
    // each match with the correct, not-yet-consumed model index.
    std::unordered_map<std::string,
                       std::deque<std::size_t>,
                       TransparentStringHash,
                       std::equal_to<>>
        by_name;
    for (std::size_t i = 0; i < candidates.size(); ++i) {
        by_name[candidates[i]].push_back(i);
    }

    std::vector<std::size_t> indices;
    indices.reserve(matches.size());
    for (const auto& match : matches) {
        auto& queue = by_name[match];
        indices.push_back(queue.front());
        queue.pop_front();
    }

    return indices;
}

void UiState::set_available_symbols(std::vector<std::string> symbols) {
    available_symbols_ = std::move(symbols);
}

std::vector<std::string> UiState::filtered_symbols() const {
    return filter_symbols(available_symbols_, query_);
}

std::optional<std::size_t>
UiState::model_index_of(const std::string_view variable_name) const {
    for (std::size_t i = 0; i < model_.size(); ++i) {
        if (model_.variable_name_of(i) == variable_name) {
            return i;
        }
    }
    return std::nullopt;
}

bool UiState::contrast_enabled() const {
    return contrast_;
}

void UiState::set_contrast_enabled(const bool enabled) {
    contrast_ = enabled;
}

bool UiState::link_views() const {
    return link_views_;
}

void UiState::set_link_views(const bool enabled) {
    link_views_ = enabled;
}

bool UiState::ac_editor_visible() const {
    return ac_editor_visible_;
}

void UiState::set_ac_editor_visible(const bool visible) {
    ac_editor_visible_ = visible;
}

std::optional<std::pair<int, int>>
UiState::parse_goto(const std::string_view x, const std::string_view y) {
    const auto px = parse_int_field(x);
    const auto py = parse_int_field(y);
    if (!px.has_value() || !py.has_value()) {
        return std::nullopt;
    }
    return std::make_pair(*px, *py);
}

void UiState::set_status_message(std::string msg) {
    status_message_ = std::move(msg);
}

const std::string& UiState::status_message() const {
    return status_message_;
}

} // namespace oid::host
