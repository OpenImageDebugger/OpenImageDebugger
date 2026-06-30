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

#include "message_handler.h"

#include <bit>
#include <array>
#include <iostream>
#include <memory>
#include <ranges>
#include <span>
#include <sstream>

#include <QSizeF>

#include "ipc/message_exchange.h"
#include "ipc/raw_data_decode.h"
#include "platform/gl_dialect.h"
#include "platform/render_scheduler.h"
#include "ui/gl_canvas.h"
#include "ui/main_window/main_window.h"
#include "ui/main_window/settings_applier.h"
#include "ui/messaging/session_state_codec.h"
#include "visualization/components/buffer.h"
#include "visualization/stage.h"

namespace oid {

MessageHandler::MessageHandler(Dependencies deps, QObject* parent)
    : QObject{parent}, deps_{std::move(deps)} {}

void MessageHandler::decode_set_available_symbols() const {
    const auto lock = std::unique_lock{deps_.ui_mutex};
    auto message_decoder = MessageDecoder{deps_.transport};
    message_decoder.read<QStringList, QString>(
        deps_.buffer_data.available_vars);

    for (const auto& symbol_value : deps_.buffer_data.available_vars) {
        const auto symbol_std_str = symbol_value.toStdString();
        if (deps_.buffer_data.previous_session_buffers.contains(
                symbol_std_str)) {
            request_plot_buffer(symbol_std_str);
        }
    }

    deps_.state.completer_updated = true;
}

void MessageHandler::respond_get_observed_symbols() const {
    const auto lock = std::unique_lock{deps_.ui_mutex};
    auto message_composer = MessageComposer{};
    message_composer.push(MessageType::GetObservedSymbolsResponse)
        .push(deps_.buffer_data.held_buffers.size());
    for (const auto& name : deps_.buffer_data.held_buffers | std::views::keys) {
        message_composer.push(name);
    }
    message_composer.send(deps_.transport);
}

QListWidgetItem* MessageHandler::find_image_list_item(
    const std::string& variable_name_str) const {
    for (int i = 0; i < deps_.ui_components.ui->imageList->count(); ++i) {
        const auto item = deps_.ui_components.ui->imageList->item(i);
        if (item->data(Qt::UserRole) != variable_name_str.c_str()) {
            continue;
        }
        return item;
    }
    return nullptr;
}

void MessageHandler::repaint_image_list_icon(
    const std::string& variable_name_str) const {
    const auto paint_icon = [this, variable_name_str]() {
        const auto lock = std::unique_lock{deps_.ui_mutex};
        const auto itStage = deps_.buffer_data.stages.find(variable_name_str);
        if (itStage == deps_.buffer_data.stages.end()) [[unlikely]] {
            return;
        }

        const auto& [name, stage] = *itStage;

        const auto icon_size = deps_.get_icon_size();
        const auto icon_width = static_cast<int>(icon_size.width());
        const auto icon_height = static_cast<int>(icon_size.height());

        if (!deps_.ui_components.ui->bufferPreview->render_buffer_icon(
                *stage, icon_width, icon_height)) {
            return;
        }

        const auto& dialect = the_dialect();
        const auto bytes_per_line = icon_width * dialect.icon_bytes_per_pixel;
        const auto bufferIcon = QImage{stage->get_buffer_icon().data(),
                                       icon_width,
                                       icon_height,
                                       bytes_per_line,
                                       dialect.icon_image_format};

        if (const auto item = find_image_list_item(variable_name_str);
            item != nullptr) {
            item->setIcon(QPixmap::fromImage(bufferIcon));
        }
    };

    deps_.scheduler.run_icon_gl(paint_icon);
}

void MessageHandler::update_image_list_label(
    const std::string& variable_name_str, const std::string& label_str) const {
    if (const auto item = find_image_list_item(variable_name_str);
        item != nullptr) {
        item->setText(label_str.c_str());
    }
}

void MessageHandler::decode_plot_buffer_contents() {
    auto variable_name_str = std::string{};
    auto display_name_str = std::string{};
    auto pixel_layout_str = std::string{};
    auto transpose_buffer = bool{};
    auto buff_width = int{};
    auto buff_height = int{};
    auto buff_channels = int{};
    auto buff_stride = int{};
    auto buff_type = BufferType{};
    auto buff_contents = std::vector<std::byte>{};

    auto message_decoder = MessageDecoder{deps_.transport};
    message_decoder.read(variable_name_str)
        .read(display_name_str)
        .read(pixel_layout_str)
        .read(transpose_buffer)
        .read(buff_width)
        .read(buff_height)
        .read(buff_channels)
        .read(buff_stride)
        .read(buff_type)
        .read(buff_contents);

    plot_buffer_from_fields(variable_name_str,
                            display_name_str,
                            pixel_layout_str,
                            transpose_buffer,
                            buff_width,
                            buff_height,
                            buff_channels,
                            buff_stride,
                            buff_type,
                            std::move(buff_contents));
}

void MessageHandler::plot_buffer_from_fields(
    const std::string& variable_name_str,
    const std::string& display_name_str,
    const std::string& pixel_layout_str,
    bool transpose_buffer,
    int buff_width,
    int buff_height,
    int buff_channels,
    int buff_stride,
    BufferType buff_type,
    std::vector<std::byte> buff_contents) {
    const auto lock = std::unique_lock{deps_.ui_mutex};

    if (buff_type == BufferType::Float64) {
        deps_.buffer_data.held_buffers[variable_name_str] =
            make_float_buffer_from_double(buff_contents);
    } else {
        deps_.buffer_data.held_buffers[variable_name_str] =
            std::move(buff_contents);
    }
    const auto& held_buffer = deps_.buffer_data.held_buffers[variable_name_str];
    const auto buff_span = std::span<const std::byte>(held_buffer);

    auto visualized_width = int{};
    auto visualized_height = int{};
    if (!transpose_buffer) {
        visualized_width = buff_width;
        visualized_height = buff_height;
    } else {
        visualized_width = buff_height;
        visualized_height = buff_width;
    }

    const auto label_str = [&] {
        std::stringstream label_ss;
        label_ss << display_name_str;
        label_ss << "\n[" << visualized_width << "x" << visualized_height
                 << "]";
        label_ss << "\n"
                 << get_type_label(static_cast<int>(buff_type), buff_channels);
        return label_ss.str();
    }();

    const BufferParams params{.buffer = buff_span,
                              .buffer_width_i = buff_width,
                              .buffer_height_i = buff_height,
                              .channels = buff_channels,
                              .type = buff_type,
                              .step = buff_stride,
                              .pixel_layout = pixel_layout_str,
                              .transpose_buffer = transpose_buffer};

    if (deps_.buffer_data.stages.find(variable_name_str) ==
        deps_.buffer_data.stages.end()) {

        auto stage = deps_.create_stage();
        deps_.scheduler.run_gl([this,
                                stage,
                                params,
                                variable_name_str,
                                label_str,
                                ac_enabled = deps_.state.ac_enabled]() mutable {
            if (!stage->initialize(params)) [[unlikely]] {
                std::cerr << "[Error] Could not initialize OpenGL canvas. Stage "
                             "initialization failed."
                          << std::endl;
                return;
            }
            stage->set_contrast_enabled(ac_enabled);
            {
                const auto gl_lock = std::unique_lock{deps_.ui_mutex};
                deps_.buffer_data.stages.try_emplace(variable_name_str, stage);
            }
            // Add the list row only after the stage initialized and is in the
            // map, so a failed init leaves no orphan row. blockSignals prevents
            // premature stage selection before select_stage is called.
            {
                auto new_item = std::make_unique<QListWidgetItem>(
                    label_str.c_str(), deps_.ui_components.ui->imageList);
                new_item->setData(Qt::UserRole,
                                  QString(variable_name_str.c_str()));
                new_item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled |
                                   Qt::ItemIsDragEnabled);
                auto* list = deps_.ui_components.ui->imageList;
                list->blockSignals(true);
                list->addItem(new_item.release());
                list->blockSignals(false);
            }
            deps_.select_stage(stage);
            {
                const auto ui_lock = std::unique_lock{deps_.ui_mutex};
                auto* list = deps_.ui_components.ui->imageList;
                list->blockSignals(true);
                for (int i = 0; i < list->count(); ++i) {
                    const auto* item = list->item(i);
                    if (item->data(Qt::UserRole).toString().toStdString() ==
                        variable_name_str) {
                        list->setCurrentRow(i);
                        break;
                    }
                }
                list->blockSignals(false);
            }
            deps_.state.request_render_update = true;
            deps_.state.request_icons_update = true;
        });
    } else {
        deps_.scheduler.run_gl([this, params, variable_name_str]() {
            const auto gl_lock = std::unique_lock{deps_.ui_mutex};
            const auto it = deps_.buffer_data.stages.find(variable_name_str);
            if (it == deps_.buffer_data.stages.end()) [[unlikely]] {
                return;
            }
            if (!it->second->buffer_update(params)) [[unlikely]] {
                std::cerr << "[Error] Buffer update failed for: "
                          << variable_name_str << std::endl;
            }
            deps_.state.request_icons_update = true;
            deps_.state.request_render_update = true;
        });
    }

