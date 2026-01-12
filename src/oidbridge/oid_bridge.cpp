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

#include "oid_bridge.h"

#include <cstdint>
#if defined(__unix__) || defined(__APPLE__)
#include <dlfcn.h>
#include <libgen.h>
#include <limits.h>
#include <unistd.h>
#endif

#include <bit>
#include <chrono>
#include <deque>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <span>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "debuggerinterface/preprocessor_directives.h"
#include "debuggerinterface/python_native_interface.h"
#include "ipc/message_exchange.h"
#include "system/process/process.h"

#include <QDataStream>
#include <QPointer>
#include <QTcpServer>
#include <QTcpSocket>


struct PlotBufferParams
{
    std::string variable_name_str;
    std::string display_name_str;
    std::string pixel_layout_str;
    bool transpose_buffer;
    int buff_width;
    int buff_height;
    int buff_channels;
    int buff_stride;
    oid::BufferType buff_type;
    std::span<const std::byte> buffer;
};

struct UiMessage
{
    virtual ~UiMessage() = default;
};

struct GetObservedSymbolsResponseMessage final : UiMessage
{
    std::deque<std::string> observed_symbols{};
};

struct PlotBufferRequestMessage final : UiMessage
{
    std::string buffer_name{};
};

class PyGILRAII
{
  private:
    // H17: Class-level static variable for LLDB mode detection (cached,
    // thread-safe)
    static bool s_is_lldb_mode;

  public:
    // H17: Check if we're in LLDB mode (cached, thread-safe)
    static bool is_lldb_mode()
    {
        // Access the cached value - will be set on first PyGILRAII construction
        return s_is_lldb_mode;
    }

    PyGILRAII()
    {
        // #region agent log
        std::ofstream log_file(
            "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
            std::ios::app);
        if (log_file.is_open()) {
            log_file
                << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H1","location":"oid_bridge.cpp:88","message":"PyGILRAII_constructor_entry","data":{"thread_id":)"
                << std::this_thread::get_id() << R"(},"timestamp":)"
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()
                << "}\n";
            log_file.close();
        }
        // #endregion

        // H16: Cache LLDB mode detection to avoid calling unsafe Python C API
        // functions (PyGILState_GetThisThreadState, Py_IsInitialized) from
        // event loop thread. These calls crash in LLDB's embedded Python when
        // called from non-main threads, even when we skip GIL management.
        static bool lldb_mode_cached = false;

        PyThreadState* tstate_before = nullptr;
        bool py_init_before          = false;

        if (!lldb_mode_cached) {
            // First call: detect LLDB mode from main thread (safe)
            PyThreadState* tstate = PyGILState_GetThisThreadState();
            bool py_init          = Py_IsInitialized() != 0;

            // #region agent log
            std::ofstream log_file2(
                "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
                std::ios::app);
            if (log_file2.is_open()) {
                log_file2
                    << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H16","location":"oid_bridge.cpp:107","message":"PyGILRAII_checking_thread_state_first_call","data":{"py_initialized":)"
                    << (py_init ? "true" : "false") << R"(,"thread_state":)"
                    << (tstate ? "not_null" : "null") << R"(},"timestamp":)"
                    << std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count()
                    << "}\n";
                log_file2.close();
            }
            // #endregion

            tstate_before    = PyGILState_GetThisThreadState();
            py_init_before   = Py_IsInitialized() != 0;
            s_is_lldb_mode   = (!tstate_before && !py_init_before);
            lldb_mode_cached = true;

            // #region agent log
            std::ofstream log_file_cache(
                "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
                std::ios::app);
            if (log_file_cache.is_open()) {
                log_file_cache
                    << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H16","location":"oid_bridge.cpp:162","message":"PyGILRAII_lldb_mode_cached","data":{"is_lldb_mode":)"
                    << (s_is_lldb_mode ? "true" : "false")
                    << R"(},"timestamp":)"
                    << std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count()
                    << "}\n";
                log_file_cache.close();
            }
            // #endregion
        } else {
            // Use cached result - no more Python C API calls needed
            if (s_is_lldb_mode) {
                // Use cached LLDB mode - don't call Python C API functions
                tstate_before  = nullptr;
                py_init_before = false;
            } else {
                // Normal mode - safe to call these functions
                tstate_before  = PyGILState_GetThisThreadState();
                py_init_before = Py_IsInitialized() != 0;
            }
        }

        // #region agent log
        std::ofstream log_file_condition(
            "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
            std::ios::app);
        if (log_file_condition.is_open()) {
            log_file_condition
                << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H16","location":"oid_bridge.cpp:162","message":"PyGILRAII_condition_check","data":{"tstate_before":)"
                << (tstate_before ? "not_null" : "null")
                << R"(,"py_init_before":)"
                << (py_init_before ? "true" : "false") << R"(,"is_lldb_mode":)"
                << (s_is_lldb_mode ? "true" : "false")
                << R"(,"condition_result":)"
                << ((!tstate_before && !py_init_before) ? "true" : "false")
                << R"(},"timestamp":)"
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()
                << "}\n";
            log_file_condition.close();
        }
        // #endregion

        // If we have no thread state AND Python reports not initialized,
        // but we're being called from Python code, this is LLDB's
        // sub-interpreter. In this case:
        // 1. We're called from Python via ctypes, so GIL is already held
        // 2. PyGILState_Ensure() aborts because it can't register thread state
        // 3. But Python APIs work (PyDict_Check succeeds), so GIL IS held
        // Solution: Skip PyGILState_Ensure()/Release() - GIL is managed by
        // Python/LLDB, not by us. We can use Python APIs directly.
        if (s_is_lldb_mode || (!tstate_before && !py_init_before)) {
            // LLDB sub-interpreter: GIL is held by Python/LLDB, but thread
            // state isn't registered with GILState API. We can use Python APIs
            // but cannot call PyGILState_Ensure()/Release(). Skip GIL
            // management.
            _has_gil = true; // Don't try to acquire/release - it's already held
            // #region agent log
            std::ofstream log_file3(
                "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
                std::ios::app);
            if (log_file3.is_open()) {
                log_file3
                    << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H1","location":"oid_bridge.cpp:187","message":"PyGILRAII_skipping_gil_management_lldb","data":{},"timestamp":)"
                    << std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count()
                    << "}\n";
                log_file3.close();
            }
            // #endregion
        } else {
            // Normal case: try PyGILState_Ensure()
            // #region agent log
            std::ofstream log_file4(
                "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
                std::ios::app);
            if (log_file4.is_open()) {
                log_file4
                    << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H1","location":"oid_bridge.cpp:155","message":"PyGILState_Ensure_called","data":{},"timestamp":)"
                    << std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count()
                    << "}\n";
                log_file4.close();
            }
            // #endregion
            _py_gil_state = PyGILState_Ensure();
            _has_gil      = false; // We acquired it, so we need to release it
            // #region agent log
            std::ofstream log_file5(
                "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
                std::ios::app);
            if (log_file5.is_open()) {
                log_file5
                    << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H1","location":"oid_bridge.cpp:167","message":"PyGILState_Ensure_success","data":{},"timestamp":)"
                    << std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count()
                    << "}\n";
                log_file5.close();
            }
            // #endregion
        }
    }

    PyGILRAII(const PyGILRAII&)            = delete;
    PyGILRAII(PyGILRAII&&)                 = delete;
    PyGILRAII& operator=(const PyGILRAII&) = delete;
    PyGILRAII& operator=(PyGILRAII&&)      = delete;

    ~PyGILRAII() noexcept
    {
        if (!_has_gil) {
            PyGILState_Release(_py_gil_state);
        }
    }

  private:
    PyGILState_STATE _py_gil_state{};
    bool _has_gil{false};
};

// H17: Static member definition for LLDB mode detection
bool PyGILRAII::s_is_lldb_mode = false;

class OidBridge
{
  public:
    explicit OidBridge(std::function<int(const char*)> plot_callback)
        : plot_callback_{std::move(plot_callback)}
    {
        // #region agent log
        std::ofstream log_file(
            "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
            std::ios::app);
        if (log_file.is_open()) {
            log_file
                << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H4","location":"oid_bridge.cpp:133","message":"OidBridge_constructor_complete","data":{"thread_id":)"
                << std::this_thread::get_id() << R"(},"timestamp":)"
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()
                << "}\n";
            log_file.close();
        }
        // #endregion
    }

