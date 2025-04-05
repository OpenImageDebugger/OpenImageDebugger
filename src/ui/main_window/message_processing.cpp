/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2025 OpenImageDebugger contributors
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
#include <memory>
#include <ranges>

#include "ui_main_window.h"

namespace oid
{

void MainWindow::decode_set_available_symbols()
{
    const auto lock      = std::unique_lock{ui_mutex_};
    auto message_decoder = MessageDecoder{&socket_};
    message_decoder.read<QStringList, QString>(available_vars_);

    for (const auto& symbol_value : available_vars_) {
        // Plot buffer if it was available in the previous session
        if (previous_session_buffers_.contains(symbol_value.toStdString())) {
            request_plot_buffer(symbol_value.toStdString().data());
        }
    }

    completer_updated_ = true;
}


void MainWindow::respond_get_observed_symbols()
{
    auto message_composer = MessageComposer{};
    message_composer.push(MessageType::GetObservedSymbolsResponse)
        .push(held_buffers_.size());
    for (const auto& name : held_buffers_ | std::views::keys) {
        message_composer.push(name);
    }
    message_composer.send(&socket_);
}


QListWidgetItem*
MainWindow::find_image_list_item(const std::string& variable_name_str) const
{
    // Looking for corresponding item...
    for (int i = 0; i < ui_->imageList->count(); ++i) {
        const auto item = ui_->imageList->item(i);
        if (item->data(Qt::UserRole) != variable_name_str.c_str()) {
            continue;
        }

        return item;
    }
    return nullptr;
}


void MainWindow::repaint_image_list_icon(const std::string& variable_name_str)
{
    const auto itStage = stages_.find(variable_name_str);
    if (itStage == stages_.end()) {
        return;
    }

    const auto& stage = itStage->second;

    // Buffer icon dimensions
    const auto icon_size      = get_icon_size();
    const auto icon_width     = static_cast<int>(icon_size.width());
    const auto icon_height    = static_cast<int>(icon_size.height());
    const auto bytes_per_line = icon_width * 3;

    // Update buffer icon
    ui_->bufferPreview->render_buffer_icon(
        stage.get(), icon_width, icon_height);

    // Construct icon widget
    const auto bufferIcon = QImage{stage->buffer_icon.data(),
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
    auto buff_contents     = std::vector<std::uint8_t>{};

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

    // Put the data buffer into the container
    if (buff_type == BufferType::Float64) {
        held_buffers_[variable_name_str] =
            make_float_buffer_from_double(buff_contents);
    } else {
        held_buffers_[variable_name_str] = std::move(buff_contents);
    }
    const auto buff_ptr = held_buffers_[variable_name_str].data();

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
    if (auto buffer_stage = stages_.find(variable_name_str);
        buffer_stage == stages_.end()) {

        // Construct a new stage buffer if needed
        auto stage = std::make_shared<Stage>(this);
        if (!stage->initialize(buff_ptr,
                               buff_width,
                               buff_height,
                               buff_channels,
                               buff_type,
                               buff_stride,
                               pixel_layout_str,
                               transpose_buffer)) {
            std::cerr << "[error] Could not initialize opengl canvas!"
                      << std::endl;
        }
        stage->contrast_enabled = ac_enabled_;
        buffer_stage = stages_.try_emplace(variable_name_str, stage).first;
    } else {

        // Update buffer data
        buffer_stage->second->buffer_update(buff_ptr,
                                            buff_width,
                                            buff_height,
                                            buff_channels,
                                            buff_type,
                                            buff_stride,
                                            pixel_layout_str,
                                            transpose_buffer);
    }

    // Construct a new list widget if needed
    if (auto item = find_image_list_item(variable_name_str); item == nullptr) {
        item =
            std::make_unique<QListWidgetItem>(label_str.c_str(), ui_->imageList)
                .release();
        item->setData(Qt::UserRole, QString(variable_name_str.c_str()));
        item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled |
                       Qt::ItemIsDragEnabled);
        ui_->imageList->addItem(item);
    }

    // Update icon and text of corresponding item in image list
    repaint_image_list_icon(variable_name_str);
    update_image_list_label(variable_name_str, label_str);

    // Update AC values
    if (currently_selected_stage_ != nullptr) {
        reset_ac_min_labels();
        reset_ac_max_labels();
    }

    // Update list of observed symbols in settings
    persist_settings_deferred();

    request_render_update_ = true;
}


void MainWindow::decode_incoming_messages()
{
    // Close application if server has disconnected
    if (socket_.state() == QTcpSocket::UnconnectedState) {
        QApplication::quit();
    }

    available_vars_.clear();

    if (socket_.bytesAvailable() == 0) {
        return;
    }

    auto header = MessageType{};
    if (!socket_.read(std::bit_cast<char*>(&header),
                      static_cast<qint64>(sizeof(header)))) {
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


void MainWindow::request_plot_buffer(const char* buffer_name)
{
    auto message_composer = MessageComposer{};
    message_composer.push(MessageType::PlotBufferRequest)
        .push(std::string(buffer_name))
        .send(&socket_);
}

} // namespace oid