    repaint_image_list_icon(variable_name_str);
    update_image_list_label(variable_name_str, label_str);

    if (!deps_.buffer_data.currently_selected_stage.expired()) {
        emit acMinLabelsResetRequested();
        emit acMaxLabelsResetRequested();
    }

    emit settingsPersistenceRequested();

    deps_.state.request_render_update = true;
}

void MessageHandler::decode_plot_buffer_begin() {
    auto p = BufferAssembler::BeginParams{};
    auto type_int = int{};
    auto message_decoder = MessageDecoder{deps_.transport};
    message_decoder.read(p.variable_name)
        .read(p.display_name)
        .read(p.pixel_layout)
        .read(p.transpose)
        .read(p.width)
        .read(p.height)
        .read(p.channels)
        .read(p.stride)
        .read(type_int)
        .read(p.total_byte_size);
    p.type = type_int;
    buffer_assembler_.begin(std::move(p));
}

void MessageHandler::decode_plot_buffer_chunk() {
    auto variable_name = std::string{};
    auto row_offset = std::size_t{};
    auto row_count = std::size_t{};
    auto bytes = std::vector<std::byte>{};
    auto message_decoder = MessageDecoder{deps_.transport};
    message_decoder.read(variable_name)
        .read(row_offset)
        .read(row_count)
        .read(bytes);
    if (!buffer_assembler_.chunk(variable_name, row_offset, row_count, bytes)) {
        std::cerr << "[Error] Dropped buffer chunk for: " << variable_name
                  << std::endl;
    }
}

