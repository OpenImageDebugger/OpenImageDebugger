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

#ifndef GO_TO_WIDGET_H_
#define GO_TO_WIDGET_H_

#include <QLineEdit>
#include <QWidget>


class GoToWidget : public QWidget
{
    Q_OBJECT

  public:
    explicit GoToWidget(QWidget* parent = nullptr);
    void toggle_visible();
    void set_defaults(float default_x, float default_y);

  Q_SIGNALS:
    void go_to_requested(float x, float y);

  public Q_SLOTS:

  protected:
    void keyPressEvent(QKeyEvent* e);


  private:
    QLineEdit* x_coordinate_;
    QLineEdit* y_coordinate_;
};

#endif // GO_TO_WIDGET_H_
