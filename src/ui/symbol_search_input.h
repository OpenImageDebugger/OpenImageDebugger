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

#ifndef SYMBOL_SEARCH_INPUT_H_
#define SYMBOL_SEARCH_INPUT_H_

#include <cassert>
#include <functional>
#include <optional>

#include <QLineEdit>

#include "ui/symbol_completer.h"

namespace oid
{

class SymbolSearchInput final : public QLineEdit
{
    Q_OBJECT

  public:
    explicit SymbolSearchInput(QWidget* parent = nullptr);
    ~SymbolSearchInput() override;

    void set_completer(SymbolCompleter& completer);

    [[nodiscard]] SymbolCompleter* symbolCompleter() const;

  protected:
    void keyPressEvent(QKeyEvent* e) override;

  private Q_SLOTS:
    void insert_completion(const QString& completion);

  private:
    std::optional<std::reference_wrapper<SymbolCompleter>>
        completer_{}; // Always set via set_completer() before use

    [[nodiscard]] SymbolCompleter& completer() const
    {
        assert(completer_.has_value() &&
               "completer_ must be set via set_completer() before use");
        return completer_->get();
    }
};

} // namespace oid

#endif // SYMBOL_SEARCH_INPUT_H_
