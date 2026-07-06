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

#include "host/settings/session_buffers.h"

#include <gtest/gtest.h>

using namespace oid::host;

TEST(MergePreviousBuffers, LoadedGetFreshExpiryDeletedDropped) {
    std::vector<PreviousBuffer> prior = {
        {"a", 5}, {"gone", 999999}, {"stale", 1}};
    std::vector<std::string> loaded = {"a"}; // only "a" loaded now
    std::set<std::string, std::less<>> seen = {
        "a", "gone"}; // "gone" was loaded then removed
    const auto out =
        merge_previous_buffers(prior, loaded, seen, /*now*/ 100, /*ttl*/ 50);
    // "a": loaded -> fresh expiry 150; "gone": seen+absent -> dropped;
    // "stale": not seen but expired (1<100) -> dropped.
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].variable_name, "a");
    EXPECT_EQ(out[0].expiry_epoch_s, 150);
}

TEST(MergePreviousBuffers, NotYetReloadedUnexpiredSurvives) {
    std::vector<PreviousBuffer> prior = {{"later", 1000}};
    const auto out =
        merge_previous_buffers(prior, {}, {}, /*now*/ 100, /*ttl*/ 50);
    ASSERT_EQ(out.size(), 1u); // never seen, not expired -> kept as-is
    EXPECT_EQ(out[0].variable_name, "later");
    EXPECT_EQ(out[0].expiry_epoch_s, 1000);
}
