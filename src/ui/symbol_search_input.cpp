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

/*
 * Advanced search completer based on:
 * http://www.qtcentre.org/threads/23518
 */
#include "symbol_search_input.h"

#include <QAbstractItemView>
#include <QKeyEvent>


SymbolSearchInput::SymbolSearchInput(QWidget* parent)
    : QLineEdit(parent)
    , completer_(nullptr)
{
}


SymbolSearchInput::~SymbolSearchInput() = default;


void SymbolSearchInput::set_completer(SymbolCompleter* completer)
{
    if (completer_) {
        disconnect(completer_, nullptr, this, nullptr);
    }

    completer_ = completer;

    if (!completer_) {
        return;
    }

    completer_->setWidget(this);
    connect(completer,
            SIGNAL(activated(const QString&)),
            this,
            SLOT(insert_completion(const QString&)));
}


SymbolCompleter* SymbolSearchInput::completer() const
{
    return completer_;
}


void SymbolSearchInput::insert_completion(const QString& completion)
{
    setText(completion);
    selectAll();
}


void SymbolSearchInput::keyPressEvent(QKeyEvent* e)
{
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

    if (!is_shortcut) {
        QLineEdit::keyPressEvent(
            e); // Don't send the shortcut (CTRL-E) to the text edit.
    }

    if (!completer_) {
        return;
    }

    if (const auto ctrl_or_shift =
            e->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier);
        !is_shortcut && !ctrl_or_shift && e->modifiers() != Qt::NoModifier) {
        completer_->popup()->hide();
        return;
    }

    completer_->update(text());
    completer_->popup()->setCurrentIndex(
        completer_->completionModel()->index(0, 0));
}
