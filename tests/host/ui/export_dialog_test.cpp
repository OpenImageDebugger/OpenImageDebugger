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

// Exercises the pure export-path helpers (default_export_path,
// open_export_dialog, classify_export_format, ensure_export_extension,
// set_export_path) declared in host/ui/export_dialog.h. None of them touch
// ImGui/GL, so this test binary never needs to link either.

#include "host/ui/export_dialog.h"

#include <string_view>

#include <gtest/gtest.h>

using oid::BufferExporter::OutputType;
using oid::host::classify_export_format;
using oid::host::default_export_path;
using oid::host::ensure_export_extension;
using oid::host::ExportDialogState;
using oid::host::open_export_dialog;
using oid::host::set_export_path;

TEST(ExportDialog, DefaultPathPrefersLastExportDir) {
    EXPECT_EQ(default_export_path("/exp", "/home/x", "buf", OutputType::BITMAP),
              "/exp/buf.png");
    EXPECT_EQ(default_export_path(
                  "/exp", "/home/x", "buf", OutputType::OCTAVE_MATRIX),
              "/exp/buf.oct");
}

TEST(ExportDialog, DefaultPathFallsBackToHomeDesktop) {
    EXPECT_EQ(default_export_path("", "/home/x", "buf", OutputType::BITMAP),
              "/home/x/Desktop/buf.png");
}

TEST(ExportDialog, DefaultPathFallsBackToDotWhenHomeUnset) {
    EXPECT_EQ(default_export_path("", nullptr, "buf", OutputType::BITMAP),
              "./buf.png");
    EXPECT_EQ(default_export_path("", "", "buf", OutputType::OCTAVE_MATRIX),
              "./buf.oct");
}

TEST(ExportDialog, DefaultPathStripsTrailingSlashFromDir) {
    EXPECT_EQ(
        default_export_path("/exp/", "/home/x", "buf", OutputType::BITMAP),
        "/exp/buf.png");
}

TEST(ExportDialog, OpenExportDialogSeedsStateFromBufferAndDir) {
    ExportDialogState st;
    open_export_dialog(st, "buf", "/exp");
    EXPECT_TRUE(st.open);
    EXPECT_EQ(st.buffer_name, "buf");
    EXPECT_EQ(st.format, OutputType::BITMAP);
    EXPECT_STREQ(st.path_buf.data(), "/exp/buf.png");
}

TEST(ExportDialog, OpenResetsStateFromPriorOpen) {
    ExportDialogState st;
    open_export_dialog(st, "first", "/exp");
    st.format = OutputType::OCTAVE_MATRIX;

    open_export_dialog(st, "second", "/exp");
    EXPECT_EQ(st.buffer_name, "second");
    EXPECT_EQ(st.format, OutputType::BITMAP);
    EXPECT_STREQ(st.path_buf.data(), "/exp/second.png");
}

TEST(ExportDialog, ClassifyFormatFromExtension) {
    EXPECT_EQ(classify_export_format("/a/buf.oct"), OutputType::OCTAVE_MATRIX);
    EXPECT_EQ(classify_export_format("/a/buf.png"), OutputType::BITMAP);
    EXPECT_EQ(classify_export_format("/a/buf"), OutputType::BITMAP);
    EXPECT_EQ(classify_export_format("buf.OCT"),
              OutputType::BITMAP); // case-sensitive
}

TEST(ExportDialog, EnsureExtensionAppendsWhenMissing) {
    EXPECT_EQ(ensure_export_extension("/a/buf", OutputType::BITMAP),
              "/a/buf.png");
    EXPECT_EQ(ensure_export_extension("/a/buf", OutputType::OCTAVE_MATRIX),
              "/a/buf.oct");
}

TEST(ExportDialog, EnsureExtensionNoOpWhenPresent) {
    EXPECT_EQ(ensure_export_extension("/a/buf.png", OutputType::BITMAP),
              "/a/buf.png");
    EXPECT_EQ(ensure_export_extension("/a/buf.oct", OutputType::OCTAVE_MATRIX),
              "/a/buf.oct");
}

TEST(ExportDialog, SetExportPathCopiesIntoBuffer) {
    ExportDialogState st;
    set_export_path(st, "/exp/img.png");
    EXPECT_STREQ(st.path_buf.data(), "/exp/img.png");
}
