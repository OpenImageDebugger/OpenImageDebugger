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

#include "decorated_line_edit.h"

#include <memory>
#include <string>

#include <QAction>
#include <QIcon>
#include <QPainter>

namespace oid
{

DecoratedLineEdit::DecoratedLineEdit(std::string_view icon_path,
                                     std::string_view tooltip,
                                     QWidget* parent)
    : QLineEdit{parent}
{
    // Convert std::string_view to std::string for Qt API (QIcon and QAction
    // require null-terminated strings)
    const auto label_icon = QIcon{std::string(icon_path).c_str()};
    auto label_widget     = std::make_unique<QAction>(
        label_icon, std::string(tooltip).c_str(), this);
    addAction(label_widget.release(), LeadingPosition);
}

} // namespace oid
