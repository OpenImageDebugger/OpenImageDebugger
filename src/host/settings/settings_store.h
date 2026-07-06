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
#include <string>
#include <string_view>

#include "host/settings/app_settings.h"

namespace oid::host {

// Serializes an AppSettings snapshot to the same JSON shape SettingsStore
// persists to disk. Never throws.
[[nodiscard]] std::string settings_to_json(const AppSettings& s);

// Parses a JSON snapshot back into AppSettings, tolerant field-by-field: a
// missing/wrong-typed field (or a malformed/empty document) falls back to
// that field's default without discarding any other valid field. Never
// throws.
[[nodiscard]] AppSettings settings_from_json(std::string_view json);

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
