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

#include <bit>
#include <chrono>
#include <deque>
#include <filesystem>
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
#include "library_path.h"
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
        // H16: Cache LLDB mode detection to avoid calling unsafe Python C API
        // functions (PyGILState_GetThisThreadState, Py_IsInitialized) from
        // event loop thread. These calls crash in LLDB's embedded Python when
        // called from non-main threads, even when we skip GIL management.
        static bool lldb_mode_cached = false;

        PyThreadState* tstate_before = nullptr;
        bool py_init_before          = false;

        if (!lldb_mode_cached) {
            // First call: detect LLDB mode from main thread (safe)
            tstate_before    = PyGILState_GetThisThreadState();
            py_init_before   = Py_IsInitialized() != 0;
            s_is_lldb_mode   = (!tstate_before && !py_init_before);
            lldb_mode_cached = true;
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
        } else {
            // Normal case: try PyGILState_Ensure()
            _py_gil_state = PyGILState_Ensure();
            _has_gil      = false; // We acquired it, so we need to release it
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
    }

    bool start()
    {

        // Initialize server

        if (!server_.listen(QHostAddress::Any)) {
            std::cerr << "[OpenImageDebugger] Could not start TCP server"
                      << std::endl;
            return false;
        }

        // H14: Fallback to detect library path when oid_path_ is empty (LLDB
        // Python bug - __file__ not set)
        std::string oid_path_to_use = this->oid_path_;
        if (oid_path_to_use.empty()) {
            const std::filesystem::path library_path =
                oid::get_current_library_path();

            // Use std::filesystem for cross-platform path operations
            if (!library_path.empty()) {
                // Try canonical() first, fallback to absolute() if it fails
                bool path_resolved = false;
                try {
                    const auto canonical_path =
                        std::filesystem::canonical(library_path);
                    const auto lib_dir = canonical_path.parent_path();
                    oid_path_to_use    = lib_dir.string();
                    path_resolved      = true;
                } catch (const std::filesystem::filesystem_error& ex) {
                    // canonical() failed (e.g., path doesn't exist) - try
                    // absolute() as fallback
                    std::cerr << "[OpenImageDebugger] Failed to get canonical "
                                 "library path: "
                              << ex.what() << std::endl;
                }

                if (!path_resolved) {
                    try {
                        const auto absolute_path =
                            std::filesystem::absolute(library_path);
                        const auto lib_dir = absolute_path.parent_path();
                        oid_path_to_use    = lib_dir.string();
                    } catch (const std::filesystem::filesystem_error& ex) {
                        // Both canonical() and absolute() failed - continue
                        // with empty oid_path_to_use (will use fallback or fail
                        // later if needed)
                        const auto error_msg = ex.what();
                        std::cerr
                            << "[OpenImageDebugger] Failed to get absolute "
                               "library path: "
                            << error_msg << std::endl;
                    }
                }
            }
        }

        const auto windowBinaryPath = oid_path_to_use + "/oidwindow";
        const auto portStdString    = std::to_string(server_.serverPort());

        std::vector<std::string> command{
            windowBinaryPath, "-style", "fusion", "-p", portStdString};

        ui_proc_.start(command);

        ui_proc_.waitForStart();

        wait_for_client();

        return client_ != nullptr;
    }

    void set_path(const std::string_view& oid_path)
    {

        oid_path_ = oid_path;
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

        try_read_incoming_messages(static_cast<int>(1000.0 / 5.0));

        auto plot_request_message = std::make_unique<UiMessage>();
        while ((plot_request_message = try_get_stored_message(
                    oid::MessageType::PlotBufferRequest)) != nullptr) {

            const PlotBufferRequestMessage* msg =
                dynamic_cast<PlotBufferRequestMessage*>(
                    plot_request_message.get());
            if (plot_callback_) {

                plot_callback_(msg->buffer_name.c_str());
            }
        }
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

        // Check if client is connected before sending
        if (client_.isNull() ||
            client_->state() != QAbstractSocket::ConnectedState) {
            std::cerr << "[OpenImageDebugger] Cannot plot buffer: client "
                         "socket is null or not connected"
                      << std::endl;
            return;
        }

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

        if (client_.isNull()) {

            if (!server_.waitForNewConnection(10000)) {

                std::cerr << "[OpenImageDebugger] No clients connected to "
                             "OpenImageDebugger server"
                          << std::endl;
            } else {
            }
            client_ = QPointer<QTcpSocket>(server_.nextPendingConnection());
        }
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

    const auto py_gil_raii = PyGILRAII{};

    if (optional_parameters != nullptr && !PyDict_Check(optional_parameters))
        [[unlikely]] {

        RAISE_PY_EXCEPTION(
            PyExc_TypeError,
            "Invalid second parameter given to oid_initialize (was expecting"
            " a dict).");
        return nullptr;
    }

    /*
     * Get optional fields
     */

    PyObject* py_oid_path = nullptr;
    if (optional_parameters != nullptr) {

        // H1: Test PyDict_Size BEFORE logging to avoid abort during log
        // formatting H2-H5: Test if dictionary operations require proper thread
        // state registration

        // Test if we can safely call Python APIs by trying a lightweight
        // operation first PyDict_Size() requires GIL and is safer than
        // PyDict_GetItemString
        Py_ssize_t dict_size = PyDict_Size(optional_parameters);

        // H7: Use PyDict_Next() to iterate through dictionary directly - this
        // gives us existing key/value pairs without creating new objects.
        // PyDict_Keys() creates a new list, which might abort. PyDict_Next()
        // just iterates through existing pairs.

        // Iterate through dictionary using PyDict_Next - this avoids creating
        // new objects and uses existing key/value pairs
        Py_ssize_t pos  = 0;
        PyObject* key   = nullptr;
        PyObject* value = nullptr;
        PyObject* fallback_value =
            nullptr; // Store value if key is empty (LLDB Python bug workaround)

        while (PyDict_Next(optional_parameters, &pos, &key, &value)) {

            // key and value are borrowed references from the dictionary
            if (key != nullptr && PyUnicode_Check(key)) {

                // Try to get UTF-8 string for comparison
                // Note: PyUnicode_AsUTF8 might abort, but we'll try it
                const char* key_str = PyUnicode_AsUTF8(key);

                if (key_str != nullptr && strcmp(key_str, "oid_path") == 0) {
                    // Found the key - value already contains the result
                    py_oid_path = value; // Borrowed reference

                    break;
                } else if (key_str != nullptr && strlen(key_str) == 0 &&
                           value != nullptr) {
                    // H13: LLDB Python bug workaround - key is empty string but
                    // value might be the path we need Store it as fallback if
                    // dictionary has only one entry

                    fallback_value = value; // Borrowed reference
                }
            }
        }

        // H13: If we didn't find "oid_path" key but found an empty key with a
        // value and dictionary has only one entry, use the fallback value (LLDB
        // Python bug workaround)
        if (py_oid_path == nullptr && fallback_value != nullptr &&
            dict_size == 1) {
            py_oid_path = fallback_value; // Borrowed reference
        }

        // If we didn't find the key via iteration, the key doesn't exist in
        // the dictionary. Don't fall back to PyDict_GetItemString as it aborts.
        // Just leave py_oid_path as nullptr - this is a valid case (optional
        // parameter).
        if (py_oid_path == nullptr) {

            // py_oid_path remains nullptr - this is fine, it's an optional
            // parameter
        }

    } else {
    }

    auto app = std::make_unique<OidBridge>(std::move(plot_callback));

    if (py_oid_path) {

        auto oid_path_str = std::string{};
        oid::copy_py_string(oid_path_str, py_oid_path);

        app->set_path(oid_path_str);
    } else {
    }

    return app;
}
} // namespace