    bool start()
    {
        // #region agent log
        std::ofstream log_file(
            "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
            std::ios::app);
        if (log_file.is_open()) {
            log_file
                << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H4","location":"oid_bridge.cpp:142","message":"OidBridge_start_entry","data":{"thread_id":)"
                << std::this_thread::get_id() << R"(},"timestamp":)"
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()
                << "}\n";
            log_file.close();
        }
        // #endregion

        // Initialize server
        // #region agent log
        std::ofstream log_file2(
            "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
            std::ios::app);
        if (log_file2.is_open()) {
            log_file2
                << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H4","location":"oid_bridge.cpp:150","message":"OidBridge_before_listen","data":{},"timestamp":)"
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()
                << "}\n";
            log_file2.close();
        }
        // #endregion
        if (!server_.listen(QHostAddress::Any)) {
            std::cerr << "[OpenImageDebugger] Could not start TCP server"
                      << std::endl;
            return false;
        }

        // #region agent log
        std::ofstream log_file3(
            "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
            std::ios::app);
        if (log_file3.is_open()) {
            log_file3
                << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H4","location":"oid_bridge.cpp:160","message":"OidBridge_after_listen","data":{"port":)"
                << server_.serverPort() << R"(},"timestamp":)"
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()
                << "}\n";
            log_file3.close();
        }
        // #endregion

        // H14: Fallback to detect library path when oid_path_ is empty (LLDB
        // Python bug - __file__ not set)
        std::string oid_path_to_use = this->oid_path_;
        if (oid_path_to_use.empty()) {
            // #region agent log
            std::ofstream log_file_fallback_path(
                "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
                std::ios::app);
            if (log_file_fallback_path.is_open()) {
                log_file_fallback_path
                    << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H14","location":"oid_bridge.cpp:341","message":"oid_path_empty_using_fallback","data":{},"timestamp":)"
                    << std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count()
                    << "}\n";
                log_file_fallback_path.close();
            }
            // #endregion

#if defined(__unix__) || defined(__APPLE__)
            // Use dladdr to get the path of the current library
            Dl_info info;
            if (dladdr(reinterpret_cast<void*>(&oid_initialize), &info) != 0 &&
                info.dli_fname != nullptr) {
                char resolved_path[PATH_MAX];
                if (realpath(info.dli_fname, resolved_path) != nullptr) {
                    // Get directory of the library
                    char* lib_dir   = dirname(resolved_path);
                    oid_path_to_use = lib_dir;
                    // #region agent log
                    std::ofstream log_file_dladdr_success(
                        "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
                        std::ios::app);
                    if (log_file_dladdr_success.is_open()) {
                        log_file_dladdr_success
                            << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H14","location":"oid_bridge.cpp:360","message":"dladdr_fallback_success","data":{"oid_path":")"
                            << oid_path_to_use << R"("},"timestamp":)"
                            << std::chrono::duration_cast<
                                   std::chrono::milliseconds>(
                                   std::chrono::system_clock::now()
                                       .time_since_epoch())
                                   .count()
                            << "}\n";
                        log_file_dladdr_success.close();
                    }
                    // #endregion
                } else {
                    // #region agent log
                    std::ofstream log_file_realpath_failed(
                        "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
                        std::ios::app);
                    if (log_file_realpath_failed.is_open()) {
                        log_file_realpath_failed
                            << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H14","location":"oid_bridge.cpp:375","message":"realpath_failed","data":{},"timestamp":)"
                            << std::chrono::duration_cast<
                                   std::chrono::milliseconds>(
                                   std::chrono::system_clock::now()
                                       .time_since_epoch())
                                   .count()
                            << "}\n";
                        log_file_realpath_failed.close();
                    }
                    // #endregion
                }
            } else {
                // #region agent log
                std::ofstream log_file_dladdr_failed(
                    "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
                    std::ios::app);
                if (log_file_dladdr_failed.is_open()) {
                    log_file_dladdr_failed
                        << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H14","location":"oid_bridge.cpp:390","message":"dladdr_failed","data":{},"timestamp":)"
                        << std::chrono::duration_cast<
                               std::chrono::milliseconds>(
                               std::chrono::system_clock::now()
                                   .time_since_epoch())
                               .count()
                        << "}\n";
                    log_file_dladdr_failed.close();
                }
                // #endregion
            }