void MessageHandler::decode_plot_buffer_end() {
    auto variable_name = std::string{};
    auto message_decoder = MessageDecoder{deps_.transport};
    message_decoder.read(variable_name);
    auto assembled = buffer_assembler_.end(variable_name);
    if (!assembled) {
        std::cerr << "[Error] PlotBufferEnd for unknown buffer: " << variable_name
                  << std::endl;
        return;
    }
    plot_buffer_from_fields(assembled->variable_name,
                            assembled->display_name,
                            assembled->pixel_layout,
                            assembled->transpose,
                            assembled->width,
                            assembled->height,
                            assembled->channels,
                            assembled->stride,
                            static_cast<BufferType>(assembled->type),
                            std::move(assembled->bytes));
}

void MessageHandler::notify_buffer_removed(const QString& buffer_name) const {
    auto message_composer = MessageComposer{};
    message_composer.push(MessageType::BufferRemoved)
        .push(buffer_name.toStdString());
    message_composer.send(deps_.transport);
}

void MessageHandler::decode_apply_session_state() const {
    auto message_decoder = MessageDecoder{deps_.transport};
    auto json = std::string{};
    message_decoder.read(json);

    if (deps_.settings_applier == nullptr) {
        return;
    }
    auto fields = SessionStateFields{};
    if (!parse_session_state_json(QByteArray::fromStdString(json), fields)) {
        std::cerr << "[OID] Invalid ApplySessionState JSON" << std::endl;
        return;
    }
    apply_session_state_fields(fields, *deps_.settings_applier);
}

void MessageHandler::decode_incoming_messages() {
    {
        const auto lock = std::unique_lock{deps_.ui_mutex};
        deps_.buffer_data.available_vars.clear();
    }

    if (!deps_.transport.has_data()) [[unlikely]] {
        return;
    }

    auto header = MessageType{};
    std::array<std::byte, sizeof(header)> header_bytes{};
    if (deps_.transport.receive(header_bytes) != header_bytes.size()) [[unlikely]] {
        return;
    }
    header = std::bit_cast<MessageType>(header_bytes);

    switch (header) {
    case MessageType::SetAvailableSymbols:
        decode_set_available_symbols();
        break;
    case MessageType::GetObservedSymbols:
        respond_get_observed_symbols();
        break;
    case MessageType::PlotBufferContents:
        decode_plot_buffer_contents();
        break;
    case MessageType::PlotBufferBegin:
        decode_plot_buffer_begin();
        break;
    case MessageType::PlotBufferChunk:
        decode_plot_buffer_chunk();
        break;
    case MessageType::PlotBufferEnd:
        decode_plot_buffer_end();
        break;
    case MessageType::ExportSelectedBuffer:
        emit exportSelectedBufferRequested();
        break;
    case MessageType::ApplySessionState:
        decode_apply_session_state();
        break;
    default:
        break;
    }
}

void MessageHandler::request_plot_buffer(
    const std::string_view buffer_name) const {
    auto message_composer = MessageComposer{};
    message_composer.push(MessageType::PlotBufferRequest)
        .push(std::string(buffer_name))
        .send(deps_.transport);
}

void MessageHandler::request_export_buffer(const QString& buffer_name,
                                           const int format,
                                           const QList<float>& contrast) const {
    auto message_composer = MessageComposer{};
    message_composer.push(MessageType::ExportBufferRequest)
        .push(buffer_name.toStdString())
        .push(format);
    for (int i = 0; i < 8; ++i) {
        const auto value = i < contrast.size() ? contrast[i] : 0.f;
        message_composer.push(value);
    }
    message_composer.send(deps_.transport);
}

std::string MessageHandler::get_type_label(const int type, const int channels) {
    auto result = std::stringstream{};
    switch (static_cast<BufferType>(type)) {
    case BufferType::Float32:
        result << "float32";
        break;
    case BufferType::UnsignedByte:
        result << "uint8";
        break;
    case BufferType::Short:
        result << "int16";
        break;
    case BufferType::UnsignedShort:
        result << "uint16";
        break;
    case BufferType::Int32:
        result << "int32";
        break;
    case BufferType::Float64:
        result << "float64";
        break;
    default:
        result << "unknown";
        break;
    }
    result << "x" << channels;

    return result.str();
}

} // namespace oid