// NOSONAR: C API requires function pointer (extern "C")
AppHandler oid_initialize(int (*plot_callback)(const char*), // NOSONAR
                          PyObject* optional_parameters)
{

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

        return app ? app.release() : nullptr;
    } catch (const std::exception&) {

        return nullptr;
    } catch (...) {

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

    const auto py_gil_raii = PyGILRAII{};

    const auto app = static_cast<OidBridge*>(handler);

    if (app == nullptr) [[unlikely]] {
        RAISE_PY_EXCEPTION(PyExc_RuntimeError,
                           "oid_exec received null application handler");
        return;
    }

    app->start();
}

int oid_is_window_ready(const AppHandler handler)
{

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

        Py_INCREF(Py_None);

        return Py_None;
    }

    const auto py_observed_symbols =
        PyList_New(static_cast<Py_ssize_t>(observed_symbols.size()));

    const auto observed_symbols_sentinel =
        static_cast<int>(observed_symbols.size());
    for (int i = 0; i < observed_symbols_sentinel; ++i) {
        const auto& symbol_name = observed_symbols[i];

        const auto py_symbol_name = PyBytes_FromString(symbol_name.c_str());

        if (py_symbol_name == nullptr) [[unlikely]] {
            Py_DECREF(py_observed_symbols);
            return nullptr;
        }

        PyList_SetItem(py_observed_symbols, i, py_symbol_name);
    }

    return py_observed_symbols;
}

