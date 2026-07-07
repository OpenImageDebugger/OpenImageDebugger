/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2026 OpenImageDebugger
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

#include "process.h"
#include "process_impl.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace oid {

namespace {

std::wstring utf8_to_wide(const std::string_view text) {
    if (text.empty()) {
        return {};
    }

    const auto size = MultiByteToWideChar(CP_UTF8,
                                          MB_ERR_INVALID_CHARS,
                                          text.data(),
                                          static_cast<int>(text.size()),
                                          nullptr,
                                          0);
    if (size <= 0) {
        return {};
    }

    std::wstring wide(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8,
                        MB_ERR_INVALID_CHARS,
                        text.data(),
                        static_cast<int>(text.size()),
                        wide.data(),
                        size);
    return wide;
}

std::wstring quote_windows_arg(const std::wstring& arg) {
    if (arg.empty()) {
        return L"\"\"";
    }

    if (arg.find_first_of(L" \t\"") == std::wstring::npos) {
        return arg;
    }

    std::wstring quoted;
    quoted.reserve(arg.size() + 2);
    quoted.push_back(L'"');
    for (const wchar_t ch : arg) {
        if (ch == L'"') {
            quoted.append(LR"(\")");
        } else {
            quoted.push_back(ch);
        }
    }
    quoted.push_back(L'"');
    return quoted;
}

std::wstring build_command_line(const std::vector<std::string>& command) {
    std::wstring line;
    for (size_t i = 0; i < command.size(); ++i) {
        if (i > 0) {
            line.push_back(L' ');
        }
        line += quote_windows_arg(utf8_to_wide(command[i]));
    }
    return line;
}

} // namespace

class ProcessImplWin32 final : public ProcessImpl {
  public:
    ProcessImplWin32() = default;

    ProcessImplWin32(const ProcessImplWin32&) = delete;
    ProcessImplWin32(ProcessImplWin32&&) = delete;
    ProcessImplWin32& operator=(const ProcessImplWin32&) = delete;
    ProcessImplWin32& operator=(ProcessImplWin32&&) = delete;

    ~ProcessImplWin32() noexcept override {
        kill();
        close_process_handle();
    }

    void start(std::vector<std::string>& command) override {
        kill();
        close_process_handle();

        if (command.empty()) {
            return;
        }

        auto command_line = build_command_line(command);
        command_line.push_back(L'\0');

        STARTUPINFOW startup_info{};
        startup_info.cb = sizeof(startup_info);
        PROCESS_INFORMATION process_info{};

        if (!CreateProcessW(nullptr,
                            command_line.data(),
                            nullptr,
                            nullptr,
                            FALSE,
                            0,
                            nullptr,
                            nullptr,
                            &startup_info,
                            &process_info)) {
            return;
        }

        CloseHandle(process_info.hThread);
        process_handle_ = process_info.hProcess;
    }

    [[nodiscard]] bool isRunning() const override {
        if (process_handle_ == nullptr) {
            return false;
        }

        DWORD exit_code = 0;
        if (!GetExitCodeProcess(process_handle_, &exit_code)) {
            return false;
        }

        return exit_code == STILL_ACTIVE;
    }

    void kill() override {
        if (process_handle_ != nullptr && isRunning()) {
            TerminateProcess(process_handle_, 1);
        }
    }

  private:
    void close_process_handle() {
        if (process_handle_ != nullptr) {
            CloseHandle(process_handle_);
            process_handle_ = nullptr;
        }
    }

    HANDLE process_handle_{nullptr};
};

void Process::createImpl() {
    impl_ = std::make_shared<ProcessImplWin32>();
}

} // namespace oid