#endif
        }

        const auto windowBinaryPath = oid_path_to_use + "/oidwindow";
        const auto portStdString    = std::to_string(server_.serverPort());

        std::vector<std::string> command{
            windowBinaryPath, "-style", "fusion", "-p", portStdString};

        // #region agent log
        std::ofstream log_file4(
            "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
            std::ios::app);
        if (log_file4.is_open()) {
            log_file4
                << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H4","location":"oid_bridge.cpp:172","message":"OidBridge_before_process_start","data":{"path":")"
                << windowBinaryPath << R"("},"timestamp":)"
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()
                << "}\n";
            log_file4.close();
        }
        // #endregion
        ui_proc_.start(command);

        // #region agent log
        std::ofstream log_file5(
            "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
            std::ios::app);
        if (log_file5.is_open()) {
            log_file5
                << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H4","location":"oid_bridge.cpp:180","message":"OidBridge_after_process_start","data":{},"timestamp":)"
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()
                << "}\n";
            log_file5.close();
        }
        // #endregion

        // #region agent log
        std::ofstream log_file_before_waitForStart(
            "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
            std::ios::app);
        if (log_file_before_waitForStart.is_open()) {
            log_file_before_waitForStart
                << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H11","location":"oid_bridge.cpp:379","message":"before_waitForStart","data":{},"timestamp":)"
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()
                << "}\n";
            log_file_before_waitForStart.close();
        }
        // #endregion

        ui_proc_.waitForStart();

        // #region agent log
        std::ofstream log_file6(
            "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
            std::ios::app);
        if (log_file6.is_open()) {
            log_file6
                << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H4","location":"oid_bridge.cpp:188","message":"OidBridge_after_waitForStart","data":{},"timestamp":)"
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()
                << "}\n";
            log_file6.close();
        }
        // #endregion

        // #region agent log
        std::ofstream log_file_before_wait_for_client(
            "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
            std::ios::app);
        if (log_file_before_wait_for_client.is_open()) {
            log_file_before_wait_for_client
                << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H11","location":"oid_bridge.cpp:400","message":"before_wait_for_client","data":{},"timestamp":)"
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()
                << "}\n";
            log_file_before_wait_for_client.close();
        }
        // #endregion

        wait_for_client();

        // #region agent log
        std::ofstream log_file7(
            "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
            std::ios::app);
        if (log_file7.is_open()) {
            log_file7
                << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H4","location":"oid_bridge.cpp:196","message":"OidBridge_start_exit","data":{"client_not_null":)"
                << (client_ != nullptr ? "true" : "false")
                << R"(},"timestamp":)"
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()
                << "}\n";
            log_file7.close();
        }
        // #endregion

        return client_ != nullptr;
    }

    void set_path(const std::string_view& oid_path)
    {
        // #region agent log
        std::ofstream log_file_set_path(
            "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
            std::ios::app);
        if (log_file_set_path.is_open()) {
            log_file_set_path
                << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H13","location":"oid_bridge.cpp:448","message":"set_path_called","data":{"oid_path":")"
                << oid_path << R"(","oid_path_length":)" << oid_path.length()
                << R"(},"timestamp":)"
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()
                << "}\n";
            log_file_set_path.close();
        }
        // #endregion
        oid_path_ = oid_path;
        // #region agent log
        std::ofstream log_file_set_path_after(
            "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
            std::ios::app);
        if (log_file_set_path_after.is_open()) {
            log_file_set_path_after
                << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H13","location":"oid_bridge.cpp:451","message":"set_path_complete","data":{"oid_path_":")"
                << oid_path_ << R"(","oid_path_length":)" << oid_path_.length()
                << R"(},"timestamp":)"
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()
                << "}\n";
            log_file_set_path_after.close();
        }
        // #endregion
    }

    [[nodiscard]] bool is_window_ready() const
    {
        return client_ != nullptr && ui_proc_.isRunning();
    }

    std::deque<std::string> get_observed_symbols()
    {
        assert(!client_.isNull());

        auto message_composer = oid::MessageComposer{};
        message_composer.push(oid::MessageType::GetObservedSymbols)
            .send(client_);

        if (const auto response =
                fetch_message(oid::MessageType::GetObservedSymbolsResponse);
            response != nullptr) {
            return dynamic_cast<GetObservedSymbolsResponseMessage*>(
                       response.get())
                ->observed_symbols;
        }

        return {};
    }


    void
    set_available_symbols(const std::deque<std::string>& available_vars) const
    {
        assert(!client_.isNull());

        auto message_composer = oid::MessageComposer{};
        message_composer.push(oid::MessageType::SetAvailableSymbols)
            .push(available_vars)
            .send(client_);
    }

    void run_event_loop()
    {
        // #region agent log
        std::ofstream log_file_run_event_loop_entry(
            "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
            std::ios::app);
        if (log_file_run_event_loop_entry.is_open()) {
            log_file_run_event_loop_entry
                << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H15","location":"oid_bridge.cpp:612","message":"run_event_loop_entry","data":{"thread_id":)"
                << std::this_thread::get_id() << R"(},"timestamp":)"
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()
                << "}\n";
            log_file_run_event_loop_entry.close();
        }
        // #endregion

        try_read_incoming_messages(static_cast<int>(1000.0 / 5.0));

        // #region agent log
        std::ofstream log_file_after_try_read(
            "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
            std::ios::app);
        if (log_file_after_try_read.is_open()) {
            log_file_after_try_read
                << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H15","location":"oid_bridge.cpp:625","message":"run_event_loop_after_try_read","data":{},"timestamp":)"
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()
                << "}\n";
            log_file_after_try_read.close();
        }
        // #endregion

        auto plot_request_message = std::make_unique<UiMessage>();
        while ((plot_request_message = try_get_stored_message(
                    oid::MessageType::PlotBufferRequest)) != nullptr) {
            // #region agent log
            std::ofstream log_file_plot_request_found(
                "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
                std::ios::app);
            if (log_file_plot_request_found.is_open()) {
                log_file_plot_request_found
                    << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H15","location":"oid_bridge.cpp:635","message":"plot_request_found","data":{},"timestamp":)"
                    << std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count()
                    << "}\n";
                log_file_plot_request_found.close();
            }
            // #endregion

            const PlotBufferRequestMessage* msg =
                dynamic_cast<PlotBufferRequestMessage*>(
                    plot_request_message.get());
            if (plot_callback_) {
                // #region agent log
                std::ofstream log_file_before_plot_callback(
                    "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
                    std::ios::app);
                if (log_file_before_plot_callback.is_open()) {
                    log_file_before_plot_callback
                        << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H15","location":"oid_bridge.cpp:645","message":"before_plot_callback","data":{"buffer_name":")"
                        << msg->buffer_name << R"("},"timestamp":)"
                        << std::chrono::duration_cast<
                               std::chrono::milliseconds>(
                               std::chrono::system_clock::now()
                                   .time_since_epoch())
                               .count()
                        << "}\n";
                    log_file_before_plot_callback.close();
                }
                // #endregion

                plot_callback_(msg->buffer_name.c_str());

                // #region agent log
                std::ofstream log_file_after_plot_callback(
                    "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
                    std::ios::app);
                if (log_file_after_plot_callback.is_open()) {
                    log_file_after_plot_callback
                        << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H15","location":"oid_bridge.cpp:655","message":"after_plot_callback","data":{},"timestamp":)"
                        << std::chrono::duration_cast<
                               std::chrono::milliseconds>(
                               std::chrono::system_clock::now()
                                   .time_since_epoch())
                               .count()
                        << "}\n";
                    log_file_after_plot_callback.close();
                }
                // #endregion
            }
        }

        // #region agent log
        std::ofstream log_file_run_event_loop_exit(
            "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
            std::ios::app);
        if (log_file_run_event_loop_exit.is_open()) {
            log_file_run_event_loop_exit
                << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H15","location":"oid_bridge.cpp:665","message":"run_event_loop_exit","data":{},"timestamp":)"
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()
                << "}\n";
            log_file_run_event_loop_exit.close();
        }
        // #endregion
    }

    void plot_buffer(const PlotBufferParams& params) const
    {
        const auto& variable_name_str = params.variable_name_str;
        const auto& display_name_str  = params.display_name_str;
        const auto& pixel_layout_str  = params.pixel_layout_str;
        const auto transpose_buffer   = params.transpose_buffer;
        const auto buff_width         = params.buff_width;
        const auto buff_height        = params.buff_height;
        const auto buff_channels      = params.buff_channels;
        const auto buff_stride        = params.buff_stride;
        const auto buff_type          = params.buff_type;
        const auto& buffer            = params.buffer;

        auto message_composer = oid::MessageComposer{};
        message_composer.push(oid::MessageType::PlotBufferContents)
            .push(variable_name_str)
            .push(display_name_str)
            .push(pixel_layout_str)
            .push(transpose_buffer)
            .push(buff_width)
            .push(buff_height)
            .push(buff_channels)
            .push(buff_stride)
            .push(buff_type)
            .push(buffer)
            .send(client_);
    }

    ~OidBridge() noexcept
    {
        ui_proc_.kill();
    }

  private:
    oid::Process ui_proc_{};
    QTcpServer server_{};
    QPointer<QTcpSocket> client_{}; // Qt-managed non-owning pointer
    std::string oid_path_{};

    std::function<int(const char*)> plot_callback_{};

    std::map<oid::MessageType, std::unique_ptr<UiMessage>> received_messages_{};

    std::unique_ptr<UiMessage>
    try_get_stored_message(const oid::MessageType& msg_type)
    {
        if (const auto find_msg_handler = received_messages_.find(msg_type);
            find_msg_handler != received_messages_.end()) {
            auto result = std::move(find_msg_handler->second);
            received_messages_.erase(find_msg_handler);
            return result;
        }

        return nullptr;
    }


    void try_read_incoming_messages(const int msecs = 3000)
    {
        assert(!client_.isNull());

        do {
            client_->waitForReadyRead(msecs);

            if (client_.isNull() || client_->bytesAvailable() == 0) {
                break;
            }

            auto header = oid::MessageType{};
            client_->read(std::bit_cast<char*>(&header),
                          static_cast<qint64>(sizeof(header)));

            switch (header) {
            case oid::MessageType::PlotBufferRequest:
                received_messages_[header] = decode_plot_buffer_request();
                break;
            case oid::MessageType::GetObservedSymbolsResponse:
                received_messages_[header] =
                    decode_get_observed_symbols_response();
                break;
            default:
                std::cerr
                    << "[OpenImageDebugger] Received message with incorrect "
                       "header"
                    << std::endl;
                break;
            }
        } while (!client_.isNull() && client_->bytesAvailable() > 0);
    }


    [[nodiscard]] std::unique_ptr<UiMessage> decode_plot_buffer_request() const
    {
        assert(!client_.isNull());

        auto response        = std::make_unique<PlotBufferRequestMessage>();
        auto message_decoder = oid::MessageDecoder{client_};
        message_decoder.read(response->buffer_name);
        return response;
    }

    [[nodiscard]] std::unique_ptr<UiMessage>
    decode_get_observed_symbols_response() const
    {
        assert(!client_.isNull());

        auto response = std::make_unique<GetObservedSymbolsResponseMessage>();

        auto message_decoder = oid::MessageDecoder{client_};
        message_decoder.read<std::deque<std::string>, std::string>(
            response->observed_symbols);

        return response;
    }

    std::unique_ptr<UiMessage> fetch_message(const oid::MessageType& msg_type)
    {
        // Return message if it was already received before
        if (auto result = try_get_stored_message(msg_type); result != nullptr) {
            return result;
        }

        // Try to fetch message
        try_read_incoming_messages();

        return try_get_stored_message(msg_type);
    }


    void wait_for_client()
    {
        // #region agent log
        std::ofstream log_file_wait_client_entry(
            "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
            std::ios::app);
        if (log_file_wait_client_entry.is_open()) {
            log_file_wait_client_entry
                << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H11","location":"oid_bridge.cpp:604","message":"wait_for_client_entry","data":{"client_is_null":)"
                << (client_.isNull() ? "true" : "false") << R"(},"timestamp":)"
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()
                << "}\n";
            log_file_wait_client_entry.close();
        }
        // #endregion

        if (client_.isNull()) {
            // #region agent log
            std::ofstream log_file_before_waitForNewConnection(
                "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
                std::ios::app);
            if (log_file_before_waitForNewConnection.is_open()) {
                log_file_before_waitForNewConnection
                    << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H11","location":"oid_bridge.cpp:620","message":"before_waitForNewConnection","data":{},"timestamp":)"
                    << std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count()
                    << "}\n";
                log_file_before_waitForNewConnection.close();
            }
            // #endregion

            if (!server_.waitForNewConnection(10000)) {
                // #region agent log
                std::ofstream log_file_waitForNewConnection_timeout(
                    "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
                    std::ios::app);
                if (log_file_waitForNewConnection_timeout.is_open()) {
                    log_file_waitForNewConnection_timeout
                        << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H11","location":"oid_bridge.cpp:630","message":"waitForNewConnection_timeout","data":{},"timestamp":)"
                        << std::chrono::duration_cast<
                               std::chrono::milliseconds>(
                               std::chrono::system_clock::now()
                                   .time_since_epoch())
                               .count()
                        << "}\n";
                    log_file_waitForNewConnection_timeout.close();
                }
                // #endregion
                std::cerr << "[OpenImageDebugger] No clients connected to "
                             "OpenImageDebugger server"
                          << std::endl;
            } else {
                // #region agent log
                std::ofstream log_file_waitForNewConnection_success(
                    "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
                    std::ios::app);
                if (log_file_waitForNewConnection_success.is_open()) {
                    log_file_waitForNewConnection_success
                        << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H11","location":"oid_bridge.cpp:645","message":"waitForNewConnection_success","data":{},"timestamp":)"
                        << std::chrono::duration_cast<
                               std::chrono::milliseconds>(
                               std::chrono::system_clock::now()
                                   .time_since_epoch())
                               .count()
                        << "}\n";
                    log_file_waitForNewConnection_success.close();
                }
                // #endregion
            }
            client_ = QPointer<QTcpSocket>(server_.nextPendingConnection());

            // #region agent log
            std::ofstream log_file_after_nextPendingConnection(
                "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
                std::ios::app);
            if (log_file_after_nextPendingConnection.is_open()) {
                log_file_after_nextPendingConnection
                    << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H11","location":"oid_bridge.cpp:660","message":"after_nextPendingConnection","data":{"client_is_null":)"
                    << (client_.isNull() ? "true" : "false")
                    << R"(},"timestamp":)"
                    << std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count()
                    << "}\n";
                log_file_after_nextPendingConnection.close();
            }
            // #endregion
        }

        // #region agent log
        std::ofstream log_file_wait_client_exit(
            "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
            std::ios::app);
        if (log_file_wait_client_exit.is_open()) {
            log_file_wait_client_exit
                << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H11","location":"oid_bridge.cpp:675","message":"wait_for_client_exit","data":{"client_is_null":)"
                << (client_.isNull() ? "true" : "false") << R"(},"timestamp":)"
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()
                << "}\n";
            log_file_wait_client_exit.close();
        }
        // #endregion
    }
};


