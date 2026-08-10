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

#ifndef HOST_SETTINGS_SETTINGS_STORE_H_
#define HOST_SETTINGS_SETTINGS_STORE_H_

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>

#include "host/settings/app_settings.h"

namespace oid::host {

// Serializes an AppSettings snapshot to the JSON shape SettingsStore
// persists to disk. Never throws.
//
// Under SettingsScope::VIEWER_OWNED the host-owned keys are omitted entirely
// rather than emitted empty: an embedding host that stores what it is given
// should not be storing a window it owns, nor a buffer list the viewer would
// read back as an instruction to re-plot.
[[nodiscard]] std::string settings_to_json(const AppSettings& s,
                                           SettingsScope scope);

// Parses a JSON snapshot back into AppSettings, tolerant field-by-field: a
// missing/wrong-typed field (or a malformed/empty document) falls back to
// that field's default without discarding any other valid field. Never
// throws.
//
// Under SettingsScope::VIEWER_OWNED the host-owned sections are not applied,
// however well-formed they are: on such a build the window and the buffer
// list belong to whoever embeds the viewer, and reading them back would let
// a stored payload drive a restore this build does not own.
//
// `on_ignored` is called once per host-owned key found in such a payload.
// It is a sink rather than a log line so that this stays a pure function
// with no opinion about logging, the same way SettingsSaver takes its
// SaveSink -- inject a callback instead of doing I/O here (SaveSink itself
// is mandatory and unguarded; this sink is optional and guarded, so the
// likeness is in the shape, not in the contract). An empty sink is not an
// error and parsing behaves identically without one. A sink that throws is
// contained and cannot affect the parse result: it cannot discard a field
// that had already parsed cleanly, nor be mistaken for a parse failure.
[[nodiscard]] AppSettings settings_from_json(
    std::string_view json,
    SettingsScope scope,
    const std::function<void(std::string_view key)>& on_ignored = {});

// Qt-free JSON settings persistence. load()/save() are thin file-I/O
// wrappers around settings_from_json()/settings_to_json(): load() reads the
// file and parses it (never throws -- any error falls back to defaults);
// save() serializes and writes atomically via a temp file + rename (never
// throws, logging failures to std::cerr).
class SettingsStore {
  public:
    explicit SettingsStore(std::filesystem::path file);

    [[nodiscard]] AppSettings load() const;
    void save(const AppSettings& settings) const;

  private:
    std::filesystem::path file_;
};

} // namespace oid::host

#endif // HOST_SETTINGS_SETTINGS_STORE_H_
