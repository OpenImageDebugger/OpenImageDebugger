#include <iostream>

#include <QAction>
#include <QHBoxLayout>
#include <QIcon>
#include <QPainter>

#include "decorated_line_edit.h"


DecoratedLineEdit::DecoratedLineEdit(const char* icon_path,
                                     const char* tooltip,
                                     QWidget* parent)
    : QLineEdit(parent)
{
    QIcon label_icon(icon_path);
    QAction* label_widget = new QAction(label_icon, tooltip, this);
    addAction(label_widget, QLineEdit::ActionPosition::LeadingPosition);
}
