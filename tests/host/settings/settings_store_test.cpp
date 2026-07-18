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

#include "host/settings/settings_store.h"
#include "support/scratch_dir.h"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
using namespace oid::host;

static std::filesystem::path temp_file(const std::string& name) {
    static const std::filesystem::path dir = oid::test::scratch_dir();
    return dir / ("oid_p4_" + name + ".json");
}

TEST(SettingsStore, RoundTrip) {
    const auto p = temp_file("roundtrip");
    AppSettings s;
    s.window_w = 800;
    s.window_h = 600;
    s.window_x = 40;
    s.window_y = 30;
    s.left_pane_w = 300.0f;
    s.contrast_enabled = false;
    s.link_views = true;
    s.previous_buffers = {{"a", 111}, {"b", 222}};
    SettingsStore{p}.save(s);
    const AppSettings out = SettingsStore{p}.load();
    EXPECT_EQ(out, s);
}

TEST(SettingsStore, MissingFileYieldsDefaults) {
    EXPECT_EQ(SettingsStore{temp_file("missing")}.load(), AppSettings{});
}

TEST(SettingsStore, GarbageYieldsDefaults) {
    const auto p = temp_file("garbage");
    std::ofstream{p} << "}{ not json";
    EXPECT_EQ(SettingsStore{p}.load(), AppSettings{});
}

TEST(SettingsStore, PartialJsonKeepsPresentFieldsRestDefault) {
    const auto p = temp_file("partial");
    std::ofstream{p} << R"({"ui":{"leftPaneWidth":150.0,"linkViews":true}})";
    const AppSettings out = SettingsStore{p}.load();
    EXPECT_FLOAT_EQ(out.left_pane_w, 150.0f);
    EXPECT_TRUE(out.link_views);
    EXPECT_TRUE(out.contrast_enabled);      // default kept
    EXPECT_EQ(out.window_w, 1024);          // default kept
    EXPECT_FALSE(out.window_x.has_value()); // omitted -> nullopt
}

TEST(SettingsStore, WrongTypedFieldFallsBackToDefault) {
    const auto p = temp_file("wrongtype");
    std::ofstream{p}
        << R"({"window":{"w":"not-an-int"},"ui":{"contrastEnabled":false}})";
    const AppSettings out = SettingsStore{p}.load();
    EXPECT_EQ(out.window_w, 1024); // bad type -> default
    EXPECT_FALSE(out.contrast_enabled);
}

TEST(SettingsStore, LastExportDirRoundTripsAndDefaultsEmpty) {
    const auto p = temp_file("exportdir");
    AppSettings s;
    s.last_export_dir = "/home/user/exports";
    SettingsStore{p}.save(s);
    EXPECT_EQ(SettingsStore{p}.load().last_export_dir, "/home/user/exports");
    std::ofstream{temp_file("noexportdir")} << R"({"version":1})";
    EXPECT_EQ(SettingsStore{temp_file("noexportdir")}.load().last_export_dir,
              "");
}

TEST(SettingsStore, OutOfRangeWindowSizeFallsBackToDefault) {
    {
        const auto p = temp_file("zerosize");
        std::ofstream{p} << R"({"window":{"w":0,"h":0}})";
        const AppSettings out = SettingsStore{p}.load();
        EXPECT_EQ(out.window_w, 1024);
        EXPECT_EQ(out.window_h, 768);
    }
    {
        const auto p = temp_file("hugewidth");
        std::ofstream{p} << R"({"window":{"w":99999999999,"h":50}})";
        const AppSettings out = SettingsStore{p}.load();
        EXPECT_EQ(out.window_w, 1024); // overflowed int -> default
        EXPECT_EQ(out.window_h, 768);  // 50 < 100 -> default
    }
    {
        const auto p = temp_file("validsize");
        std::ofstream{p} << R"({"window":{"w":800,"h":600}})";
        const AppSettings out = SettingsStore{p}.load();
        EXPECT_EQ(out.window_w, 800); // in range -> kept as-is
        EXPECT_EQ(out.window_h, 600);
    }
}

