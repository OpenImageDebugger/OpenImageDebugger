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

#include <gtest/gtest.h>

#include "host/ui/buffer_model.h"

using oid::host::make_default_mock_model;
using oid::host::MockBufferModel;
using oid::host::UiState;

TEST(UiState, SelectionClampsToModel) {
    const MockBufferModel m = make_default_mock_model();
    UiState s{m};
    s.select(999);
    EXPECT_LT(s.selected(), m.size());
}

TEST(UiState, SelectionStartsAtZero) {
    const MockBufferModel m = make_default_mock_model();
    const UiState s{m};
    EXPECT_EQ(s.selected(), 0u);
    EXPECT_TRUE(s.has_selection());
}

TEST(UiState, SelectionInRangeIsUnchanged) {
    const MockBufferModel m = make_default_mock_model();
    UiState s{m};
    s.select(1);
    EXPECT_EQ(s.selected(), 1u);
}

TEST(UiState, FilteredIndicesFollowQuery) {
    const MockBufferModel m = make_default_mock_model();
    UiState s{m};

    s.set_query("nonexistent_zzz");
    EXPECT_TRUE(s.filtered_indices().empty());

    s.set_query("");
    EXPECT_EQ(s.filtered_indices().size(), m.size());
}

TEST(UiState, FilteredIndicesMapBackToModelRecords) {
    const MockBufferModel m = make_default_mock_model();
    UiState s{m};

    s.set_query("gradient");
    const auto indices = s.filtered_indices();
    ASSERT_EQ(indices.size(), 1u);
    EXPECT_EQ(m.at(indices[0]).display_name, "gradient (64x64 rgb8)");
}

TEST(UiState, ToolbarFlagsDefaultAndToggle) {
    const MockBufferModel m = make_default_mock_model();
    UiState s{m};

    EXPECT_TRUE(s.contrast_enabled());
    EXPECT_FALSE(s.link_views());

    s.set_contrast_enabled(false);
    s.set_link_views(true);
    EXPECT_FALSE(s.contrast_enabled());
    EXPECT_TRUE(s.link_views());
}

TEST(UiState, ParseGotoRejectsGarbageInEitherField) {
    const MockBufferModel m = make_default_mock_model();
    UiState s{m};

    EXPECT_EQ(s.parse_goto("x", "4"), std::nullopt);
    EXPECT_EQ(s.parse_goto("3", "y"), std::nullopt);
    EXPECT_EQ(s.parse_goto("", ""), std::nullopt);
}

TEST(UiState, ParseGotoAcceptsBothNumeric) {
    const MockBufferModel m = make_default_mock_model();
    UiState s{m};

    EXPECT_EQ(s.parse_goto("3", "4"), (std::pair{3, 4}));
}
