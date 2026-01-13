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

/*
 * Advanced search completer based on:
 * http://www.qtcentre.org/threads/23518
 */
#include "symbol_search_input.h"

#include <QAbstractItemView>
#include <QDateTime>
#include <QKeyEvent>
#include <QString>
#include <fstream>
#include <sstream>


namespace oid
{

SymbolSearchInput::SymbolSearchInput(QWidget* parent)
    : QLineEdit{parent}
{
}


SymbolSearchInput::~SymbolSearchInput() = default;


void SymbolSearchInput::set_completer(SymbolCompleter& completer)
{
    // #region agent log
    {
        std::ofstream log("/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
                          std::ios::app);
        std::stringstream ss;
        ss << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\","
              "\"hypothesisId\":\"D\",\"location\":\"symbol_search_input.cpp:"
              "48\",\"message\":\"set_completer "
              "called\",\"data\":{\"completer_ptr\":\""
           << std::hex << reinterpret_cast<quintptr>(&completer)
           << "\"},\"timestamp\":" << QDateTime::currentMSecsSinceEpoch()
           << "}\n";
        log << ss.str();
    }
    // #endregion

    if (completer_.has_value()) {
        disconnect(&completer_->get(), nullptr, this, nullptr);
    }

    completer_ = std::ref(completer);

    // Register the completer with QLineEdit - this is required for automatic
    // popup behavior when typing
    QLineEdit::setCompleter(&completer);

    this->completer().setWidget(this);
    connect(&completer,
            qOverload<const QString&>(&QCompleter::activated),
            this,
            &SymbolSearchInput::insert_completion);

    // #region agent log
    {
        std::ofstream log("/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
                          std::ios::app);
        std::stringstream ss;
        ss << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\","
              "\"hypothesisId\":\"D\",\"location\":\"symbol_search_input.cpp:"
              "65\",\"message\":\"set_completer "
              "completed\",\"data\":{\"qlineedit_completer_set\":"
           << (QLineEdit::completer() != nullptr ? "true" : "false")
           << "},\"timestamp\":" << QDateTime::currentMSecsSinceEpoch()
           << "}\n";
        log << ss.str();
    }
    // #endregion
}


SymbolCompleter* SymbolSearchInput::symbolCompleter() const
{
    if (!completer_.has_value()) {
        return nullptr;
    }
    return &completer_->get();
}


void SymbolSearchInput::insert_completion(const QString& completion)
{
    setText(completion);
    selectAll();
}


void SymbolSearchInput::keyPressEvent(QKeyEvent* e)
{
    // #region agent log
    {
        std::ofstream log("/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
                          std::ios::app);
        QString textStr = text();
        textStr.replace("\"", "\\\"").replace("\n", "\\n");
        std::stringstream ss;
        ss << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\","
              "\"hypothesisId\":\"B\",\"location\":\"symbol_search_input.cpp:"
              "84\",\"message\":\"keyPressEvent entry\",\"data\":{\"key\":"
           << e->key() << ",\"modifiers\":" << static_cast<int>(e->modifiers())
           << ",\"text\":\"" << textStr.toStdString()
           << "\",\"text_length\":" << text().length()
           << "},\"timestamp\":" << QDateTime::currentMSecsSinceEpoch()
           << "}\n";
        log << ss.str();
    }
    // #endregion

    // The following keys are forwarded by the completer to the widget
    switch (e->key()) {
    case Qt::Key_Escape:
        clearFocus();
        e->accept();
        return;
    case Qt::Key_Tab:
        [[fallthrough]];
    case Qt::Key_Backtab:
        [[fallthrough]];
    case Qt::Key_Enter:
        [[fallthrough]];
    case Qt::Key_Return:
        e->ignore();
        return; // Let the completer do default behavior
    default:
        break;
    }

    const auto is_shortcut =
        (e->modifiers() & Qt::ControlModifier) && e->key() == Qt::Key_E;

    // Handle editing keys (Backspace, Delete) specially to ensure they work
    // even when the popup is visible
    const bool is_editing_key =
        (e->key() == Qt::Key_Backspace || e->key() == Qt::Key_Delete);
    const bool popup_visible =
        completer().popup() != nullptr && completer().popup()->isVisible();

    if (is_editing_key && popup_visible) {
        // For editing keys (Backspace/Delete), hide popup and let QLineEdit
        // handle the key normally. Don't update completer to avoid
        // auto-completion interference.
        completer().popup()->hide();
        if (!is_shortcut) {
            QLineEdit::keyPressEvent(e);
        }
        return;
    }

    if (!is_shortcut) {
        QLineEdit::keyPressEvent(
            e); // Don't send the shortcut (CTRL-E) to the text edit.
    }

    // completer_ is guaranteed to be set via set_completer() before use

    if (const auto ctrl_or_shift =
            e->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier);
        !is_shortcut && !ctrl_or_shift && e->modifiers() != Qt::NoModifier) {
        completer().popup()->hide();
        return;
    }

    // #region agent log
    {
        std::ofstream log("/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
                          std::ios::app);
        QString textStr = text();
        textStr.replace("\"", "\\\"").replace("\n", "\\n");
        std::stringstream ss;
        ss << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\","
              "\"hypothesisId\":\"B\",\"location\":\"symbol_search_input.cpp:"
              "122\",\"message\":\"before "
              "completer().update()\",\"data\":{\"text_after_keypress\":\""
           << textStr.toStdString() << "\",\"completer_widget_set\":"
           << (completer().widget() != nullptr ? "true" : "false")
           << "},\"timestamp\":" << QDateTime::currentMSecsSinceEpoch()
           << "}\n";
        log << ss.str();
    }
    // #endregion

    completer().update(text());

    // #region agent log
    {
        std::ofstream log("/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
                          std::ios::app);
        auto* popup         = completer().popup();
        QRect popupGeometry = popup->geometry();
        std::stringstream ss;
        ss << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\","
              "\"hypothesisId\":\"E\",\"location\":\"symbol_search_input.cpp:"
              "125\",\"message\":\"after "
              "completer().update()\",\"data\":{\"popup_visible\":"
           << (popup->isVisible() ? "true" : "false")
           << ",\"popup_count\":" << completer().completionCount()
           << ",\"popup_x\":" << popupGeometry.x()
           << ",\"popup_y\":" << popupGeometry.y()
           << ",\"popup_width\":" << popupGeometry.width()
           << ",\"popup_height\":" << popupGeometry.height()
           << ",\"popup_minimum_height\":" << popup->minimumHeight()
           << ",\"widget_visible\":" << (isVisible() ? "true" : "false")
           << "},\"timestamp\":" << QDateTime::currentMSecsSinceEpoch()
           << "}\n";
        log << ss.str();
    }
    // #endregion

    completer().popup()->setCurrentIndex(
        completer().completionModel()->index(0, 0));
}

} // namespace oid