void oid_set_available_symbols(const AppHandler handler,
                               PyObject* available_vars)
{

    const auto py_gil_raii = PyGILRAII{};

    // H17: In LLDB mode, avoid calling Python C API functions from event loop
    // thread. PyList_Check, PyList_Size, PyList_GetItem crash when called from
    // non-main threads in LLDB's embedded Python.
    if (PyGILRAII::is_lldb_mode()) {
        // Skip setting available symbols in LLDB mode - this is called from the
        // event loop thread and Python C API calls are unsafe.

        return;
    }

    assert(PyList_Check(available_vars));

    const auto app = static_cast<OidBridge*>(handler);

    if (app == nullptr) [[unlikely]] {
        RAISE_PY_EXCEPTION(PyExc_RuntimeError,
                           "oid_set_available_symbols received null "
                           "application handler");
        return;
    }

    auto available_vars_stl = std::deque<std::string>{};
    const auto list_size    = PyList_Size(available_vars);

    for (Py_ssize_t pos = 0; pos < list_size; ++pos) {
        auto var_name_str = std::string{};

        const auto listItem = PyList_GetItem(available_vars, pos);

        oid::copy_py_string(var_name_str, listItem);
        available_vars_stl.push_back(var_name_str);
    }

    app->set_available_symbols(available_vars_stl);
}

void oid_set_available_symbols_safe(const AppHandler handler,
                                    const char* const* available_vars,
                                    size_t count)
{
    // H17: Safe version for LLDB mode - no Python C API calls
    // This function can be called from any thread without GIL issues

    const auto app = static_cast<OidBridge*>(handler);

    if (app == nullptr) [[unlikely]] {
        return;
    }

    auto available_vars_stl = std::deque<std::string>{};

    for (size_t pos = 0; pos < count; ++pos) {
        if (available_vars[pos] != nullptr) {
            available_vars_stl.emplace_back(available_vars[pos]);
        }
    }

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

    app->run_event_loop();
}

void oid_plot_buffer(AppHandler handler, PyObject* buffer_metadata)
{

    const auto py_gil_raii = PyGILRAII{};

    // H19: In LLDB mode, avoid calling Python C API functions from event loop
    // thread. PyDict_Check, PyDict_GetItemString, PyUnicode_Check, etc. crash
    // when called from non-main threads in LLDB's embedded Python.
    if (PyGILRAII::is_lldb_mode()) {
        // Skip plotting in LLDB mode - this is called from the event loop
        // thread and Python C API calls are unsafe.
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
}
