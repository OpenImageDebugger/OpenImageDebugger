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

#include "host/ui/symbol_filter.h"

#include <gtest/gtest.h>

using oid::host::filter_symbols;

// Semantics match Qt's QStringList::filter(): a literal case-insensitive
// QString::contains() test -- contiguous substring, not prefix-only and
// not fuzzy/subsequence matching.
TEST(SymbolFilter, CaseInsensitiveSubstringSorted) {
    std::vector<std::string> c{"imgOut", "BigImg", "tmp", "my_img_data", "BUF"};
    EXPECT_EQ(filter_symbols(c, "img"),
              (std::vector<std::string>{"BigImg", "imgOut", "my_img_data"}));
}

TEST(SymbolFilter, EmptyQueryReturnsAllSorted) {
    std::vector<std::string> c{"b", "A", "c"};
    EXPECT_EQ(filter_symbols(c, ""), (std::vector<std::string>{"A", "b", "c"}));
}

TEST(SymbolFilter, NoMatchesReturnsEmpty) {
    std::vector<std::string> c{"foo", "bar", "baz"};
    EXPECT_TRUE(filter_symbols(c, "xyz").empty());
}

TEST(SymbolFilter, MatchesSubstringAnywhereInString) {
    std::vector<std::string> c{"prefix_mid_suffix", "unrelated"};
    EXPECT_EQ(filter_symbols(c, "mid"),
              (std::vector<std::string>{"prefix_mid_suffix"}));
}

TEST(SymbolFilter, EmptyCandidatesReturnsEmpty) {
    std::vector<std::string> c{};
    EXPECT_TRUE(filter_symbols(c, "img").empty());
}

using oid::host::symbol_completion_nav;

TEST(SymbolCompletionNav, DownAdvancesAndStopsAtLast) {
    EXPECT_EQ(symbol_completion_nav(0, 3, /*up=*/false, /*down=*/true), 1);
    EXPECT_EQ(symbol_completion_nav(1, 3, /*up=*/false, /*down=*/true), 2);
    EXPECT_EQ(symbol_completion_nav(2, 3, /*up=*/false, /*down=*/true), 2);
}

TEST(SymbolCompletionNav, UpRetreatsAndStopsAtFirst) {
    EXPECT_EQ(symbol_completion_nav(2, 3, /*up=*/true, /*down=*/false), 1);
    EXPECT_EQ(symbol_completion_nav(0, 3, /*up=*/true, /*down=*/false), 0);
}

TEST(SymbolCompletionNav, NoMatchesIsAlwaysZero) {
    EXPECT_EQ(symbol_completion_nav(5, 0, /*up=*/false, /*down=*/true), 0);
    EXPECT_EQ(symbol_completion_nav(0, 0, /*up=*/true, /*down=*/false), 0);
}

TEST(SymbolCompletionNav, BothOrNeitherOnlyClamps) {
    EXPECT_EQ(symbol_completion_nav(1, 3, /*up=*/true, /*down=*/true), 1);
    EXPECT_EQ(symbol_completion_nav(1, 3, /*up=*/false, /*down=*/false), 1);
}

TEST(SymbolCompletionNav, OutOfRangeCurrentIsClamped) {
    EXPECT_EQ(symbol_completion_nav(5, 3, /*up=*/false, /*down=*/false), 2);
    EXPECT_EQ(symbol_completion_nav(-4, 3, /*up=*/false, /*down=*/false), 0);
}
