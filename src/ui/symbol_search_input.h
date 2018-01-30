/*
 * Advanced search completer based on:
 * http://www.qtcentre.org/threads/23518
 */

#ifndef SYMBOL_SEARCH_INPUT_H_
#define SYMBOL_SEARCH_INPUT_H_

#include <QLineEdit>

#include "ui/symbol_completer.h"


class SymbolSearchInput : public QLineEdit
{
    Q_OBJECT

  public:
    SymbolSearchInput(QWidget* parent = 0);
    ~SymbolSearchInput();

    void set_completer(SymbolCompleter* completer_);

    SymbolCompleter* completer() const;

  protected:
    void keyPressEvent(QKeyEvent* e);

  private Q_SLOTS:
    void insert_completion(const QString& completion);

  private:
    SymbolCompleter* completer_;
};


#endif // SYMBOL_SEARCH_INPUT_H_
