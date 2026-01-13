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

#include "ipc/message_exchange.h"
#include "main_window.h"

#include <bit>
#include <iostream>
#include <fstream>
#include <sstream>
#include <memory>
#include <ranges>
#include <vector>
#include <QString>
#include <QDateTime>

#include "ui_main_window.h"

namespace oid
{

void MainWindow::decode_set_available_symbols()
{
    // #region agent log
    {
        std::ofstream log("/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log", std::ios::app);
        std::stringstream ss;
        ss << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"A\",\"location\":\"message_processing.cpp:40\",\"message\":\"decode_set_available_symbols entry\",\"data\":{},\"timestamp\":" << QDateTime::currentMSecsSinceEpoch() << "}\n";
        log << ss.str();
    }
    // #endregion

    const auto lock      = std::unique_lock{ui_mutex_};
    auto message_decoder = MessageDecoder{&socket_};
    message_decoder.read<QStringList, QString>(buffer_data_.available_vars);

    // #region agent log
    {
        std::ofstream log("/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log", std::ios::app);
        QString first5 = buffer_data_.available_vars.mid(0, 5).join(", ");
        first5.replace("\"", "\\\"").replace("\n", "\\n");
        std::stringstream ss;
        ss << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"A\",\"location\":\"message_processing.cpp:48\",\"message\":\"decode_set_available_symbols after read\",\"data\":{\"symbol_count\":" << buffer_data_.available_vars.size() << ",\"first_5_symbols\":\"" << first5.toStdString() << "\"},\"timestamp\":" << QDateTime::currentMSecsSinceEpoch() << "}\n";
        log << ss.str();
    }
    // #endregion

    // Store for persistence (available_vars gets cleared in
    // decode_incoming_messages)
    buffer_data_.last_known_available_vars = buffer_data_.available_vars;

    // Trigger settings persistence to save available vars
    persist_settings_deferred();

    for (const auto& symbol_value : buffer_data_.available_vars) {
        // Plot buffer if it was available in the previous session
        // (either as a plotted buffer or as an available variable)
        const auto symbol_std_str = symbol_value.toStdString();
        if (buffer_data_.previous_session_buffers.contains(symbol_std_str) ||
            buffer_data_.previous_session_available_vars.contains(
                symbol_std_str)) {
            request_plot_buffer(symbol_std_str);
        }
    }

    state_.completer_updated = true;

    // #region agent log
    {
        std::ofstream log("/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log", std::ios::app);
        std::stringstream ss;
        ss << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"hypothesisId\":\"A\",\"location\":\"message_processing.cpp:64\",\"message\":\"decode_set_available_symbols exit\",\"data\":{\"completer_updated\":true},\"timestamp\":" << QDateTime::currentMSecsSinceEpoch() << "}\n";
        log << ss.str();
    }
    // #endregion
}


void MainWindow::respond_get_observed_symbols()
{
    const auto lock       = std::unique_lock{ui_mutex_};
    auto message_composer = MessageComposer{};
    message_composer.push(MessageType::GetObservedSymbolsResponse)
        .push(buffer_data_.held_buffers.size());
    for (const auto& name : buffer_data_.held_buffers | std::views::keys) {
        message_composer.push(name);
    }
    message_composer.send(&socket_);
}


QListWidgetItem*
MainWindow::find_image_list_item(const std::string& variable_name_str) const
{
    // Looking for corresponding item...
    for (int i = 0; i < ui_components_.ui->imageList->count(); ++i) {
        const auto item = ui_components_.ui->imageList->item(i);
        if (item->data(Qt::UserRole) != variable_name_str.c_str()) {
            continue;
        }

        return item;
    }
    return nullptr;
}


void MainWindow::repaint_image_list_icon(const std::string& variable_name_str)
{
    const auto lock    = std::unique_lock{ui_mutex_};
    const auto itStage = buffer_data_.stages.find(variable_name_str);
    if (itStage == buffer_data_.stages.end()) [[unlikely]] {
        return;
    }

    const auto& [name, stage] = *itStage;

    // Buffer icon dimensions
    const auto icon_size      = get_icon_size();
    const auto icon_width     = static_cast<int>(icon_size.width());
    const auto icon_height    = static_cast<int>(icon_size.height());
    const auto bytes_per_line = icon_width * 3;

    // Update buffer icon
    ui_components_.ui->bufferPreview->render_buffer_icon(
        *stage, icon_width, icon_height);

    // Construct icon widget
    const auto bufferIcon = QImage{stage->get_buffer_icon().data(),
                                   icon_width,
                                   icon_height,
                                   bytes_per_line,
                                   QImage::Format_RGB888};

    // Replace icon in the corresponding item
    if (const auto item = find_image_list_item(variable_name_str);
        item != nullptr) {
        item->setIcon(QPixmap::fromImage(bufferIcon));
    }
}


void MainWindow::update_image_list_label(const std::string& variable_name_str,
                                         const std::string& label_str) const
{
    // Replace text in the corresponding item
    if (const auto item = find_image_list_item(variable_name_str);
        item != nullptr) {
        item->setText(label_str.c_str());
    }
}


void MainWindow::decode_plot_buffer_contents()
{
    // Read buffer info
    auto variable_name_str = std::string{};
    auto display_name_str  = std::string{};
    auto pixel_layout_str  = std::string{};
    auto transpose_buffer  = bool{};
    auto buff_width        = int{};
    auto buff_height       = int{};
    auto buff_channels     = int{};
    auto buff_stride       = int{};
    auto buff_type         = BufferType{};
    auto buff_contents     = std::vector<std::byte>{};

    auto message_decoder = MessageDecoder{&socket_};
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

    const auto lock = std::unique_lock{ui_mutex_};

    // Put the data buffer into the container
    if (buff_type == BufferType::Float64) {
        buffer_data_.held_buffers[variable_name_str] =
            make_float_buffer_from_double(buff_contents);
    } else {
        buffer_data_.held_buffers[variable_name_str] = std::move(buff_contents);
    }
    const auto& held_buffer = buffer_data_.held_buffers[variable_name_str];
    const auto buff_span    = std::span<const std::byte>(held_buffer);

    // Human readable dimensions
    auto visualized_width  = int{};
    auto visualized_height = int{};
    if (!transpose_buffer) {
        visualized_width  = buff_width;
        visualized_height = buff_height;
    } else {
        visualized_width  = buff_height;
        visualized_height = buff_width;
    }

    const auto label_str = [&] {
        std::stringstream label_ss;
        label_ss << display_name_str;
        label_ss << "\n[" << visualized_width << "x" << visualized_height
                 << "]";
        label_ss << "\n" << get_type_label(buff_type, buff_channels);
        return label_ss.str();
    }();

    // Find corresponding stage buffer
    if (auto buffer_stage = buffer_data_.stages.find(variable_name_str);
        buffer_stage == buffer_data_.stages.end()) {

        // Construct a new stage buffer if needed
        auto stage = std::make_shared<Stage>(*this);
        if (const BufferParams params{.buffer           = buff_span,
                                      .buffer_width_i   = buff_width,
                                      .buffer_height_i  = buff_height,
                                      .channels         = buff_channels,
                                      .type             = buff_type,
                                      .step             = buff_stride,
                                      .pixel_layout     = pixel_layout_str,
                                      .transpose_buffer = transpose_buffer};
            !stage->initialize(params)) [[unlikely]] {
            std::cerr << "[Error] Could not initialize OpenGL canvas. Stage "
                         "initialization failed."
                      << std::endl;
            // Continue execution but mark that initialization failed
            // The stage will not be usable, but the application can continue
        }
        stage->set_contrast_enabled(state_.ac_enabled);
        buffer_stage =
            buffer_data_.stages.try_emplace(variable_name_str, stage).first;
    } else {

        // Update buffer data
        const BufferParams params{.buffer           = buff_span,
                                  .buffer_width_i   = buff_width,
                                  .buffer_height_i  = buff_height,
                                  .channels         = buff_channels,
                                  .type             = buff_type,
                                  .step             = buff_stride,
                                  .pixel_layout     = pixel_layout_str,
                                  .transpose_buffer = transpose_buffer};
        const auto& [buffer_name, buffer_stage_ptr] = *buffer_stage;
        if (!buffer_stage_ptr->buffer_update(params)) [[unlikely]] {
            std::cerr << "[Error] Buffer update failed for: "
                      << variable_name_str << std::endl;
            // Continue processing other buffers even if this one fails
        }
    }

    // Construct a new list widget if needed
    if (auto item = find_image_list_item(variable_name_str); item == nullptr) {
        // Create item with unique_ptr to manage ownership until addItem takes
        // it
        auto new_item = std::make_unique<QListWidgetItem>(
            label_str.c_str(), ui_components_.ui->imageList);
        new_item->setData(Qt::UserRole, QString(variable_name_str.c_str()));
        new_item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled |
                           Qt::ItemIsDragEnabled);
        // addItem() takes ownership, so release() is safe here
        ui_components_.ui->imageList->addItem(new_item.release());
    }

