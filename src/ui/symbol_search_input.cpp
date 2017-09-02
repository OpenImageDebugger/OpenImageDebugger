/*
 * Advanced search completer based on:
 * http://www.qtcentre.org/threads/23518
 */

#include <QAbstractItemView>
#include <QKeyEvent>
#include "symbol_search_input.h"

SymbolSearchInput::SymbolSearchInput(QWidget *parent)
    : QLineEdit(parent), completer_(0)
{
}

SymbolSearchInput::~SymbolSearchInput()
{
}

void SymbolSearchInput::setCompleter(SymbolCompleter *completer)
{
    if (completer_)
        QObject::disconnect(completer_, 0, this, 0);

    completer_ = completer;

    if (!completer_)
        return;

    completer_->setWidget(this);
    connect(completer, SIGNAL(activated(const QString&)),
            this, SLOT(insertCompletion(const QString&)));
}

SymbolCompleter *SymbolSearchInput::completer() const
{
    return completer_;
}

void SymbolSearchInput::insertCompletion(const QString& completion)
{
    setText(completion);
    selectAll();
}

void SymbolSearchInput::keyPressEvent(QKeyEvent *e)
{
    if (completer_ && completer_->popup()->isVisible())
    {
        // The following keys are forwarded by the completer to the widget
        switch (e->key())
        {
        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Escape:
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
            e->ignore();
            return; // Let the completer do default behavior
        }
    }

    bool isShortcut = (e->modifiers() & Qt::ControlModifier) && e->key() == Qt::Key_E;
    if (!isShortcut)
        QLineEdit::keyPressEvent(e); // Don't send the shortcut (CTRL-E) to the text edit.

    if (!completer_)
        return;

    bool ctrlOrShift = e->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier);
    if (!isShortcut && !ctrlOrShift && e->modifiers() != Qt::NoModifier)
    {
        completer_->popup()->hide();
        return;
    }

    completer_->update(text());
    completer_->popup()->setCurrentIndex(completer_->completionModel()->index(0, 0));
}
