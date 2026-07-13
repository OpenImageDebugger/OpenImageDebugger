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

#include "host/io/file_open_queue.h"

#include <string>
#include <string_view>
#include <vector>

#include "host/io/expected.h"

#include <gtest/gtest.h>

using namespace oid::host;

namespace {

BufferRecord record_named(std::string_view name) {
    BufferRecord record;
    record.variable_name = std::string(name);
    record.display_name = std::string(name);
    record.kind = BufferKind::LOCAL_FILE;
    return record;
}

} // namespace

TEST(FileOpenQueueTest, StartsEmpty) {
    FileOpenQueue queue;
    EXPECT_TRUE(queue.empty());
}

TEST(FileOpenQueueTest, DrainLoadsAndUpsertsEach) {
    FileOpenQueue queue;
    queue.push_all({"a.png", "b.png"});
    EXPECT_FALSE(queue.empty());

    std::vector<std::string> upserted;
    const auto outcome = queue.drain(
        [](const std::string& path) -> oid::Expected<BufferRecord> {
            return record_named(path);
        },
        [&upserted](const BufferRecord& record) {
            upserted.push_back(record.variable_name);
        });

    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(outcome.succeeded, 2);
    EXPECT_EQ(outcome.failed, 0);
    ASSERT_EQ(upserted.size(), 2u);
    EXPECT_EQ(upserted[0], "a.png");
    EXPECT_EQ(upserted[1], "b.png");
    EXPECT_EQ(outcome.last_success, "b.png");
}

TEST(FileOpenQueueTest, DrainRecordsFailuresAndContinues) {
    FileOpenQueue queue;
    queue.push_all({"good.png", "bad.png"});

    int upsert_calls = 0;
    const auto outcome = queue.drain(
        [](const std::string& path) -> oid::Expected<BufferRecord> {
            if (path == "bad.png") {
                return oid::make_error("decode failed");
            }
            return record_named(path);
        },
        [&upsert_calls](const BufferRecord&) { ++upsert_calls; });

    EXPECT_EQ(outcome.succeeded, 1);
    EXPECT_EQ(outcome.failed, 1);
    EXPECT_EQ(outcome.last_error, "decode failed");
    EXPECT_EQ(upsert_calls, 1);
}

TEST(FileOpenQueueTest, DrainOfEmptyQueueIsNoop) {
    FileOpenQueue queue;
    int loader_calls = 0;
    const auto outcome = queue.drain(
        [&loader_calls](const std::string&) -> oid::Expected<BufferRecord> {
            ++loader_calls;
            return record_named("x");
        },
        [](const BufferRecord&) { /* empty queue never upserts */ });
    EXPECT_EQ(loader_calls, 0);
    EXPECT_EQ(outcome.succeeded, 0);
    EXPECT_EQ(outcome.failed, 0);
}
