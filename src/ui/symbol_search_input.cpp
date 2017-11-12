/*
 * Advanced search completer based on:
 * http://www.qtcentre.org/threads/23518
 */

#include <QAbstractItemView>
#include <QKeyEvent>

#include "symbol_search_input.h"


SymbolSearchInput::SymbolSearchInput(QWidget* parent)
    : QLineEdit(parent)
    , completer_(nullptr)
{
}


SymbolSearchInput::~SymbolSearchInput()
{
}


void SymbolSearchInput::set_completer(SymbolCompleter* completer)
{
    if (completer_) {
        QObject::disconnect(completer_, 0, this, 0);
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
    case Qt::Key_Backtab:
    case Qt::Key_Enter:
    case Qt::Key_Return:
        e->ignore();
        return; // Let the completer do default behavior
    }

    bool is_shortcut =
        (e->modifiers() & Qt::ControlModifier) && e->key() == Qt::Key_E;
    if (!is_shortcut)
        QLineEdit::keyPressEvent(
            e); // Don't send the shortcut (CTRL-E) to the text edit.

    if (!completer_)
        return;

    bool ctrl_or_shift =
        e->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier);

    if (!is_shortcut && !ctrl_or_shift && e->modifiers() != Qt::NoModifier) {
        completer_->popup()->hide();
        return;
    }

    completer_->update(text());
    completer_->popup()->setCurrentIndex(
        completer_->completionModel()->index(0, 0));
}