TEST(SettingsStore, SaveCreatesMissingParentDirectories) {
    namespace fs = std::filesystem;
    const auto root = oid::test::scratch_dir() / "oid_p4_mkdirs";
    fs::remove_all(root);
    const auto p = root / "nested" / "deep" / "imgui_settings.json";
    AppSettings s;
    s.window_w = 777; // in-range so it round-trips
    SettingsStore{p}.save(s);
    ASSERT_TRUE(fs::exists(p));
    EXPECT_EQ(SettingsStore{p}.load().window_w, 777);
    fs::remove_all(root);
}

TEST(SettingsStore, JsonRoundTrip) {
    AppSettings s;
    s.window_w = 800;
    s.window_h = 600;
    s.window_x = 40;
    s.window_y = 30;
    s.left_pane_w = 300.0f;
    s.contrast_enabled = false;
    s.link_views = true;
    s.last_export_dir = "/home/user/exports";
    s.previous_buffers = {{"a", 111}, {"b", 222}};

    const std::string json = settings_to_json(s);
    const AppSettings out = settings_from_json(json);
    EXPECT_EQ(out, s);
}

TEST(SettingsStore, JsonFromEmptyStringYieldsDefaults) {
    EXPECT_EQ(settings_from_json(""), AppSettings{});
}

TEST(SettingsStore, JsonFromGarbageYieldsDefaults) {
    EXPECT_EQ(settings_from_json("}{ not json"), AppSettings{});
}

TEST(SettingsStore, JsonFromPartialKeepsPresentFieldsRestDefault) {
    const AppSettings out = settings_from_json(
        R"({"ui":{"leftPaneWidth":150.0,"linkViews":true}})");
    EXPECT_FLOAT_EQ(out.left_pane_w, 150.0f);
    EXPECT_TRUE(out.link_views);
    EXPECT_TRUE(out.contrast_enabled);      // default kept
    EXPECT_EQ(out.window_w, 1024);          // default kept
    EXPECT_FALSE(out.window_x.has_value()); // omitted -> nullopt
}

TEST(SettingsStore, StoreLoadSaveDelegateToJsonFunctions) {
    // SettingsStore::load/save are thin file wrappers around
    // settings_from_json/settings_to_json; a file written by save() must be
    // exactly what settings_to_json() would produce, and load() must equal
    // settings_from_json() on that content.
    const auto p = temp_file("delegates");
    AppSettings s;
    s.window_w = 900;
    SettingsStore{p}.save(s);

    std::ifstream is{p, std::ios::binary};
    const std::string content{std::istreambuf_iterator<char>(is),
                              std::istreambuf_iterator<char>()};
    EXPECT_EQ(content, settings_to_json(s));
    EXPECT_EQ(SettingsStore{p}.load(), settings_from_json(content));
}

TEST(SettingsStore, SaveLeavesNoLeftoverTempFiles) {
    namespace fs = std::filesystem;
    const auto dir = oid::test::scratch_dir() / "oid_p4_tmpcheck";
    fs::remove_all(dir);
    const auto p = dir / "imgui_settings.json";
    AppSettings s;
    SettingsStore store{p};
    store.save(s);
    store.save(s); // repeated writes must not accumulate temp files
    ASSERT_TRUE(fs::exists(p));
    int json_count = 0;
    int tmp_count = 0;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (const auto ext = entry.path().extension().string();
            ext == ".json") {
            ++json_count;
        } else if (ext == ".tmp") {
            ++tmp_count;
        }
    }
    EXPECT_EQ(tmp_count, 0);  // temp file renamed/cleaned, never left behind
    EXPECT_EQ(json_count, 1); // exactly the settings file
    fs::remove_all(dir);
}