// C++ implementation that accepts std::function (avoids function pointer in
// implementation)
namespace
{
std::unique_ptr<OidBridge>
oid_initialize_impl(std::function<int(const char*)> plot_callback,
                    PyObject* optional_parameters)
{
    // #region agent log
    std::ofstream log_file(
        "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log", std::ios::app);
    if (log_file.is_open()) {
        log_file
            << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H4","location":"oid_bridge.cpp:363","message":"oid_initialize_impl_entry","data":{"thread_id":)"
            << std::this_thread::get_id() << R"(},"timestamp":)"
            << std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count()
            << "}\n";
        log_file.close();
    }
    // #endregion

    // #region agent log
    std::ofstream log_file2(
        "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log", std::ios::app);
    if (log_file2.is_open()) {
        log_file2
            << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H4","location":"oid_bridge.cpp:370","message":"before_PyGILRAII","data":{},"timestamp":)"
            << std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count()
            << "}\n";
        log_file2.close();
    }
    // #endregion
    const auto py_gil_raii = PyGILRAII{};

    // #region agent log
    std::ofstream log_file3(
        "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log", std::ios::app);
    if (log_file3.is_open()) {
        log_file3
            << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H4","location":"oid_bridge.cpp:377","message":"after_PyGILRAII","data":{},"timestamp":)"
            << std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count()
            << "}\n";
        log_file3.close();
    }
    // #endregion

    // #region agent log
    std::ofstream log_file_dict_check(
        "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log", std::ios::app);
    if (log_file_dict_check.is_open()) {
        log_file_dict_check
            << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H1","location":"oid_bridge.cpp:663","message":"before_PyDict_Check","data":{},"timestamp":)"
            << std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count()
            << "}\n";
        log_file_dict_check.close();
    }
    // #endregion
    if (optional_parameters != nullptr && !PyDict_Check(optional_parameters))
        [[unlikely]] {
        // #region agent log
        std::ofstream log_file_dict_error(
            "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
            std::ios::app);
        if (log_file_dict_error.is_open()) {
            log_file_dict_error
                << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H1","location":"oid_bridge.cpp:669","message":"PyDict_Check_failed","data":{},"timestamp":)"
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()
                << "}\n";
            log_file_dict_error.close();
        }
        // #endregion
        RAISE_PY_EXCEPTION(
            PyExc_TypeError,
            "Invalid second parameter given to oid_initialize (was expecting"
            " a dict).");
        return nullptr;
    }
    // #region agent log
    std::ofstream log_file_dict_success(
        "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log", std::ios::app);
    if (log_file_dict_success.is_open()) {
        log_file_dict_success
            << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H1","location":"oid_bridge.cpp:676","message":"PyDict_Check_success","data":{},"timestamp":)"
            << std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count()
            << "}\n";
        log_file_dict_success.close();
    }
    // #endregion

    /*
     * Get optional fields
     */
    // #region agent log
    std::ofstream log_file_dict_get(
        "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log", std::ios::app);
    if (log_file_dict_get.is_open()) {
        log_file_dict_get
            << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H1","location":"oid_bridge.cpp:700","message":"before_PyDict_GetItemString","data":{},"timestamp":)"
            << std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count()
            << "}\n";
        log_file_dict_get.close();
    }
    // #endregion
    PyObject* py_oid_path = nullptr;
    if (optional_parameters != nullptr) {
        // #region agent log
        std::ofstream log_file_dict_get_call(
            "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
            std::ios::app);
        if (log_file_dict_get_call.is_open()) {
            log_file_dict_get_call
                << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H1","location":"oid_bridge.cpp:736","message":"before_PyDict_Size_test","data":{"optional_params_not_null":true},"timestamp":)"
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()
                << "}\n";
            log_file_dict_get_call.close();
        }
        // #endregion

        // H1: Test PyDict_Size BEFORE logging to avoid abort during log
        // formatting H2-H5: Test if dictionary operations require proper thread
        // state registration #region agent log
        std::ofstream log_file_before_dict_size(
            "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
            std::ios::app);
        if (log_file_before_dict_size.is_open()) {
            PyThreadState* current_tstate = PyGILState_GetThisThreadState();
            bool py_init                  = Py_IsInitialized() != 0;
            log_file_before_dict_size
                << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H2","location":"oid_bridge.cpp:755","message":"before_PyDict_Size","data":{"thread_state":)"
                << (current_tstate ? "not_null" : "null")
                << R"(,"py_initialized":)" << (py_init ? "true" : "false")
                << R"(},"timestamp":)"
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()
                << "}\n";
            log_file_before_dict_size.close();
        }
        // #endregion

        // Test if we can safely call Python APIs by trying a lightweight
        // operation first PyDict_Size() requires GIL and is safer than
        // PyDict_GetItemString
        Py_ssize_t dict_size = PyDict_Size(optional_parameters);

        // #region agent log
        std::ofstream log_file_dict_size(
            "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
            std::ios::app);
        if (log_file_dict_size.is_open()) {
            log_file_dict_size
                << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H1","location":"oid_bridge.cpp:775","message":"PyDict_Size_success","data":{"dict_size":)"
                << dict_size << R"(},"timestamp":)"
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()
                << "}\n";
            log_file_dict_size.close();
        }
        // #endregion

        // #region agent log
        std::ofstream log_file_before_dict_get(
            "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
            std::ios::app);
        if (log_file_before_dict_get.is_open()) {
            log_file_before_dict_get
                << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H3","location":"oid_bridge.cpp:790","message":"before_PyDict_GetItemString","data":{"dict_size":)"
                << dict_size << R"(},"timestamp":)"
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()
                << "}\n";
            log_file_before_dict_get.close();
        }
        // #endregion

        // H7: Use PyDict_Next() to iterate through dictionary directly - this
        // gives us existing key/value pairs without creating new objects.
        // PyDict_Keys() creates a new list, which might abort. PyDict_Next()
        // just iterates through existing pairs.
        // #region agent log
        std::ofstream log_file_before_dict_next(
            "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
            std::ios::app);
        if (log_file_before_dict_next.is_open()) {
            log_file_before_dict_next
                << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H7","location":"oid_bridge.cpp:815","message":"before_PyDict_Next_iteration","data":{},"timestamp":)"
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()
                << "}\n";
            log_file_before_dict_next.close();
        }
        // #endregion

        // Iterate through dictionary using PyDict_Next - this avoids creating
        // new objects and uses existing key/value pairs
        Py_ssize_t pos      = 0;
        PyObject* key       = nullptr;
        PyObject* value     = nullptr;
        int iteration_count = 0;
        PyObject* fallback_value =
            nullptr; // Store value if key is empty (LLDB Python bug workaround)

        while (PyDict_Next(optional_parameters, &pos, &key, &value)) {
            iteration_count++;
            // #region agent log
            std::ofstream log_file_dict_next_iteration(
                "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
                std::ios::app);
            if (log_file_dict_next_iteration.is_open()) {
                log_file_dict_next_iteration
                    << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H7","location":"oid_bridge.cpp:840","message":"PyDict_Next_iteration","data":{"iteration":)"
                    << iteration_count << R"(,"key":)"
                    << (key ? "not_null" : "null") << R"(,"value":)"
                    << (value ? "not_null" : "null") << R"(},"timestamp":)"
                    << std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count()
                    << "}\n";
                log_file_dict_next_iteration.close();
            }
            // #endregion

            // key and value are borrowed references from the dictionary
            if (key != nullptr && PyUnicode_Check(key)) {
                // #region agent log
                std::ofstream log_file_before_unicode_asutf8(
                    "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
                    std::ios::app);
                if (log_file_before_unicode_asutf8.is_open()) {
                    log_file_before_unicode_asutf8
                        << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H8","location":"oid_bridge.cpp:860","message":"before_PyUnicode_AsUTF8","data":{"iteration":)"
                        << iteration_count << R"(},"timestamp":)"
                        << std::chrono::duration_cast<
                               std::chrono::milliseconds>(
                               std::chrono::system_clock::now()
                                   .time_since_epoch())
                               .count()
                        << "}\n";
                    log_file_before_unicode_asutf8.close();
                }
                // #endregion

                // Try to get UTF-8 string for comparison
                // Note: PyUnicode_AsUTF8 might abort, but we'll try it
                const char* key_str = PyUnicode_AsUTF8(key);

                // #region agent log
                std::ofstream log_file_after_unicode_asutf8(
                    "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
                    std::ios::app);
                if (log_file_after_unicode_asutf8.is_open()) {
                    log_file_after_unicode_asutf8
                        << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H8","location":"oid_bridge.cpp:875","message":"after_PyUnicode_AsUTF8","data":{"iteration":)"
                        << iteration_count << R"(,"key_str":)"
                        << (key_str ? "not_null" : "null")
                        << R"(},"timestamp":)"
                        << std::chrono::duration_cast<
                               std::chrono::milliseconds>(
                               std::chrono::system_clock::now()
                                   .time_since_epoch())
                               .count()
                        << "}\n";
                    log_file_after_unicode_asutf8.close();
                }
                // #endregion

                // #region agent log
                std::ofstream log_file_key_comparison(
                    "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
                    std::ios::app);
                if (log_file_key_comparison.is_open()) {
                    log_file_key_comparison
                        << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H9","location":"oid_bridge.cpp:895","message":"key_comparison","data":{"iteration":)"
                        << iteration_count << R"(,"key_str":")"
                        << (key_str ? key_str : "null")
                        << R"(","target":"oid_path","match":)"
                        << (key_str && strcmp(key_str, "oid_path") == 0
                                ? "true"
                                : "false")
                        << R"(},"timestamp":)"
                        << std::chrono::duration_cast<
                               std::chrono::milliseconds>(
                               std::chrono::system_clock::now()
                                   .time_since_epoch())
                               .count()
                        << "}\n";
                    log_file_key_comparison.close();
                }
                // #endregion

                if (key_str != nullptr && strcmp(key_str, "oid_path") == 0) {
                    // Found the key - value already contains the result
                    py_oid_path = value; // Borrowed reference
                    // #region agent log
                    std::ofstream log_file_key_found(
                        "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
                        std::ios::app);
                    if (log_file_key_found.is_open()) {
                        log_file_key_found
                            << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H7","location":"oid_bridge.cpp:890","message":"key_found_via_PyDict_Next","data":{},"timestamp":)"
                            << std::chrono::duration_cast<
                                   std::chrono::milliseconds>(
                                   std::chrono::system_clock::now()
                                       .time_since_epoch())
                                   .count()
                            << "}\n";
                        log_file_key_found.close();
                    }
                    // #endregion
                    break;
                } else if (key_str != nullptr && strlen(key_str) == 0 &&
                           value != nullptr) {
                    // H13: LLDB Python bug workaround - key is empty string but
                    // value might be the path we need Store it as fallback if
                    // dictionary has only one entry
                    // #region agent log
                    std::ofstream log_file_empty_key_value_check(
                        "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
                        std::ios::app);
                    if (log_file_empty_key_value_check.is_open()) {
                        // Try to extract the value to see what it contains
                        const char* value_str = nullptr;
                        if (PyUnicode_Check(value)) {
                            value_str = PyUnicode_AsUTF8(value);
                        }
                        log_file_empty_key_value_check
                            << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H13","location":"oid_bridge.cpp:1075","message":"empty_key_value_check","data":{"iteration":)"
                            << iteration_count << R"(,"value_str":)"
                            << (value_str ? (value_str[0] ? "\"" : "\"\"")
                                          : "null")
                            << (value_str && value_str[0] ? value_str : "")
                            << (value_str ? "\"" : "")
                            << R"(,"value_str_length":)"
                            << (value_str ? strlen(value_str) : 0)
                            << R"(},"timestamp":)"
                            << std::chrono::duration_cast<
                                   std::chrono::milliseconds>(
                                   std::chrono::system_clock::now()
                                       .time_since_epoch())
                                   .count()
                            << "}\n";
                        log_file_empty_key_value_check.close();
                    }
                    // #endregion
                    fallback_value = value; // Borrowed reference
                    // #region agent log
                    std::ofstream log_file_empty_key_fallback(
                        "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
                        std::ios::app);
                    if (log_file_empty_key_fallback.is_open()) {
                        log_file_empty_key_fallback
                            << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H13","location":"oid_bridge.cpp:1105","message":"empty_key_fallback_stored","data":{"iteration":)"
                            << iteration_count << R"(},"timestamp":)"
                            << std::chrono::duration_cast<
                                   std::chrono::milliseconds>(
                                   std::chrono::system_clock::now()
                                       .time_since_epoch())
                                   .count()
                            << "}\n";
                        log_file_empty_key_fallback.close();
                    }
                    // #endregion
                }
            }
        }

