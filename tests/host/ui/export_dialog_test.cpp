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

// Exercises only the pure helpers (default_export_path,
// apply_format_extension, open_export_dialog) declared in
// host/ui/export_dialog.h. draw_export_dialog() lives in a separate TU
// (export_dialog_draw.cpp, see its comment) precisely so this test binary
// never needs to link ImGui/GL at all.

#include "host/ui/export_dialog.h"

#include <format>

#include <gtest/gtest.h>

using oid::BufferExporter::OutputType;
using oid::host::apply_format_extension;
using oid::host::default_export_path;
using oid::host::ExportDialogState;
using oid::host::open_export_dialog;

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
    EXPECT_FALSE(st.user_edited_path);
    EXPECT_EQ(st.format, OutputType::BITMAP);
    EXPECT_STREQ(st.path_buf.data(), "/exp/buf.png");
}

TEST(ExportDialog, FormatSwapReplacesExtensionWhenPathNotUserEdited) {
    ExportDialogState st;
    open_export_dialog(st, "buf", "/exp");
    ASSERT_STREQ(st.path_buf.data(), "/exp/buf.png");

    st.format = OutputType::OCTAVE_MATRIX;
    apply_format_extension(st);
    EXPECT_STREQ(st.path_buf.data(), "/exp/buf.oct");

    st.format = OutputType::BITMAP;
    apply_format_extension(st);
    EXPECT_STREQ(st.path_buf.data(), "/exp/buf.png");
}

TEST(ExportDialog, FormatSwapAppendsExtensionWhenPathHasNeither) {
    ExportDialogState st;
    const auto res = std::format_to_n(
        st.path_buf.data(), st.path_buf.size() - 1, "{}", "/exp/noext");
    *res.out = '\0';
    st.format = OutputType::OCTAVE_MATRIX;
    apply_format_extension(st);
    EXPECT_STREQ(st.path_buf.data(), "/exp/noext.oct");
}

TEST(ExportDialog, FormatSwapRespectsUserEditedPath) {
    ExportDialogState st;
    open_export_dialog(st, "buf", "/exp");
    const auto res = std::format_to_n(
        st.path_buf.data(), st.path_buf.size() - 1, "{}", "/exp/custom.name");
    *res.out = '\0';
    st.user_edited_path = true;

    st.format = OutputType::OCTAVE_MATRIX;
    apply_format_extension(st);
    EXPECT_STREQ(st.path_buf.data(), "/exp/custom.name");
}

TEST(ExportDialog, OpenResetsFormatAndUserEditedFlagFromPriorOpen) {
    ExportDialogState st;
    open_export_dialog(st, "first", "/exp");
    st.format = OutputType::OCTAVE_MATRIX;
    apply_format_extension(st);
    const auto res = std::format_to_n(
        st.path_buf.data(), st.path_buf.size() - 1, "{}", "/exp/custom.name");
    *res.out = '\0';
    st.user_edited_path = true;

    open_export_dialog(st, "second", "/exp");
    EXPECT_EQ(st.buffer_name, "second");
    EXPECT_FALSE(st.user_edited_path);
    EXPECT_EQ(st.format, OutputType::BITMAP);
    EXPECT_STREQ(st.path_buf.data(), "/exp/second.png");
}
