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
#include <algorithm>
#include <exception>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
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

    const std::string json = settings_to_json(s, SettingsScope::FULL);
    const AppSettings out = settings_from_json(json, SettingsScope::FULL);
    EXPECT_EQ(out, s);
}

TEST(SettingsStore, JsonFromEmptyStringYieldsDefaults) {
    EXPECT_EQ(settings_from_json("", SettingsScope::FULL), AppSettings{});
}

TEST(SettingsStore, JsonFromGarbageYieldsDefaults) {
    EXPECT_EQ(settings_from_json("}{ not json", SettingsScope::FULL),
              AppSettings{});
}

TEST(SettingsStore, JsonFromPartialKeepsPresentFieldsRestDefault) {
    const AppSettings out =
        settings_from_json(R"({"ui":{"leftPaneWidth":150.0,"linkViews":true}})",
                           SettingsScope::FULL);
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
    EXPECT_EQ(content, settings_to_json(s, SettingsScope::FULL));
    EXPECT_EQ(SettingsStore{p}.load(),
              settings_from_json(content, SettingsScope::FULL));
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

TEST(SettingsForScope, FullIsIdentity) {
    AppSettings s;
    s.window_w = 800;
    s.window_h = 600;
    s.window_x = 40;
    s.window_y = 30;
    s.previous_buffers = {{"a", 111}};
    EXPECT_EQ(settings_for_scope(s, SettingsScope::FULL), s);
}

TEST(SettingsForScope, ViewerOwnedDropsWhatTheHostOwns) {
    AppSettings s;
    s.window_w = 800;
    s.window_h = 600;
    s.window_x = 40;
    s.window_y = 30;
    s.previous_buffers = {{"a", 111}};
    s.contrast_enabled = false;
    s.left_pane_w = 300.0f;
    s.last_export_dir = "/home/user/exports";

    const AppSettings scoped =
        settings_for_scope(s, SettingsScope::VIEWER_OWNED);
    const AppSettings defaults{};

    EXPECT_EQ(scoped.window_w, defaults.window_w);
    EXPECT_EQ(scoped.window_h, defaults.window_h);
    EXPECT_EQ(scoped.window_x, defaults.window_x);
    EXPECT_EQ(scoped.window_y, defaults.window_y);
    EXPECT_TRUE(scoped.previous_buffers.empty());

    // Everything the viewer owns survives untouched.
    EXPECT_FALSE(scoped.contrast_enabled);
    EXPECT_FLOAT_EQ(scoped.left_pane_w, 300.0f);
    EXPECT_EQ(scoped.last_export_dir, "/home/user/exports");
}

// The traffic fix, stated as an assertion: two snapshots that differ only in
// geometry must be indistinguishable to the saver's comparison once scoped,
// because that comparison is the only thing deciding whether a frame goes out.
TEST(SettingsForScope, GeometryChurnComparesEqualUnderViewerOwned) {
    AppSettings a;
    a.window_w = 800;
    a.window_h = 600;
    AppSettings b = a;
    b.window_w = 801;
    b.window_h = 599;

    EXPECT_NE(a, b);
    EXPECT_EQ(settings_for_scope(a, SettingsScope::VIEWER_OWNED),
              settings_for_scope(b, SettingsScope::VIEWER_OWNED));
}

TEST(SettingsForScope, LeavesItsArgumentAlone) {
    AppSettings s;
    s.window_w = 800;
    s.previous_buffers = {{"a", 111}};
    const AppSettings before = s;

    (void)settings_for_scope(s, SettingsScope::VIEWER_OWNED);

    EXPECT_EQ(s, before);
}

TEST(SettingsToJson, FullOutputIsByteIdenticalToWhatWeAlwaysWrote) {
    AppSettings s;
    s.window_w = 800;
    s.window_h = 600;
    s.window_x = 40;
    s.window_y = 30;
    s.left_pane_w = 300.0f;
    s.contrast_enabled = false;
    s.link_views = true;
    s.previous_buffers = {{"a", 111}, {"b", 222}};
    s.last_export_dir = "/home/user/exports";

    // Byte-for-byte, from the build that shipped before the scope existed.
    // The on-disk settings file is this format; a diff here is a migration
    // nobody planned. Weakening this test defeats its only purpose.
    EXPECT_EQ(settings_to_json(s, SettingsScope::FULL),
              R"({
  "previousBuffers": [
    {
      "expiry": 111,
      "name": "a"
    },
    {
      "expiry": 222,
      "name": "b"
    }
  ],
  "ui": {
    "contrastEnabled": false,
    "lastExportDir": "/home/user/exports",
    "leftPaneWidth": 300.0,
    "linkViews": true
  },
  "version": 1,
  "window": {
    "h": 600,
    "w": 800,
    "x": 40,
    "y": 30
  }
})");
}