        // #region agent log
        std::ofstream log_file_dict_next_complete(
            "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
            std::ios::app);
        if (log_file_dict_next_complete.is_open()) {
            log_file_dict_next_complete
                << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H7","location":"oid_bridge.cpp:905","message":"PyDict_Next_complete","data":{"iteration_count":)"
                << iteration_count << R"(,"py_oid_path":)"
                << (py_oid_path ? "not_null" : "null") << R"(},"timestamp":)"
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()
                << "}\n";
            log_file_dict_next_complete.close();
        }
        // #endregion

        // H13: If we didn't find "oid_path" key but found an empty key with a
        // value and dictionary has only one entry, use the fallback value (LLDB
        // Python bug workaround)
        if (py_oid_path == nullptr && fallback_value != nullptr &&
            dict_size == 1) {
            py_oid_path = fallback_value; // Borrowed reference
            // #region agent log
            std::ofstream log_file_fallback_used(
                "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
                std::ios::app);
            if (log_file_fallback_used.is_open()) {
                log_file_fallback_used
                    << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H13","location":"oid_bridge.cpp:1115","message":"using_empty_key_fallback","data":{"dict_size":)"
                    << dict_size << R"(},"timestamp":)"
                    << std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count()
                    << "}\n";
                log_file_fallback_used.close();
            }
            // #endregion
        }

        // If we didn't find the key via iteration, the key doesn't exist in
        // the dictionary. Don't fall back to PyDict_GetItemString as it aborts.
        // Just leave py_oid_path as nullptr - this is a valid case (optional
        // parameter).
        if (py_oid_path == nullptr) {
            // #region agent log
            std::ofstream log_file_key_not_found(
                "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
                std::ios::app);
            if (log_file_key_not_found.is_open()) {
                log_file_key_not_found
                    << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H10","location":"oid_bridge.cpp:1135","message":"key_oid_path_not_found","data":{},"timestamp":)"
                    << std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count()
                    << "}\n";
                log_file_key_not_found.close();
            }
            // #endregion
            // py_oid_path remains nullptr - this is fine, it's an optional
            // parameter
        }

        // #region agent log
        std::ofstream log_file_dict_get_success(
            "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
            std::ios::app);
        if (log_file_dict_get_success.is_open()) {
            log_file_dict_get_success
                << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H3","location":"oid_bridge.cpp:890","message":"final_py_oid_path_result","data":{"py_oid_path":)"
                << (py_oid_path ? "not_null" : "null") << R"(},"timestamp":)"
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()
                << "}\n";
            log_file_dict_get_success.close();
        }
        // #endregion
    } else {
        // #region agent log
        std::ofstream log_file_dict_get_null(
            "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
            std::ios::app);
        if (log_file_dict_get_null.is_open()) {
            log_file_dict_get_null
                << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H1","location":"oid_bridge.cpp:760","message":"optional_parameters_is_null","data":{},"timestamp":)"
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()
                << "}\n";
            log_file_dict_get_null.close();
        }
        // #endregion
    }

    // #region agent log
    std::ofstream log_file4(
        "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log", std::ios::app);
    if (log_file4.is_open()) {
        log_file4
            << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H4","location":"oid_bridge.cpp:395","message":"before_OidBridge_constructor","data":{},"timestamp":)"
            << std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count()
            << "}\n";
        log_file4.close();
    }
    // #endregion
    auto app = std::make_unique<OidBridge>(std::move(plot_callback));

    // #region agent log
    std::ofstream log_file5(
        "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log", std::ios::app);
    if (log_file5.is_open()) {
        log_file5
            << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H4","location":"oid_bridge.cpp:401","message":"after_OidBridge_constructor","data":{},"timestamp":)"
            << std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count()
            << "}\n";
        log_file5.close();
    }
    // #endregion

    if (py_oid_path) {
        // #region agent log
        std::ofstream log_file_before_copy(
            "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
            std::ios::app);
        if (log_file_before_copy.is_open()) {
            log_file_before_copy
                << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H13","location":"oid_bridge.cpp:1225","message":"before_copy_py_string","data":{"py_oid_path":not_null},"timestamp":)"
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()
                << "}\n";
            log_file_before_copy.close();
        }
        // #endregion
        auto oid_path_str = std::string{};
        oid::copy_py_string(oid_path_str, py_oid_path);
        // #region agent log
        std::ofstream log_file_after_copy(
            "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
            std::ios::app);
        if (log_file_after_copy.is_open()) {
            log_file_after_copy
                << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H13","location":"oid_bridge.cpp:1227","message":"after_copy_py_string","data":{"oid_path_str":")"
                << oid_path_str << R"(","oid_path_str_length":)"
                << oid_path_str.length() << R"(},"timestamp":)"
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()
                << "}\n";
            log_file_after_copy.close();
        }
        // #endregion
        app->set_path(oid_path_str);
    } else {
        // #region agent log
        std::ofstream log_file_py_oid_path_null(
            "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
            std::ios::app);
        if (log_file_py_oid_path_null.is_open()) {
            log_file_py_oid_path_null
                << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H13","location":"oid_bridge.cpp:1225","message":"py_oid_path_is_null","data":{},"timestamp":)"
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()
                << "}\n";
            log_file_py_oid_path_null.close();
        }
        // #endregion
    }

    // #region agent log
    std::ofstream log_file6(
        "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log", std::ios::app);
    if (log_file6.is_open()) {
        log_file6
            << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H4","location":"oid_bridge.cpp:411","message":"oid_initialize_impl_exit","data":{},"timestamp":)"
            << std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count()
            << "}\n";
        log_file6.close();
    }
    // #endregion

    return app;
}
} // namespace


// NOSONAR: C API requires function pointer (extern "C")
AppHandler oid_initialize(int (*plot_callback)(const char*), // NOSONAR
                          PyObject* optional_parameters)
{
    // #region agent log
    std::ofstream log_file(
        "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log", std::ios::app);
    if (log_file.is_open()) {
        log_file
            << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H4","location":"oid_bridge.cpp:398","message":"oid_initialize_entry","data":{"thread_id":)"
            << std::this_thread::get_id() << R"(},"timestamp":)"
            << std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count()
            << "}\n";
        log_file.close();
    }
    // #endregion

    // Convert C-style function pointer to std::function for modern C++
    // implementation. Function pointer parameter required for C API
    // compatibility (extern "C" interface)
    try {
        auto app = oid_initialize_impl(
            plot_callback ? std::function<int(const char*)>{plot_callback}
                          : std::function<int(const char*)>{},
            optional_parameters);

        // H18: In LLDB mode, we cannot create ANY Python objects, even from the
        // main thread. PyList_New(0) crashes. Skip empty list initialization
        // and return nullptr from oid_get_observed_buffers instead. Python side
        // will handle None gracefully.

        // #region agent log
        std::ofstream log_file2(
            "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
            std::ios::app);
        if (log_file2.is_open()) {
            log_file2
                << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H4","location":"oid_bridge.cpp:410","message":"oid_initialize_impl_success","data":{"app_is_null":)"
                << (app == nullptr ? "true" : "false") << R"(},"timestamp":)"
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()
                << "}\n";
            log_file2.close();
        }
        // #endregion

        return app ? app.release() : nullptr;
    } catch (const std::exception& e) {
        // #region agent log
        std::ofstream log_file3(
            "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
            std::ios::app);
        if (log_file3.is_open()) {
            log_file3
                << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H4","location":"oid_bridge.cpp:418","message":"oid_initialize_exception","data":{"what":")"
                << e.what() << R"("},"timestamp":)"
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()
                << "}\n";
            log_file3.close();
        }
        // #endregion
        return nullptr;
    } catch (...) {
        // #region agent log
        std::ofstream log_file4(
            "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
            std::ios::app);
        if (log_file4.is_open()) {
            log_file4
                << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H4","location":"oid_bridge.cpp:426","message":"oid_initialize_unknown_exception","data":{},"timestamp":)"
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()
                << "}\n";
            log_file4.close();
        }
        // #endregion
        return nullptr;
    }
}


