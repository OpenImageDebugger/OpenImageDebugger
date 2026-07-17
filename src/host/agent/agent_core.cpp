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

#include "host/agent/agent_core.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <functional>
#include <limits>
#include <numbers>
#include <optional>
#include <string_view>
#include <unordered_map>

#include "ipc/raw_data_decode.h"

namespace oid::host::agent {

namespace {

constexpr std::size_t DEFAULT_MAX_BYTES = 256ULL * 1024 * 1024;
constexpr double PI = std::numbers::pi;

// Constant-time token compare: accumulates the XOR of every byte pair over
// the longer of the two strings (out-of-range reads substitute 0) into a
// volatile accumulator so neither the branch nor the loop can be optimized
// into a short-circuiting compare.
bool constant_time_equal(std::string_view a, std::string_view b) {
    const std::size_t max_len = (std::max)(a.size(), b.size());
    volatile unsigned char diff = // NOSONAR
        static_cast<unsigned char>(a.size() != b.size());
    for (std::size_t i = 0; i < max_len; ++i) {
        const unsigned char ca =
            i < a.size() ? static_cast<unsigned char>(a[i]) : 0;
        const unsigned char cb =
            i < b.size() ? static_cast<unsigned char>(b[i]) : 0;
        diff |= static_cast<unsigned char>(ca ^ cb); // NOSONAR
    }
    return diff == 0;
}

// Transparent hash so the method-dispatch map can be looked up by
// string_view without first constructing a std::string key (S6045).
struct TransparentStringHash {
    using is_transparent = void;
    std::size_t operator()(std::string_view sv) const noexcept {
        return std::hash<std::string_view>{}(sv);
    }
};

Reply make_error(const char* code, std::string message) {
    nlohmann::json body;
    body["error"]["code"] = code;
    body["error"]["message"] = std::move(message);
    return Reply{std::move(body), {}};
}

// A well-formed image buffer has positive width/height/channels and a row
// stride (step, in pixels) at least as wide as the row. A non-positive
// dimension or a step < width is a malformed/inconsistent BufferRecord whose
// declared byte size can't be trusted -- and which oid-mcp's decode_buffer (it
// needs width > 0 and stride >= width) would reject client-side anyway,
// producing a cryptic failure instead of a clean endpoint rejection. Returns an
// error Reply to send BEFORE read_pixels (so a malformed record can't force a
// full backing-vector copy in read_pixels()), or nullopt when the dimensions
// are sound. (width > 0 && step >= width implies step > 0, so the caller's
// declared-bytes fold still divides by a positive step.)
std::optional<Reply> validate_buffer_dimensions(const BufferInfo& info,
                                                const std::string& symbol) {
    if (info.width <= 0 || info.step < info.width || info.height <= 0 ||
        info.channels <= 0) {
        return make_error(AgentCore::ERR_INTERNAL,
                          "buffer has invalid dimensions: " + symbol);
    }
    return std::nullopt;
}

// Builds the get_view body from the model's current state: this is both
// get_view's own reply and the read-back set_view returns after applying a
// patch.
nlohmann::json get_view_body(ViewModel& model) {
    const auto name = model.selected_name();
    const std::optional<ViewState> state =
        name ? model.view_of(*name) : std::nullopt;

    nlohmann::json body;
    if (model.buffer_count() == 0 || !name || !state) {
        body["buffer"] = nullptr;
        body["center"] = nullptr;
        body["zoom"] = nullptr;
        body["rotation_deg"] = nullptr;
        body["channel"] = nullptr;
    } else {
        body["buffer"] = *state->buffer;
        body["center"] =
            nlohmann::json::array({*state->center_x, *state->center_y});
        body["zoom"] = *state->zoom;
        body["rotation_deg"] = *state->rotation_deg;
        body["channel"] = state->channel;
    }
    body["auto_contrast"] = model.auto_contrast();
    const auto [viewport_w, viewport_h] = model.viewport_size();
    body["viewport"] = nlohmann::json::array({viewport_w, viewport_h});
    return body;
}

// Validates j as a JSON number and rewrites it into a finite double. Returns
// false (leaving out unchanged) for non-numbers and NaN/Inf alike.
bool as_finite_number(const nlohmann::json& j, double& out) {
    if (!j.is_number()) {
        return false;
    }
    out = j.get<double>();
    if (!std::isfinite(out)) {
        return false;
    }
    // Reject magnitudes that would overflow to +/-inf when the native adapter
    // narrows to float (center/zoom/rotation feed Camera as float); a
    // non-finite float installs bad state and yields NaN read-back.
    return std::abs(out) <=
           static_cast<double>((std::numeric_limits<float>::max)());
}

// Parses a channel selector as either a JSON integer or an integer-valued
// string ("0", "1", ...). std::nullopt means neither -- the caller treats
// that as invalid.
std::optional<long long> parse_channel_index(const nlohmann::json& j) {
    if (j.is_number_integer()) {
        return j.get<long long>();
    }
    if (j.is_string()) {
        const std::string s = j.get<std::string>();
        if (!s.empty()) {
            char* end = nullptr;
            const long long value = std::strtoll(s.c_str(), &end, 10);
            if (end == s.c_str() + s.size()) {
                return value;
            }
        }
    }
    return std::nullopt;
}

// Parsed "channel" field: either the "all" sentinel or a validated 0-based
// index into the (capped-at-3) channel range.
struct ChannelSelection {
    bool all = false;
    int index = 0;
};

// Parsed, validated fields of a set_view request. They are applied together
// (apply_view) only after every field validates, so a partially-invalid
// request never mutates the model.
struct ViewUpdate {
    std::optional<std::string> buffer_name;
    std::optional<std::string> target;
    std::optional<ChannelSelection> channel;
    std::optional<double> zoom;
    std::optional<std::pair<double, double>> center;
    std::optional<double> rotation_deg;
    std::optional<bool> auto_contrast;
};

// Each set_view field validator below mirrors the same shape: if the field
// is absent from the request, return std::nullopt and leave `out` untouched;
// if present but invalid, return the error Reply (out is left untouched);
// if present and valid, populate `out` and return std::nullopt. This keeps
// handle_set_view's validation phase a flat sequence of
// "validate -> return error on failure" calls.

std::optional<Reply> parse_buffer_name(const nlohmann::json& request,
                                       ViewModel& model,
                                       std::optional<std::string>& out) {
    if (!request.contains("buffer")) {
        return std::nullopt;
    }
    if (!request.at("buffer").is_string()) {
        return make_error(AgentCore::ERR_BAD_PARAMS, "buffer must be a string");
    }
    const std::string name = request.at("buffer").get<std::string>();
    if (!model.buffer_named(name)) {
        return make_error(AgentCore::ERR_UNKNOWN_BUFFER,
                          "no such buffer: " + name);
    }
    out = name;
    return std::nullopt;
}

// Resolves the buffer that per-buffer fields (center/zoom/rotation_deg/
// channel) apply to: the explicit `buffer` name if given, else whichever
// buffer is currently selected. Errors only if a per-buffer field is present
// but no buffer can be resolved.
std::optional<Reply>
resolve_target(const nlohmann::json& request,
               ViewModel& model,
               const std::optional<std::string>& buffer_name,
               std::optional<std::string>& out) {
    const bool has_per_buffer_field =
        request.contains("center") || request.contains("zoom") ||
        request.contains("rotation_deg") || request.contains("channel");
    out = buffer_name ? buffer_name : model.selected_name();
    if (!out && has_per_buffer_field) {
        return make_error(AgentCore::ERR_BAD_PARAMS,
                          "no buffer to apply view to");
    }
    return std::nullopt;
}

std::optional<Reply> parse_channel(const nlohmann::json& request,
                                   ViewModel& model,
                                   const std::optional<std::string>& target,
                                   std::optional<ChannelSelection>& out) {
    if (!request.contains("channel")) {
        return std::nullopt;
    }
    const auto& j = request.at("channel");
    if (j.is_string() && j.get<std::string>() == "all") {
        out = ChannelSelection{true, 0};
        return std::nullopt;
    }
    const auto info = model.buffer_named(*target);
    const int max_channel = info ? (std::min)(info->channels, 3) - 1 : -1;
    const auto parsed = parse_channel_index(j);
    if (!parsed || *parsed < 0 || *parsed > max_channel) {
        return make_error(
            AgentCore::ERR_BAD_PARAMS,
            std::format("channel must be \"all\" or an int in 0..{}",
                        (std::max)(max_channel, 0)));
    }
    out = ChannelSelection{false, static_cast<int>(*parsed)};
    return std::nullopt;
}

std::optional<Reply> parse_zoom(const nlohmann::json& request,
                                std::optional<double>& out) {
    if (!request.contains("zoom")) {
        return std::nullopt;
    }
    double value = 0.0;
    // Reject non-positive zoom, and positive values so tiny they narrow to a
    // subnormal/zero float in Camera::compute_zoom() (pow), where
    // 1/compute_zoom() would then blow up to +inf and corrupt the view -- the
    // lower-bound dual of as_finite_number's float-overflow guard.
    if (!as_finite_number(request.at("zoom"), value) ||
        value < static_cast<double>((std::numeric_limits<float>::min)())) {
        return make_error(AgentCore::ERR_BAD_PARAMS,
                          "zoom must be finite and > 0");
    }
    // Reject a zoom whose engine power (log(zoom)/log(ZOOM_FACTOR)) would
    // overflow the float pow() in Camera::compute_zoom() -- pow(1.1f, ~931)
    // exceeds FLT_MAX and would install a non-finite scale.
    if (constexpr double MAX_ZOOM_POWER = 930.0;
        std::log(value) / std::log(ViewModel::ZOOM_FACTOR) > MAX_ZOOM_POWER) {
        return make_error(AgentCore::ERR_BAD_PARAMS, "zoom is too large");
    }
    out = value;
    return std::nullopt;
}

std::optional<Reply>
parse_center(const nlohmann::json& request,
             std::optional<std::pair<double, double>>& out) {
    if (!request.contains("center")) {
        return std::nullopt;
    }
    const auto& c = request.at("center");
    double x = 0.0;
    double y = 0.0;
    if (!c.is_array() || c.size() != 2 || !as_finite_number(c[0], x) ||
        !as_finite_number(c[1], y)) {
        return make_error(AgentCore::ERR_BAD_PARAMS,
                          "center must be a 2-array of finite numbers");
    }
    out = std::make_pair(x, y);
    return std::nullopt;
}

std::optional<Reply> parse_rotation(const nlohmann::json& request,
                                    std::optional<double>& out) {
    if (!request.contains("rotation_deg")) {
        return std::nullopt;
    }
    double value = 0.0;
    if (!as_finite_number(request.at("rotation_deg"), value)) {
        return make_error(AgentCore::ERR_BAD_PARAMS,
                          "rotation_deg must be finite");
    }
    // Normalize to the [0, 360) range get_view reports, so the accepted value
    // matches the read-back contract (e.g. set_view(450) and set_view(90) both
    // read back as 90) rather than only agreeing modulo 360.
    value = std::fmod(value, 360.0);
    if (value < 0.0) {
        value += 360.0;
    }
    out = value;
    return std::nullopt;
}

std::optional<Reply> parse_auto_contrast(const nlohmann::json& request,
                                         std::optional<bool>& out) {
    if (!request.contains("auto_contrast")) {
        return std::nullopt;
    }
    if (!request.at("auto_contrast").is_boolean()) {
        return make_error(AgentCore::ERR_BAD_PARAMS,
                          "auto_contrast must be a bool");
    }
    out = request.at("auto_contrast").get<bool>();
    return std::nullopt;
}

// Applies an already-validated ViewUpdate to the model. Returns true iff
// every mutator invoked below succeeded. Every field present in `u` is
// still applied even once a prior mutator fails (via &=, not short-circuit
// &&) so a mutator failure never leaves the model in a different partial
// state than success would -- it only changes what handle_set_view reports
// back to the caller. Split out of handle_set_view so that function's
// complexity stays low.
bool apply_view(ViewModel& model, const ViewUpdate& u) {
    bool ok = true;
    if (u.buffer_name.has_value()) {
        ok &= model.select(*u.buffer_name);
    }
    if (u.auto_contrast.has_value()) {
        model.set_auto_contrast(*u.auto_contrast);
    }
    // Apply the geometry fields most-coupled-first: rotation sets the buffer
    // pose that the center read-back is expressed against, and zoom (which is
    // center-preserving in the engine) before center, so center is applied last
    // and an absolute set_view lands the requested center regardless of which
    // other fields ride in the same patch.
    if (u.rotation_deg.has_value()) {
        ok &= model.set_rotation_rad(*u.target, *u.rotation_deg * PI / 180.0);
    }
    if (u.zoom.has_value()) {
        ok &= model.set_zoom_power(
            *u.target, std::log(*u.zoom) / std::log(ViewModel::ZOOM_FACTOR));
    }
    if (u.center.has_value()) {
        ok &= model.set_center(*u.target, u.center->first, u.center->second);
    }
    if (u.channel) {
        ok &= model.set_channel(*u.target,
                                u.channel->all ? -1 : 1,
                                u.channel->all ? 0 : u.channel->index);
    }
    return ok;
}

} // namespace

AgentCore::AgentCore(ViewModel& model,
                     std::string token,
                     long pid,
                     std::string session_kind)
    : model_(model), token_(std::move(token)), pid_(pid),
      session_kind_(std::move(session_kind)) {}

Reply AgentCore::handle(const nlohmann::json& request, bool& authed) {
    if (!request.is_object() || !request.contains("method") ||
        !request.at("method").is_string()) {
        return make_error(ERR_UNKNOWN_METHOD,
                          "malformed request (missing method)");
    }
    const std::string method = request.at("method").get<std::string>();

    if (method == "hello") {
        return handle_hello(request, authed);
    }
    if (!authed) {
        return make_error(ERR_BAD_TOKEN, "first request must be hello");
    }

    static const std::unordered_map<std::string,
                                    Handler,
                                    TransparentStringHash,
                                    std::equal_to<>>
        kHandlers = {
            {"ping", &AgentCore::handle_ping},
            {"list_buffers", &AgentCore::handle_list_buffers},
            {"get_buffer", &AgentCore::handle_get_buffer},
            {"get_view", &AgentCore::handle_get_view},
            {"set_view", &AgentCore::handle_set_view},
        };
    const auto it = kHandlers.find(method);
    if (it == kHandlers.end()) {
        return make_error(ERR_UNKNOWN_METHOD, "unknown method: " + method);
    }
    return (this->*(it->second))(request);
}

Reply AgentCore::handle_hello(const nlohmann::json& request, bool& authed) {
    std::string supplied;
    if (request.contains("token") && request.at("token").is_string()) {
        supplied = request.at("token").get<std::string>();
    }
    // Reject a wrong-length token before the constant-time compare: the token
    // length (64 hex chars) is public, so this leaks nothing, and it bounds the
    // per-hello work to the real token size. Otherwise a client could send a
    // ~1 MiB token and force that many iterations on the render thread, where
    // drain() runs.
    if (supplied.size() != token_.size() ||
        !constant_time_equal(supplied, token_)) {
        // Do not clear `authed`: a bad token only ever fails to *grant* auth,
        // it must never *revoke* an already-authenticated session. Otherwise a
        // stray or malformed re-hello would drop a live connection back to the
        // pre-auth state, where the (long-expired) absolute handshake deadline
        // then tears it down. A first, failed hello leaves authed as it was
        // (false).
        return make_error(ERR_BAD_TOKEN, "bad or missing token");
    }
    authed = true;
    nlohmann::json body;
    body["version"] = 1;
    body["kind"] = session_kind_;
    body["pid"] = pid_;
    return Reply{std::move(body), {}};
}

Reply AgentCore::handle_ping(const nlohmann::json& /*request*/) const {
    nlohmann::json body;
    body["ok"] = true;
    return Reply{std::move(body), {}};
}

Reply AgentCore::handle_list_buffers(const nlohmann::json& /*request*/) const {
    nlohmann::json buffers = nlohmann::json::array();
    const std::size_t count = model_.buffer_count();
    for (std::size_t i = 0; i < count; ++i) {
        const auto info = model_.buffer_at(i);
        if (!info) {
            continue;
        }
        nlohmann::json entry;
        entry["name"] = info->name;
        entry["display_name"] = info->display_name;
        entry["width"] = info->width;
        entry["height"] = info->height;
        entry["channels"] = info->channels;
        entry["type"] = info->type;
        entry["pixel_layout"] = info->pixel_layout;
        entry["transpose_buffer"] = info->transpose;
        buffers.push_back(std::move(entry));
    }
    nlohmann::json body;
    body["buffers"] = std::move(buffers);
    return Reply{std::move(body), {}};
}

Reply AgentCore::handle_get_buffer(const nlohmann::json& request) const {
    if (!request.contains("symbol") || !request.at("symbol").is_string()) {
        return make_error(ERR_BAD_PARAMS, "symbol is required");
    }
    const std::string symbol = request.at("symbol").get<std::string>();
    const auto info = model_.buffer_named(symbol);
    if (!info) {
        return make_error(ERR_UNKNOWN_BUFFER, "no such buffer: " + symbol);
    }

    // Always enforce the server-side cap: a client may lower it, but a
    // missing/invalid max_bytes does NOT disable the guard.
    std::size_t max_bytes = DEFAULT_MAX_BYTES;
    if (request.contains("max_bytes") &&
        request.at("max_bytes").is_number_integer()) {
        const auto requested = request.at("max_bytes").get<std::int64_t>();
        if (requested > 0 &&
            static_cast<std::uint64_t>(requested) < DEFAULT_MAX_BYTES) {
            max_bytes = static_cast<std::size_t>(requested);
        }
    }

    // Reject a malformed/inconsistent shape (see validate_buffer_dimensions)
    // BEFORE read_pixels, so it can't skip the cap below and force a full
    // backing-vector copy -- defeating the reject-before-copy protection.
    if (auto dim_error = validate_buffer_dimensions(*info, symbol)) {
        return *dim_error;
    }

    // Reject an oversized buffer BEFORE allocating/copying its pixel bytes.
    // BufferInfo::step is a row's width in *pixels* (GL_UNPACK_ROW_LENGTH;
    // see Buffer::configure), not bytes, and each pixel itself spans
    // `channels` elements of `type`'s width. So the declared byte size is
    // step * channels * type_size(type) * height, not step alone -- using step
    // alone under-rejects any wide-element or multi-channel buffer. Fold the
    // product one factor at a time and bail the moment it would exceed
    // max_bytes: this rejects the oversized buffer AND keeps pathological
    // (e.g. IPC-malformed) dimensions from overflowing the uint64 product and
    // wrapping past the cap on any target (notably 32-bit wasm32). Every factor
    // is >= 1 -- step/channels/height are checked > 0 above and type_size is
    // 1..8 -- so max_bytes / factor never divides by zero.
    std::uint64_t declared_bytes = 1;
    for (const std::uint64_t factor :
         {static_cast<std::uint64_t>(info->step),
          static_cast<std::uint64_t>(info->channels),
          static_cast<std::uint64_t>(
              oid::type_size(static_cast<oid::BufferType>(info->type))),
          static_cast<std::uint64_t>(info->height)}) {
        if (declared_bytes > max_bytes / factor) {
            return make_error(
                ERR_TOO_LARGE,
                std::format("buffer exceeds max_bytes {}", max_bytes));
        }
        declared_bytes *= factor;
    }

    std::vector<std::byte> bytes;
    if (!model_.read_pixels(symbol, bytes)) {
        return make_error(ERR_INTERNAL, "failed to read buffer: " + symbol);
    }
    // Defense in depth: re-check against the bytes actually read, in case
    // they disagree with the declared shape.
    if (bytes.size() > max_bytes) {
        return make_error(ERR_TOO_LARGE,
                          std::format("buffer of {} bytes exceeds max_bytes {}",
                                      bytes.size(),
                                      max_bytes));
    }

    nlohmann::json body;
    body["width"] = info->width;
    body["height"] = info->height;
    body["channels"] = info->channels;
    body["step"] = info->step;
    body["type"] = info->type;
    body["pixel_layout"] = info->pixel_layout;
    body["transpose_buffer"] = info->transpose;
    body["display_name"] = info->display_name;
    return Reply{std::move(body), std::move(bytes)};
}

Reply AgentCore::handle_get_view(const nlohmann::json& /*request*/) const {
    return Reply{get_view_body(model_), {}};
}

Reply AgentCore::handle_set_view(const nlohmann::json& request) const {
    // Validation phase: every field is parsed and validated into `update`;
    // nothing here mutates the model. apply_view() below touches the model,
    // and only once every field has validated (set_view is atomic).
    ViewUpdate update;
    if (auto err = parse_buffer_name(request, model_, update.buffer_name)) {
        return *err;
    }
    if (auto err = resolve_target(
            request, model_, update.buffer_name, update.target)) {
        return *err;
    }
    if (auto err =
            parse_channel(request, model_, update.target, update.channel)) {
        return *err;
    }
    if (auto err = parse_zoom(request, update.zoom)) {
        return *err;
    }
    if (auto err = parse_center(request, update.center)) {
        return *err;
    }
    if (auto err = parse_rotation(request, update.rotation_deg)) {
        return *err;
    }
    if (auto err = parse_auto_contrast(request, update.auto_contrast)) {
        return *err;
    }
    // Every field above is pre-validated against the model, so a mutator
    // failing here means the model is internally inconsistent with what
    // validation observed, not a bad request.
    if (!apply_view(model_, update)) {
        return make_error(ERR_INTERNAL, "failed to apply view");
    }
    return Reply{get_view_body(model_), {}};
}

} // namespace oid::host::agent
