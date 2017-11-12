/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 GDB ImageWatch contributors
 * (github.com/csantosbh/gdb-imagewatch/)
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

#include "go_to_widget.h"

#include <QHBoxLayout>
#include <QKeyEvent>


GoToWidget::GoToWidget(QWidget* parent)
    : QWidget(parent)
{
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setMargin(0);
    layout->setSpacing(0);

    x_coordinate_ = new QLineEdit(this);
    x_coordinate_->setPlaceholderText("x");

    y_coordinate_ = new QLineEdit(this);
    y_coordinate_->setPlaceholderText("y");

    layout->addWidget(x_coordinate_);
    layout->addWidget(y_coordinate_);

    setVisible(false);
}


void GoToWidget::keyPressEvent(QKeyEvent* e)
{
    switch (e->key()) {
    case Qt::Key_Escape:
        toggle_visible();
        e->accept();

        return;
    case Qt::Key_Enter:
    case Qt::Key_Return:
        toggle_visible();
        // TODO emit "go to position" event

        e->accept();
        return; // Let the completer do default behavior
    }
}


void GoToWidget::toggle_visible()
{
    QWidget* parent_widget = static_cast<QWidget*>(parent());

    if (isVisible()) {
        hide();

        parent_widget->setFocus();
    } else {
        show();

        this->move(parent_widget->width() - this->width(),
                   parent_widget->height() - this->height());
        x_coordinate_->setFocus();
    }
}
