/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2024 OpenImageDebugger contributors
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
#include "go_to_widget.h"

#include <cmath>

#include <memory>

#include <QHBoxLayout>
#include <QIntValidator>
#include <QKeyEvent>

namespace oid
{

GoToWidget::GoToWidget(QWidget* parent)
    : QWidget{parent}
{
    auto layout = std::make_unique<QHBoxLayout>(this);
    layout->setMargin(0);
    layout->setSpacing(0);

    x_coordinate_ = std::make_unique<DecoratedLineEdit>(
        ":resources/icons/x.svg", "Horizontal coordinate", this);
    x_coordinate_->setValidator(
        std::make_unique<QIntValidator>(x_coordinate_.get()).release());

    y_coordinate_ = std::make_unique<DecoratedLineEdit>(
        ":resources/icons/y.svg", "Vertical coordinate", this);
    y_coordinate_->setValidator(
        std::make_unique<QIntValidator>(y_coordinate_.get()).release());

    layout->addWidget(x_coordinate_.get());
    layout->addWidget(y_coordinate_.get());

    setLayout(layout.release());

    QWidget::setVisible(false);
}


void GoToWidget::keyPressEvent(QKeyEvent* e)
{
    switch (e->key()) {
    case Qt::Key_Escape:
        toggle_visible();
        e->accept();

        return;
    case Qt::Key_Enter:
        [[fallthrough]];
    case Qt::Key_Return:
        toggle_visible();
        e->accept();
        Q_EMIT(go_to_requested(x_coordinate_->text().toFloat() + 0.5f,
                               y_coordinate_->text().toFloat() + 0.5f));
        return; // Let the completer do default behavior
    default:
        return;
    }
}


void GoToWidget::toggle_visible()
{
    const auto parent_widget = dynamic_cast<QWidget*>(parent());

    if (isVisible()) {
        hide();

        parent_widget->setFocus();
    } else {
        show();

        this->move(parent_widget->width() - this->width(),
                   parent_widget->height() - this->height());

        x_coordinate_->setFocus();
        x_coordinate_->selectAll();
    }
}


void GoToWidget::set_defaults(const float default_x,
                              const float default_y) const
{
    x_coordinate_->setText(QString::number(std::round(default_x - 0.5f)));
    y_coordinate_->setText(QString::number(std::round(default_y - 0.5f)));
}

} // namespace oid
