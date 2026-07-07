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

#include <atomic>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>
#include <utility>

#if defined(_WIN32)
#include <process.h> // _getpid
#else
#include <unistd.h> // getpid
#endif

#include <nlohmann/json.hpp>

namespace oid::host {

namespace {

// Returns j[key] converted to T, or `def` when the key is missing, j isn't
// an object, or the value can't be converted to T (wrong JSON type). Used to
// make load() tolerant on a field-by-field basis: one malformed field falls
// back to its default without discarding any other valid field.
template <typename T>
T get_or(const nlohmann::json& j, const std::string& key, T def) {
    if (!j.is_object() || !j.contains(key)) {
        return def;
    }
    try {
        return j.at(key).get<T>();
    } catch (const nlohmann::json::exception&) {
        return def;
    }
}

} // namespace

std::string settings_to_json(const AppSettings& s) {
    nlohmann::json j;
    j["version"] = 1;
    j["window"] = {{"w", s.window_w}, {"h", s.window_h}};
    if (s.window_x.has_value()) {
        j["window"]["x"] = *s.window_x;
    }
    if (s.window_y.has_value()) {
        j["window"]["y"] = *s.window_y;
    }
    j["ui"] = {{"leftPaneWidth", s.left_pane_w},
               {"contrastEnabled", s.contrast_enabled},
               {"linkViews", s.link_views},
               {"lastExportDir", s.last_export_dir}};

    auto arr = nlohmann::json::array();
    for (const auto& b : s.previous_buffers) {
        arr.push_back(
            {{"name", b.variable_name}, {"expiry", b.expiry_epoch_s}});
    }
    j["previousBuffers"] = std::move(arr);

    return j.dump(2);
}

AppSettings settings_from_json(std::string_view json) {
    const AppSettings defaults{};

    try {
        const auto j = nlohmann::json::parse(json,
                                             /*cb=*/nullptr,
                                             /*allow_exceptions=*/false);
        if (j.is_discarded() || !j.is_object()) {
            return defaults;
        }

        AppSettings out{};

        if (j.contains("window") && j.at("window").is_object()) {
            const auto& w = j.at("window");
            out.window_w = get_or(w, "w", defaults.window_w);
            out.window_h = get_or(w, "h", defaults.window_h);
            // glfwCreateWindow() returns NULL (fatal startup failure) for
            // non-positive sizes, and huge persisted values can wrap to
            // negative/garbage ints when narrowed to int. Clamp to a sane
            // range so a bad persisted size (minimized-to-0x0, hand-edited,
            // or hostile) can never brick startup.
            if (out.window_w < 100 || out.window_w > 16384) {
                out.window_w = defaults.window_w;
            }
            if (out.window_h < 100 || out.window_h > 16384) {
                out.window_h = defaults.window_h;
            }
            if (w.contains("x") && w.at("x").is_number_integer()) {
                out.window_x = w.at("x").get<int>();
            }
            if (w.contains("y") && w.at("y").is_number_integer()) {
                out.window_y = w.at("y").get<int>();
            }
        }

        if (j.contains("ui") && j.at("ui").is_object()) {
            const auto& ui = j.at("ui");
            out.left_pane_w = get_or(ui, "leftPaneWidth", defaults.left_pane_w);
            out.contrast_enabled =
                get_or(ui, "contrastEnabled", defaults.contrast_enabled);
            out.link_views = get_or(ui, "linkViews", defaults.link_views);
            out.last_export_dir =
                get_or(ui, "lastExportDir", defaults.last_export_dir);
        }

        if (j.contains("previousBuffers") &&
            j.at("previousBuffers").is_array()) {
            for (const auto& entry : j.at("previousBuffers")) {
                if (!entry.is_object() || !entry.contains("name") ||
                    !entry.at("name").is_string() ||
                    !entry.contains("expiry") ||
                    !entry.at("expiry").is_number_integer()) {
                    continue; // skip malformed entry, keep the rest
                }
                out.previous_buffers.emplace_back(
                    entry.at("name").get<std::string>(),
                    entry.at("expiry").get<std::int64_t>());
            }
        }

        return out;
    } catch (const std::exception& e) {
        std::cerr << "[OID] settings parse failed: " << e.what() << '\n';
        return defaults;
    } catch (...) {
        return defaults;
    }
}

SettingsStore::SettingsStore(std::filesystem::path file)
    : file_(std::move(file)) {}

AppSettings SettingsStore::load() const {
    try {
        std::ifstream is{file_, std::ios::binary};
        if (!is) {
            return AppSettings{};
        }
        const std::string content{std::istreambuf_iterator<char>(is),
                                  std::istreambuf_iterator<char>()};
        return settings_from_json(content);
    } catch (const std::exception& e) {
        std::cerr << "[OID] settings load failed: " << e.what() << '\n';
        return AppSettings{};
    } catch (...) {
        return AppSettings{};
    }
}

void SettingsStore::save(const AppSettings& s) const {
    try {
        const std::string json = settings_to_json(s);

        if (const auto parent = file_.parent_path(); !parent.empty()) {
            std::error_code ec;
            std::filesystem::create_directories(parent, ec);
            // Ignore ec here: if the directory truly can't be created the
            // temp-file open below fails and is reported. A benign "already
            // exists" is not an error worth surfacing.
        }

        // Unique temp name per process AND per call. Two OID windows can be
        // alive at once (e.g. one lingering from a previous debug session plus
        // a fresh one), and both persist to the same settings file. A single
        // shared "<file>.tmp" made their saves race: whichever renamed first
        // consumed the temp, and the other's rename then failed with ENOENT.
        // A per-writer temp name lets each save complete independently
        // (last writer wins, which is fine for UI preferences).
        static std::atomic<unsigned> tmp_counter{0};
#if defined(_WIN32)
        const auto pid = static_cast<long>(_getpid());
#else
        const auto pid = static_cast<long>(getpid());
#endif
        const std::filesystem::path tmp =
            file_.string() + "." + std::to_string(pid) + "." +
            std::to_string(tmp_counter.fetch_add(1)) + ".tmp";
        {
            std::ofstream os{tmp, std::ios::binary | std::ios::trunc};
            os << json;
            if (!os) {
                std::cerr << "[OID] settings save failed: write error (disk "
                             "full?)\n";
                std::error_code ec;
                std::filesystem::remove(tmp, ec); // best-effort cleanup
                return;
            }
        }
        std::error_code ren_ec;
        std::filesystem::rename(tmp, file_, ren_ec); // atomic replace
        if (ren_ec) {
            std::cerr << "[OID] settings save failed: " << ren_ec.message()
                      << '\n';
            std::error_code rm_ec;
            std::filesystem::remove(tmp, rm_ec); // best-effort cleanup
        }
    } catch (const std::exception& e) {
        std::cerr << "[OID] settings save failed: " << e.what() << '\n';
    } catch (...) {
        std::cerr << "[OID] settings save failed: unknown error\n";
    }
}

} // namespace oid::host