    // Update icon and text of corresponding item in image list
    repaint_image_list_icon(variable_name_str);
    update_image_list_label(variable_name_str, label_str);

    // Update AC values
    if (!buffer_data_.currently_selected_stage.expired()) {
        reset_ac_min_labels();
        reset_ac_max_labels();
    }

    // Update list of observed symbols in settings
    persist_settings_deferred();

    state_.request_render_update = true;
}


void MainWindow::decode_incoming_messages()
{
    // Close application if server has disconnected
    if (socket_.state() == QTcpSocket::UnconnectedState) [[unlikely]] {
        QApplication::quit();
    }

    {
        const auto lock = std::unique_lock{ui_mutex_};
        buffer_data_.available_vars.clear();
    }

    // Process all available messages in a loop
    while (socket_.bytesAvailable() > 0) {
        auto header = MessageType{};
        if (!socket_.read(std::bit_cast<char*>(&header),
                          static_cast<qint64>(sizeof(header)))) [[unlikely]] {
            const auto error = socket_.error();
            std::cerr << "[Error] Failed to read message header: "
                      << socket_.errorString().toStdString() << std::endl;

            // Handle critical errors that indicate connection is broken
            if (error == QAbstractSocket::RemoteHostClosedError ||
                error == QAbstractSocket::NetworkError ||
                error == QAbstractSocket::ConnectionRefusedError ||
                error == QAbstractSocket::SocketTimeoutError) [[unlikely]] {
                std::cerr << "[Error] Critical socket error detected. Closing "
                             "connection."
                          << std::endl;
                socket_.close();
                // Next call will detect UnconnectedState and quit
            }
            // For other errors (e.g., temporary read errors), just return and
            // retry next time
            return;
        }

        socket_.waitForReadyRead(100);

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
        default:
            break;
        }
    }
}


void MainWindow::request_plot_buffer(std::string_view buffer_name)
{
    auto message_composer = MessageComposer{};
    message_composer.push(MessageType::PlotBufferRequest)
        .push(std::string(buffer_name))
        .send(&socket_);
}

} // namespace oid
