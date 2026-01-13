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

#ifndef MESSAGE_HANDLER_H_
#define MESSAGE_HANDLER_H_

#include <functional>
#include <mutex>
#include <string>
#include <string_view>

class QListWidgetItem;
class QSizeF;
class QTcpSocket;

namespace oid
{

struct BufferData;
struct UIComponents;
struct WindowState;
class AutoContrastController;
class MainWindow;
class Stage;

class MessageHandler
{
  public:
    struct Dependencies
    {
        std::recursive_mutex& ui_mutex;
        BufferData& buffer_data;
        WindowState& state;
        UIComponents& ui_components;
        QTcpSocket& socket;
        AutoContrastController& ac_controller;
        MainWindow& main_window;
        std::function<QSizeF()> get_icon_size;
        std::function<void()> persist_settings_deferred;
    };

    explicit MessageHandler(Dependencies deps);

    void decode_incoming_messages();
    void request_plot_buffer(std::string_view buffer_name);
    void repaint_image_list_icon(const std::string& variable_name_str);

  private:
    Dependencies deps_;

    void decode_set_available_symbols();
    void respond_get_observed_symbols();
    void decode_plot_buffer_contents();

    QListWidgetItem* find_image_list_item(const std::string& variable_name_str) const;
    void update_image_list_label(const std::string& variable_name_str,
                                  const std::string& label_str) const;

    static std::string get_type_label(int type, int channels);
};

} // namespace oid

#endif // MESSAGE_HANDLER_H_