TEST(SettingsToJson, ViewerOwnedCarriesOnlyVersionAndUi) {
    AppSettings s;
    s.window_w = 800;
    s.window_h = 600;
    s.window_x = 40;
    s.window_y = 30;
    s.previous_buffers = {{"a", 111}};
    s.contrast_enabled = false;

    const auto j =
        nlohmann::json::parse(settings_to_json(s, SettingsScope::VIEWER_OWNED));

    EXPECT_TRUE(j.contains("version"));
    EXPECT_TRUE(j.contains("ui"));
    EXPECT_FALSE(j.contains("window"));
    EXPECT_FALSE(j.contains("previousBuffers"));
    EXPECT_EQ(j.at("ui").at("contrastEnabled").get<bool>(), false);
}

TEST(SettingsToJson, DoesNotThrowOnInvalidUtf8LastExportDir) {
    // last_export_dir is filesystem-path-derived, and a filesystem path need
    // not be valid UTF-8 on Linux. A lone 0xFF is not a valid UTF-8 lead
    // byte on its own; confirmed against this vendored nlohmann/json (via a
    // standalone probe against the library directly, not this codebase) that
    // it makes json::dump() throw type_error.316 under the library's default
    // (strict) error handler -- the throw settings_to_json's "Never throws"
    // contract requires nothing ever hits. error_handler_t::replace is what
    // keeps that contract true for this input.
    AppSettings s;
    s.last_export_dir = std::string(1, static_cast<char>(0xFF));

    std::string json;
    EXPECT_NO_THROW(json = settings_to_json(s, SettingsScope::FULL));

    // The output is still well-formed JSON, with the invalid byte replaced
    // rather than silently dropped or left to corrupt the document.
    const auto parsed = nlohmann::json::parse(json, nullptr, false);
    ASSERT_FALSE(parsed.is_discarded());
    EXPECT_NE(
        parsed.at("ui")
            .at("lastExportDir")
            .get<std::string>()
            .find("\xEF\xBF\xBD"), // UTF-8 for U+FFFD, the replacement char
        std::string::npos);
}

TEST(SettingsFromJson, ViewerOwnedIgnoresHostOwnedSectionsAndReportsThem) {
    const auto json = R"({
      "version": 1,
      "window": {"w": 800, "h": 600, "x": 40, "y": 30},
      "ui": {"leftPaneWidth": 300.0, "contrastEnabled": false,
             "linkViews": true, "lastExportDir": "/home/user/exports"},
      "previousBuffers": [{"name": "a", "expiry": 111}]
    })";

    std::vector<std::string> ignored;
    const AppSettings out = settings_from_json(
        json,
        SettingsScope::VIEWER_OWNED,
        [&ignored](const std::string_view key) { ignored.emplace_back(key); });

    const AppSettings defaults{};
    EXPECT_EQ(out.window_w, defaults.window_w);
    EXPECT_EQ(out.window_h, defaults.window_h);
    EXPECT_EQ(out.window_x, defaults.window_x);
    EXPECT_EQ(out.window_y, defaults.window_y);
    EXPECT_TRUE(out.previous_buffers.empty());

    // The viewer's own preferences still arrive.
    EXPECT_FLOAT_EQ(out.left_pane_w, 300.0f);
    EXPECT_FALSE(out.contrast_enabled);
    EXPECT_TRUE(out.link_views);
    EXPECT_EQ(out.last_export_dir, "/home/user/exports");

    // Reporting order isn't contractual, only the set of reported keys is.
    std::ranges::sort(ignored);
    EXPECT_EQ(ignored, (std::vector<std::string>{"previousBuffers", "window"}));
}