void oid_cleanup(const AppHandler handler)
{
    const auto py_gil_raii = PyGILRAII{};

    auto app = std::unique_ptr<OidBridge>{static_cast<OidBridge*>(handler)};

    if (!app) [[unlikely]] {
        RAISE_PY_EXCEPTION(PyExc_RuntimeError,
                           "oid_terminate received null application handler");
        return;
    }

    app.reset();
}


void oid_exec(const AppHandler handler)
{
    // #region agent log
    std::ofstream log_file(
        "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log", std::ios::app);
    if (log_file.is_open()) {
        log_file
            << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H4","location":"oid_bridge.cpp:428","message":"oid_exec_entry","data":{"thread_id":)"
            << std::this_thread::get_id() << R"(},"timestamp":)"
            << std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count()
            << "}\n";
        log_file.close();
    }
    // #endregion

    const auto py_gil_raii = PyGILRAII{};

    // #region agent log
    std::ofstream log_file2(
        "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log", std::ios::app);
    if (log_file2.is_open()) {
        log_file2
            << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H4","location":"oid_bridge.cpp:437","message":"oid_exec_after_gil","data":{},"timestamp":)"
            << std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count()
            << "}\n";
        log_file2.close();
    }
    // #endregion

    const auto app = static_cast<OidBridge*>(handler);

    if (app == nullptr) [[unlikely]] {
        RAISE_PY_EXCEPTION(PyExc_RuntimeError,
                           "oid_exec received null application handler");
        return;
    }

    // #region agent log
    std::ofstream log_file3(
        "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log", std::ios::app);
    if (log_file3.is_open()) {
        log_file3
            << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H4","location":"oid_bridge.cpp:450","message":"oid_exec_before_start","data":{},"timestamp":)"
            << std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count()
            << "}\n";
        log_file3.close();
    }
    // #endregion

    app->start();

    // #region agent log
    std::ofstream log_file4(
        "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log", std::ios::app);
    if (log_file4.is_open()) {
        log_file4
            << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H4","location":"oid_bridge.cpp:458","message":"oid_exec_after_start","data":{},"timestamp":)"
            << std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count()
            << "}\n";
        log_file4.close();
    }
    // #endregion
}


int oid_is_window_ready(const AppHandler handler)
{
    // #region agent log
    std::ofstream log_file_oid_is_window_ready(
        "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log", std::ios::app);
    if (log_file_oid_is_window_ready.is_open()) {
        log_file_oid_is_window_ready
            << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H16","location":"oid_bridge.cpp:1714","message":"oid_is_window_ready_entry","data":{"thread_id":)"
            << std::this_thread::get_id() << R"(},"timestamp":)"
            << std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count()
            << "}\n";
        log_file_oid_is_window_ready.close();
    }
    // #endregion
    const auto py_gil_raii = PyGILRAII{};

    const auto app = static_cast<OidBridge*>(handler);

    if (app == nullptr) [[unlikely]] {
        RAISE_PY_EXCEPTION(PyExc_RuntimeError,
                           "oid_exec received null application handler");
        return 0;
    }

    return app->is_window_ready();
}


PyObject* oid_get_observed_buffers(AppHandler handler)
{
    // #region agent log
    std::ofstream log_file_oid_get_observed_buffers(
        "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log", std::ios::app);
    if (log_file_oid_get_observed_buffers.is_open()) {
        log_file_oid_get_observed_buffers
            << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H16","location":"oid_bridge.cpp:1730","message":"oid_get_observed_buffers_entry","data":{"thread_id":)"
            << std::this_thread::get_id() << R"(},"timestamp":)"
            << std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count()
            << "}\n";
        log_file_oid_get_observed_buffers.close();
    }
    // #endregion
    const auto py_gil_raii = PyGILRAII{};

    const auto app = static_cast<OidBridge*>(handler);

    if (app == nullptr) [[unlikely]] {
        RAISE_PY_EXCEPTION(PyExc_Exception,
                           "oid_get_observed_buffers received null "
                           "application handler");
        return nullptr;
    }

    const auto observed_symbols = app->get_observed_symbols();

    // H18: In LLDB mode, we cannot create ANY Python objects, even from the
    // main thread. PyList_New(0) crashes. Return Py_None (Python's None object)
    // instead of nullptr. Py_None is a singleton that always exists, so we can
    // safely return it even in LLDB mode.
    if (PyGILRAII::is_lldb_mode()) {
        // Return Py_None - Python side will check for None before iterating
        // #region agent log
        std::ofstream log_file_lldb_mode_early_return(
            "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
            std::ios::app);
        if (log_file_lldb_mode_early_return.is_open()) {
            log_file_lldb_mode_early_return
                << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H18","location":"oid_bridge.cpp:1800","message":"oid_get_observed_buffers_lldb_mode_early_return","data":{"observed_symbols_size":)"
                << observed_symbols.size() << R"(},"timestamp":)"
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()
                << "}\n";
            log_file_lldb_mode_early_return.close();
        }
        // #endregion
        // #region agent log
        std::ofstream log_file_before_py_incref_none(
            "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
            std::ios::app);
        if (log_file_before_py_incref_none.is_open()) {
            log_file_before_py_incref_none
                << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H18","location":"oid_bridge.cpp:1820","message":"before_Py_INCREF_None","data":{},"timestamp":)"
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()
                << "}\n";
            log_file_before_py_incref_none.close();
        }
        // #endregion
        Py_INCREF(Py_None);
        // #region agent log
        std::ofstream log_file_after_py_incref_none(
            "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
            std::ios::app);
        if (log_file_after_py_incref_none.is_open()) {
            log_file_after_py_incref_none
                << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H18","location":"oid_bridge.cpp:1830","message":"after_Py_INCREF_None","data":{},"timestamp":)"
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()
                << "}\n";
            log_file_after_py_incref_none.close();
        }
        // #endregion
        return Py_None;
    }

    // #region agent log
    std::ofstream log_file_before_pylist_new(
        "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log", std::ios::app);
    if (log_file_before_pylist_new.is_open()) {
        log_file_before_pylist_new
            << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H17","location":"oid_bridge.cpp:1815","message":"before_PyList_New","data":{"observed_symbols_size":)"
            << observed_symbols.size() << R"(},"timestamp":)"
            << std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count()
            << "}\n";
        log_file_before_pylist_new.close();
    }
    // #endregion
    const auto py_observed_symbols =
        PyList_New(static_cast<Py_ssize_t>(observed_symbols.size()));
    // #region agent log
    std::ofstream log_file_after_pylist_new(
        "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log", std::ios::app);
    if (log_file_after_pylist_new.is_open()) {
        log_file_after_pylist_new
            << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H17","location":"oid_bridge.cpp:1784","message":"after_PyList_New","data":{"py_observed_symbols":)"
            << (py_observed_symbols ? "not_null" : "null")
            << R"(},"timestamp":)"
            << std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count()
            << "}\n";
        log_file_after_pylist_new.close();
    }
    // #endregion

    const auto observed_symbols_sentinel =
        static_cast<int>(observed_symbols.size());
    for (int i = 0; i < observed_symbols_sentinel; ++i) {
        const auto& symbol_name = observed_symbols[i];
        // #region agent log
        std::ofstream log_file_before_pybytes(
            "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
            std::ios::app);
        if (log_file_before_pybytes.is_open()) {
            log_file_before_pybytes
                << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H17","location":"oid_bridge.cpp:1790","message":"before_PyBytes_FromString","data":{"iteration":)"
                << i << R"(,"symbol_name_length":)" << symbol_name.length()
                << R"(},"timestamp":)"
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()
                << "}\n";
            log_file_before_pybytes.close();
        }
        // #endregion
        const auto py_symbol_name = PyBytes_FromString(symbol_name.c_str());
        // #region agent log
        std::ofstream log_file_after_pybytes(
            "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
            std::ios::app);
        if (log_file_after_pybytes.is_open()) {
            log_file_after_pybytes
                << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H17","location":"oid_bridge.cpp:1803","message":"after_PyBytes_FromString","data":{"iteration":)"
                << i << R"(,"py_symbol_name":)"
                << (py_symbol_name ? "not_null" : "null") << R"(},"timestamp":)"
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()
                << "}\n";
            log_file_after_pybytes.close();
        }
        // #endregion

        if (py_symbol_name == nullptr) [[unlikely]] {
            Py_DECREF(py_observed_symbols);
            return nullptr;
        }
        // #region agent log
        std::ofstream log_file_before_pylist_setitem(
            "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
            std::ios::app);
        if (log_file_before_pylist_setitem.is_open()) {
            log_file_before_pylist_setitem
                << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H17","location":"oid_bridge.cpp:1818","message":"before_PyList_SetItem","data":{"iteration":)"
                << i << R"(},"timestamp":)"
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()
                << "}\n";
            log_file_before_pylist_setitem.close();
        }
        // #endregion
        PyList_SetItem(py_observed_symbols, i, py_symbol_name);
        // #region agent log
        std::ofstream log_file_after_pylist_setitem(
            "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
            std::ios::app);
        if (log_file_after_pylist_setitem.is_open()) {
            log_file_after_pylist_setitem
                << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H17","location":"oid_bridge.cpp:1825","message":"after_PyList_SetItem","data":{"iteration":)"
                << i << R"(},"timestamp":)"
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()
                << "}\n";
            log_file_after_pylist_setitem.close();
        }
        // #endregion
    }
    // #region agent log
    std::ofstream log_file_oid_get_observed_buffers_exit(
        "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log", std::ios::app);
    if (log_file_oid_get_observed_buffers_exit.is_open()) {
        log_file_oid_get_observed_buffers_exit
            << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H17","location":"oid_bridge.cpp:1865","message":"oid_get_observed_buffers_exit","data":{"py_observed_symbols":)"
            << (py_observed_symbols ? "not_null" : "null")
            << R"(},"timestamp":)"
            << std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count()
            << "}\n";
        log_file_oid_get_observed_buffers_exit.close();
    }
    // #endregion

    return py_observed_symbols;
}


