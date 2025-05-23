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

#include "ui/main_window/main_window.h"

#include <QApplication>
#include <QCommandLineParser>


int main(int argc, char* argv[])
{
    auto app = QApplication{argc, argv};

    auto parser = QCommandLineParser{};
    parser.addOptions({
        {"h", "hostname", "hostname", "127.0.0.1"},
        {"p", "port", "port", "9588"},
    });
    parser.parse(QCoreApplication::arguments());

    auto host_settings = oid::ConnectionSettings{};
    host_settings.url  = parser.value("h").toStdString();
    host_settings.port = static_cast<uint16_t>(parser.value("p").toUInt());

    auto window = oid::MainWindow{host_settings};
    window.showWindow();
    return QApplication::exec();
}
