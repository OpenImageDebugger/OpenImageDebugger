/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 OpenImageDebugger contributors
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

#include "main_window.h"

#include "ui_main_window.h"

using namespace std;


void MainWindow::decode_set_available_symbols()
{
    std::unique_lock<std::mutex> lock(ui_mutex_);
    MessageDecoder message_decoder(&socket_);
    message_decoder.read<QStringList, QString>(available_vars_);

    for (const auto& symbol_value : available_vars_) {
        // Plot buffer if it was available in the previous session
        if (previous_session_buffers_.find(symbol_value.toStdString()) !=
            previous_session_buffers_.end()) {
            request_plot_buffer(symbol_value.toStdString().data());
        }
    }

    completer_updated_ = true;
};


void MainWindow::respond_get_observed_symbols()
{
    MessageComposer message_composer;
    message_composer.push(MessageType::GetObservedSymbolsResponse)
        .push(held_buffers_.size());
    for (const auto& name : held_buffers_) {
        message_composer.push(name.first);
    }
    message_composer.send(&socket_);
};


void MainWindow::decode_plot_buffer_contents()
{
    // Buffer icon dimensions
    QSizeF icon_size         = get_icon_size();
    int icon_width           = static_cast<int>(icon_size.width());
    int icon_height          = static_cast<int>(icon_size.height());
    const int bytes_per_line = icon_width * 3;

    // Read buffer info
    string variable_name_str;
    string display_name_str;
    string pixel_layout_str;
    bool transpose_buffer;
    int buff_width;
    int buff_height;
    int buff_channels;
    int buff_stride;
    BufferType buff_type;
    vector<uint8_t> buff_contents;

    MessageDecoder message_decoder(&socket_);
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

    auto buffer_stage = stages_.find(variable_name_str);

    if (buff_type == BufferType::Float64) {
        held_buffers_[variable_name_str] =
            make_float_buffer_from_double(buff_contents);
    } else {
        held_buffers_[variable_name_str] = std::move(buff_contents);
    }

    // Human readable dimensions
    int visualized_width;
    int visualized_height;
    if (!transpose_buffer) {
        visualized_width  = buff_width;
        visualized_height = buff_height;
    } else {
        visualized_width  = buff_height;
        visualized_height = buff_width;
    }

    if (buffer_stage == stages_.end()) { // New buffer request
        shared_ptr<Stage> stage = make_shared<Stage>(this);
        if (!stage->initialize(held_buffers_[variable_name_str].data(),
                               buff_width,
                               buff_height,
                               buff_channels,
                               buff_type,
                               buff_stride,
                               pixel_layout_str,
                               transpose_buffer)) {
            cerr << "[error] Could not initialize opengl canvas!" << endl;
        }
        stage->contrast_enabled    = ac_enabled_;
        stages_[variable_name_str] = stage;

        ui_->bufferPreview->render_buffer_icon(
            stage.get(), icon_width, icon_height);

        QImage bufferIcon(stage->buffer_icon.data(),
                          icon_width,
                          icon_height,
                          bytes_per_line,
                          QImage::Format_RGB888);

        stringstream label;
        label << display_name_str << "\n[" << visualized_width << "x"
              << visualized_height << "]\n"
              << get_type_label(buff_type, buff_channels);
        QListWidgetItem* item =
            new QListWidgetItem(QPixmap::fromImage(bufferIcon),
                                label.str().c_str(),
                                ui_->imageList);
        item->setData(Qt::UserRole, QString(variable_name_str.c_str()));
        item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled |
                       Qt::ItemIsDragEnabled);
        ui_->imageList->addItem(item);

        persist_settings_deferred();
    } else { // Update buffer request
        buffer_stage->second->buffer_update(
            held_buffers_[variable_name_str].data(),
            buff_width,
            buff_height,
            buff_channels,
            buff_type,
            buff_stride,
            pixel_layout_str,
            transpose_buffer);

        // Update buffer icon
        shared_ptr<Stage>& stage = stages_[variable_name_str];
        ui_->bufferPreview->render_buffer_icon(
            stage.get(), icon_width, icon_height);

        // Looking for corresponding item...
        QImage bufferIcon(stage->buffer_icon.data(),
                          icon_width,
                          icon_height,
                          bytes_per_line,
                          QImage::Format_RGB888);
        stringstream label;
        label << display_name_str << "\n[" << visualized_width << "x"
              << visualized_height << "]\n"
              << get_type_label(buff_type, buff_channels);

        for (int i = 0; i < ui_->imageList->count(); ++i) {
            QListWidgetItem* item = ui_->imageList->item(i);
            if (item->data(Qt::UserRole) == variable_name_str.c_str()) {
                item->setIcon(QPixmap::fromImage(bufferIcon));
                item->setText(label.str().c_str());
                break;
            }
        }

        // Update AC values
        if (currently_selected_stage_ != nullptr) {
            reset_ac_min_labels();
            reset_ac_max_labels();
        }
    }

    request_render_update_ = true;
}


void MainWindow::decode_incoming_messages()
{
    // Close application if server has disconnected
    if(socket_.state() == QTcpSocket::UnconnectedState) {
        QApplication::quit();
    }

    available_vars_.clear();

    if (socket_.bytesAvailable() == 0) {
        return;
    }

    MessageType header;
    if (!socket_.read(reinterpret_cast<char*>(&header),
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
    MessageComposer message_composer;
    message_composer.push(MessageType::PlotBufferRequest)
        .push(std::string(buffer_name))
        .send(&socket_);
}