void oid_set_available_symbols(const AppHandler handler,
                               PyObject* available_vars)
{
    // #region agent log
    std::ofstream log_file_oid_set_available_symbols(
        "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log", std::ios::app);
    if (log_file_oid_set_available_symbols.is_open()) {
        log_file_oid_set_available_symbols
            << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H16","location":"oid_bridge.cpp:1765","message":"oid_set_available_symbols_entry","data":{"thread_id":)"
            << std::this_thread::get_id() << R"(},"timestamp":)"
            << std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count()
            << "}\n";
        log_file_oid_set_available_symbols.close();
    }
    // #endregion
    const auto py_gil_raii = PyGILRAII{};

    // H17: In LLDB mode, avoid calling Python C API functions from event loop
    // thread. PyList_Check, PyList_Size, PyList_GetItem crash when called from
    // non-main threads in LLDB's embedded Python.
    if (PyGILRAII::is_lldb_mode()) {
        // Skip setting available symbols in LLDB mode - this is called from the
        // event loop thread and Python C API calls are unsafe.
        // #region agent log
        std::ofstream log_file_lldb_mode_early_return(
            "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
            std::ios::app);
        if (log_file_lldb_mode_early_return.is_open()) {
            log_file_lldb_mode_early_return
                << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H17","location":"oid_bridge.cpp:1968","message":"oid_set_available_symbols_lldb_mode_early_return","data":{},"timestamp":)"
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()
                << "}\n";
            log_file_lldb_mode_early_return.close();
        }
        // #endregion
        return;
    }

    // #region agent log
    std::ofstream log_file_before_pylist_check(
        "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log", std::ios::app);
    if (log_file_before_pylist_check.is_open()) {
        log_file_before_pylist_check
            << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H17","location":"oid_bridge.cpp:1990","message":"before_PyList_Check","data":{"available_vars":)"
            << (available_vars ? "not_null" : "null") << R"(},"timestamp":)"
            << std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count()
            << "}\n";
        log_file_before_pylist_check.close();
    }
    // #endregion
    assert(PyList_Check(available_vars));
    // #region agent log
    std::ofstream log_file_after_pylist_check(
        "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log", std::ios::app);
    if (log_file_after_pylist_check.is_open()) {
        log_file_after_pylist_check
            << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H17","location":"oid_bridge.cpp:1823","message":"after_PyList_Check","data":{},"timestamp":)"
            << std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count()
            << "}\n";
        log_file_after_pylist_check.close();
    }
    // #endregion

    const auto app = static_cast<OidBridge*>(handler);

    if (app == nullptr) [[unlikely]] {
        RAISE_PY_EXCEPTION(PyExc_RuntimeError,
                           "oid_set_available_symbols received null "
                           "application handler");
        return;
    }
    // #region agent log
    std::ofstream log_file_before_pylist_size(
        "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log", std::ios::app);
    if (log_file_before_pylist_size.is_open()) {
        log_file_before_pylist_size
            << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H17","location":"oid_bridge.cpp:1840","message":"before_PyList_Size","data":{},"timestamp":)"
            << std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count()
            << "}\n";
        log_file_before_pylist_size.close();
    }
    // #endregion
    auto available_vars_stl = std::deque<std::string>{};
    const auto list_size    = PyList_Size(available_vars);
    // #region agent log
    std::ofstream log_file_after_pylist_size(
        "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log", std::ios::app);
    if (log_file_after_pylist_size.is_open()) {
        log_file_after_pylist_size
            << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H17","location":"oid_bridge.cpp:1845","message":"after_PyList_Size","data":{"list_size":)"
            << list_size << R"(},"timestamp":)"
            << std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count()
            << "}\n";
        log_file_after_pylist_size.close();
    }
    // #endregion
    for (Py_ssize_t pos = 0; pos < list_size; ++pos) {
        auto var_name_str = std::string{};
        // #region agent log
        std::ofstream log_file_before_pylist_getitem(
            "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
            std::ios::app);
        if (log_file_before_pylist_getitem.is_open()) {
            log_file_before_pylist_getitem
                << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H17","location":"oid_bridge.cpp:1854","message":"before_PyList_GetItem","data":{"pos":)"
                << pos << R"(},"timestamp":)"
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()
                << "}\n";
            log_file_before_pylist_getitem.close();
        }
        // #endregion
        const auto listItem = PyList_GetItem(available_vars, pos);
        // #region agent log
        std::ofstream log_file_after_pylist_getitem(
            "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
            std::ios::app);
        if (log_file_after_pylist_getitem.is_open()) {
            log_file_after_pylist_getitem
                << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H17","location":"oid_bridge.cpp:1867","message":"after_PyList_GetItem","data":{"pos":)"
                << pos << R"(,"listItem":)" << (listItem ? "not_null" : "null")
                << R"(},"timestamp":)"
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()
                << "}\n";
            log_file_after_pylist_getitem.close();
        }
        // #endregion
        oid::copy_py_string(var_name_str, listItem);
        available_vars_stl.push_back(var_name_str);
    }
    // #region agent log
    std::ofstream log_file_oid_set_available_symbols_exit(
        "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log", std::ios::app);
    if (log_file_oid_set_available_symbols_exit.is_open()) {
        log_file_oid_set_available_symbols_exit
            << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H17","location":"oid_bridge.cpp:1890","message":"oid_set_available_symbols_exit","data":{"available_vars_stl_size":)"
            << available_vars_stl.size() << R"(},"timestamp":)"
            << std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count()
            << "}\n";
        log_file_oid_set_available_symbols_exit.close();
    }
    // #endregion

    app->set_available_symbols(available_vars_stl);
}


void oid_run_event_loop(const AppHandler handler)
{
    // H15: Don't create PyGILRAII here - run_event_loop() doesn't use Python C
    // API, only calls C callback. Creating PyGILRAII repeatedly from event loop
    // thread causes crashes in LLDB's embedded Python. Only create it if we
    // need to raise an exception.
    const auto app = static_cast<OidBridge*>(handler);

    if (app == nullptr) [[unlikely]] {
        // Only create PyGILRAII when we actually need to raise an exception
        const auto py_gil_raii = PyGILRAII{};
        RAISE_PY_EXCEPTION(PyExc_RuntimeError,
                           "oid_run_event_loop received null application "
                           "handler");
        return;
    }

    // #region agent log
    std::ofstream log_file_oid_run_event_loop(
        "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log", std::ios::app);
    if (log_file_oid_run_event_loop.is_open()) {
        log_file_oid_run_event_loop
            << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H15","location":"oid_bridge.cpp:1793","message":"oid_run_event_loop_calling_run_event_loop","data":{"thread_id":)"
            << std::this_thread::get_id() << R"(},"timestamp":)"
            << std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count()
            << "}\n";
        log_file_oid_run_event_loop.close();
    }
    // #endregion

    app->run_event_loop();

    // #region agent log
    std::ofstream log_file_oid_run_event_loop_exit(
        "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log", std::ios::app);
    if (log_file_oid_run_event_loop_exit.is_open()) {
        log_file_oid_run_event_loop_exit
            << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H15","location":"oid_bridge.cpp:1815","message":"oid_run_event_loop_exit","data":{},"timestamp":)"
            << std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count()
            << "}\n";
        log_file_oid_run_event_loop_exit.close();
    }
    // #endregion
}