TEST(SettingsFromJson, ViewerOwnedReportsNothingWhenTheHostSendsNothingItOwns) {
    const auto json = R"({"version": 1, "ui": {"contrastEnabled": true}})";

    std::vector<std::string> ignored;
    const AppSettings out = settings_from_json(
        json,
        SettingsScope::VIEWER_OWNED,
        [&ignored](const std::string_view key) { ignored.emplace_back(key); });

    EXPECT_TRUE(out.contrast_enabled);
    EXPECT_TRUE(ignored.empty());
}

TEST(SettingsFromJson, AMissingSinkIsNotAnError) {
    const auto json =
        R"({"window": {"w": 800}, "ui": {"contrastEnabled": true}})";

    const AppSettings out =
        settings_from_json(json, SettingsScope::VIEWER_OWNED);

    EXPECT_EQ(out.window_w, AppSettings{}.window_w);
    EXPECT_TRUE(out.contrast_enabled);
}

TEST(SettingsFromJson, FullStillReadsEverything) {
    const auto json = R"({
      "version": 1,
      "window": {"w": 800, "h": 600},
      "ui": {"contrastEnabled": false},
      "previousBuffers": [{"name": "a", "expiry": 111}]
    })";

    const AppSettings out = settings_from_json(json, SettingsScope::FULL);

    EXPECT_EQ(out.window_w, 800);
    EXPECT_EQ(out.window_h, 600);
    EXPECT_FALSE(out.contrast_enabled);
    ASSERT_EQ(out.previous_buffers.size(), 1u);
    EXPECT_EQ(out.previous_buffers.front().variable_name, "a");
}

TEST(SettingsFromJson, ViewerOwnedReportsMalformedHostOwnedKeysToo) {
    // A host-owned key that is present but not the shape FULL would have
    // parsed (a number instead of an object/array) is still declined, and
    // that decline is still reported: under VIEWER_OWNED the shape never
    // gets looked at, so it can't be the thing that decides whether to
    // report.
    const auto json =
        R"({"window": 5, "previousBuffers": 7, "ui": {"contrastEnabled": true}})";

    std::vector<std::string> ignored;
    const AppSettings out = settings_from_json(
        json,
        SettingsScope::VIEWER_OWNED,
        [&ignored](const std::string_view key) { ignored.emplace_back(key); });

    EXPECT_TRUE(out.contrast_enabled);

    // Reporting order isn't contractual, only the set of reported keys is.
    std::ranges::sort(ignored);
    EXPECT_EQ(ignored, (std::vector<std::string>{"previousBuffers", "window"}));
}

namespace {
// Test-local: distinguishes "the sink under test threw" from any exception
// a real dependency (std::* or otherwise) might raise, so a passing
// assertion can't be explained by the wrong throw.
struct SinkFailure : std::exception {
    [[nodiscard]] const char* what() const noexcept override {
        return "on_ignored sink failure";
    }
};
} // namespace

TEST(SettingsFromJson, AThrowingSinkDoesNotCostTheCallerItsData) {
    // AppSettings{}.contrast_enabled defaults to true, so a fall-back to
    // defaults and a successful parse of "false" are distinguishable.
    const auto json =
        R"({"window": {"w": 800}, "ui": {"contrastEnabled": false}})";

    const AppSettings out =
        settings_from_json(json,
                           SettingsScope::VIEWER_OWNED,
                           [](std::string_view) { throw SinkFailure{}; });

    EXPECT_FALSE(out.contrast_enabled);
}

TEST(SettingsFromJson, AThrowingSinkStillReportsEveryHostOwnedKey) {
    // The test above pins containment for a single sink call; this one
    // pins it per-call: a sink that throws on EVERY call must not stop
    // after the first host-owned key, so a second one is still reported,
    // and the parse result is still the cleanly-parsed "ui" value rather
    // than a fall-back to defaults.
    const auto json = R"({
      "window": {"w": 800},
      "previousBuffers": [{"name": "a", "expiry": 111}],
      "ui": {"contrastEnabled": false}
    })";

    std::vector<std::string> ignored;
    const AppSettings out = settings_from_json(
        json, SettingsScope::VIEWER_OWNED, [&ignored](std::string_view key) {
            ignored.emplace_back(key);
            throw SinkFailure{};
        });

    EXPECT_FALSE(out.contrast_enabled);
    std::ranges::sort(ignored);
    EXPECT_EQ(ignored, (std::vector<std::string>{"previousBuffers", "window"}));
}