void oid_plot_buffer(AppHandler handler, PyObject* buffer_metadata)
{
    // #region agent log
    std::ofstream log_file_oid_plot_buffer_entry(
        "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log", std::ios::app);
    if (log_file_oid_plot_buffer_entry.is_open()) {
        log_file_oid_plot_buffer_entry
            << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H19","location":"oid_bridge.cpp:2194","message":"oid_plot_buffer_entry","data":{"thread_id":)"
            << std::this_thread::get_id() << R"(},"timestamp":)"
            << std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count()
            << "}\n";
        log_file_oid_plot_buffer_entry.close();
    }
    // #endregion

    const auto py_gil_raii = PyGILRAII{};

    // H19: In LLDB mode, avoid calling Python C API functions from event loop
    // thread. PyDict_Check, PyDict_GetItemString, PyUnicode_Check, etc. crash
    // when called from non-main threads in LLDB's embedded Python.
    if (PyGILRAII::is_lldb_mode()) {
        // Skip plotting in LLDB mode - this is called from the event loop
        // thread and Python C API calls are unsafe. #region agent log
        std::ofstream log_file_lldb_mode_early_return(
            "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
            std::ios::app);
        if (log_file_lldb_mode_early_return.is_open()) {
            log_file_lldb_mode_early_return
                << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H19","location":"oid_bridge.cpp:2205","message":"oid_plot_buffer_lldb_mode_early_return","data":{},"timestamp":)"
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count()
                << "}\n";
            log_file_lldb_mode_early_return.close();
        }
        // #endregion
        return;
    }

    const auto app = static_cast<OidBridge*>(handler);

    if (app == nullptr) [[unlikely]] {
        RAISE_PY_EXCEPTION(PyExc_RuntimeError,
                           "oid_plot_buffer received null application handler");
        return;
    }

    if (!PyDict_Check(buffer_metadata)) [[unlikely]] {
        RAISE_PY_EXCEPTION(PyExc_TypeError,
                           "Invalid object given to plot_buffer (was expecting"
                           " a dict).");
        return;
    }

    /*
     * Get required fields
     */
    const auto py_variable_name =
        PyDict_GetItemString(buffer_metadata, "variable_name");
    const auto py_display_name =
        PyDict_GetItemString(buffer_metadata, "display_name");
    const auto py_pointer  = PyDict_GetItemString(buffer_metadata, "pointer");
    const auto py_width    = PyDict_GetItemString(buffer_metadata, "width");
    const auto py_height   = PyDict_GetItemString(buffer_metadata, "height");
    const auto py_channels = PyDict_GetItemString(buffer_metadata, "channels");
    const auto py_type     = PyDict_GetItemString(buffer_metadata, "type");
    const auto py_row_stride =
        PyDict_GetItemString(buffer_metadata, "row_stride");
    const auto py_pixel_layout =
        PyDict_GetItemString(buffer_metadata, "pixel_layout");

    /*
     * Get optional fields
     */
    const auto py_transpose_buffer =
        PyDict_GetItemString(buffer_metadata, "transpose_buffer");
    auto transpose_buffer = false;
    if (py_transpose_buffer != nullptr) {
        CHECK_FIELD_TYPE(transpose_buffer, PyBool_Check, "transpose_buffer");
        transpose_buffer = PyObject_IsTrue(py_transpose_buffer);
    }

    /*
     * Check if expected fields were provided
     */
    CHECK_FIELD_PROVIDED(variable_name, "plot_buffer");
    CHECK_FIELD_PROVIDED(display_name, "plot_buffer");
    CHECK_FIELD_PROVIDED(pointer, "plot_buffer");
    CHECK_FIELD_PROVIDED(width, "plot_buffer");
    CHECK_FIELD_PROVIDED(height, "plot_buffer");
    CHECK_FIELD_PROVIDED(channels, "plot_buffer");
    CHECK_FIELD_PROVIDED(type, "plot_buffer");
    CHECK_FIELD_PROVIDED(row_stride, "plot_buffer");
    CHECK_FIELD_PROVIDED(pixel_layout, "plot_buffer");

    /*
     * Check if expected fields have the correct types
     */
    CHECK_FIELD_TYPE(variable_name, oid::check_py_string_type, "plot_buffer");
    CHECK_FIELD_TYPE(display_name, oid::check_py_string_type, "plot_buffer");
    CHECK_FIELD_TYPE(width, PY_INT_CHECK_FUNC, "plot_buffer");
    CHECK_FIELD_TYPE(height, PY_INT_CHECK_FUNC, "plot_buffer");
    CHECK_FIELD_TYPE(channels, PY_INT_CHECK_FUNC, "plot_buffer");
    CHECK_FIELD_TYPE(type, PY_INT_CHECK_FUNC, "plot_buffer");
    CHECK_FIELD_TYPE(row_stride, PY_INT_CHECK_FUNC, "plot_buffer");
    CHECK_FIELD_TYPE(pixel_layout, oid::check_py_string_type, "plot_buffer");

    // Retrieve pointer to buffer
    uint8_t* buff_ptr{nullptr};
    auto buff_size = std::size_t{0};
    if (PyMemoryView_Check(py_pointer) != 0) {
        oid::get_c_ptr_from_py_buffer(py_pointer, buff_ptr, buff_size);
    } else [[unlikely]] {
        RAISE_PY_EXCEPTION(PyExc_TypeError,
                           "Could not retrieve C pointer to provided buffer");
        return;
    }

    /*
     * Send buffer contents
     */
    auto variable_name_str = std::string{};
    auto display_name_str  = std::string{};
    auto pixel_layout_str  = std::string{};

    oid::copy_py_string(variable_name_str, py_variable_name);
    oid::copy_py_string(display_name_str, py_display_name);
    oid::copy_py_string(pixel_layout_str, py_pixel_layout);

    const auto buff_width    = static_cast<int>(oid::get_py_int(py_width));
    const auto buff_height   = static_cast<int>(oid::get_py_int(py_height));
    const auto buff_channels = static_cast<int>(oid::get_py_int(py_channels));
    const auto buff_stride   = static_cast<int>(oid::get_py_int(py_row_stride));

    const auto buff_type =
        static_cast<oid::BufferType>(oid::get_py_int(py_type));

    const auto buff_size_expected = std::size_t{
        static_cast<size_t>(buff_stride * buff_height * buff_channels) *
        oid::type_size(buff_type)};

    if (buff_ptr == nullptr) [[unlikely]] {
        RAISE_PY_EXCEPTION(
            PyExc_TypeError,
            "oid_plot_buffer received nullptr as buffer pointer");
        return;
    }

    // Create span from pointer+size for buffer storage
    const auto buff_span = std::span<const std::byte>{
        reinterpret_cast<const std::byte*>(buff_ptr), buff_size};
    if (buff_span.size() < buff_size_expected) [[unlikely]] {
        auto ss = std::stringstream{};
        ss << "oid_plot_buffer received shorter buffer then expected";
        ss << ". Variable name " << variable_name_str;
        ss << ". Expected " << buff_size_expected << "bytes";
        ss << ". Received " << buff_span.size() << "bytes";
        RAISE_PY_EXCEPTION(PyExc_TypeError, ss.str().c_str());
        return;
    }

    const PlotBufferParams params{.variable_name_str = variable_name_str,
                                  .display_name_str  = display_name_str,
                                  .pixel_layout_str  = pixel_layout_str,
                                  .transpose_buffer  = transpose_buffer,
                                  .buff_width        = buff_width,
                                  .buff_height       = buff_height,
                                  .buff_channels     = buff_channels,
                                  .buff_stride       = buff_stride,
                                  .buff_type         = buff_type,
                                  .buffer            = buff_span};
    app->plot_buffer(params);
}


void oid_plot_buffer_safe(AppHandler handler,
                          const char* variable_name,
                          const char* display_name,
                          const void* buffer_ptr,
                          size_t buffer_size,
                          int width,
                          int height,
                          int channels,
                          int type,
                          int row_stride,
                          const char* pixel_layout,
                          int transpose_buffer)
{
    // #region agent log
    std::ofstream log_file_oid_plot_buffer_safe_entry(
        "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log", std::ios::app);
    if (log_file_oid_plot_buffer_safe_entry.is_open()) {
        log_file_oid_plot_buffer_safe_entry
            << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H19","location":"oid_bridge.cpp:2370","message":"oid_plot_buffer_safe_entry","data":{"thread_id":)"
            << std::this_thread::get_id() << R"(,"variable_name":)"
            << (variable_name ? "not_null" : "null") << R"(},"timestamp":)"
            << std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count()
            << "}\n";
        log_file_oid_plot_buffer_safe_entry.close();
    }
    // #endregion

    const auto app = static_cast<OidBridge*>(handler);

    if (app == nullptr) [[unlikely]] {
        return;
    }

    if (buffer_ptr == nullptr) [[unlikely]] {
        return;
    }

    // Create span from pointer+size for buffer storage
    const auto buff_span = std::span<const std::byte>{
        reinterpret_cast<const std::byte*>(buffer_ptr), buffer_size};

    const auto buff_size_expected =
        std::size_t{static_cast<size_t>(row_stride * height * channels) *
                    oid::type_size(static_cast<oid::BufferType>(type))};

    if (buff_span.size() < buff_size_expected) [[unlikely]] {
        return;
    }

    const PlotBufferParams params{
        .variable_name_str =
            variable_name ? std::string{variable_name} : std::string{},
        .display_name_str =
            display_name ? std::string{display_name} : std::string{},
        .pixel_layout_str =
            pixel_layout ? std::string{pixel_layout} : std::string{},
        .transpose_buffer = transpose_buffer != 0,
        .buff_width       = width,
        .buff_height      = height,
        .buff_channels    = channels,
        .buff_stride      = row_stride,
        .buff_type        = static_cast<oid::BufferType>(type),
        .buffer           = buff_span};
    app->plot_buffer(params);

    // #region agent log
    std::ofstream log_file_oid_plot_buffer_safe_exit(
        "/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log", std::ios::app);
    if (log_file_oid_plot_buffer_safe_exit.is_open()) {
        log_file_oid_plot_buffer_safe_exit
            << R"({"sessionId":"debug-session","runId":"run1","hypothesisId":"H19","location":"oid_bridge.cpp:2420","message":"oid_plot_buffer_safe_exit","data":{},"timestamp":)"
            << std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count()
            << "}\n";
        log_file_oid_plot_buffer_safe_exit.close();
    }
    // #endregion
}
