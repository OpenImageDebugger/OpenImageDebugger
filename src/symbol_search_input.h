#ifndef SYMBOL_SEARCH_INPUT_H
#define SYMBOL_SEARCH_INPUT_H

#include <QLineEdit>
#include "src/symbol_completer.h"

class SymbolSearchInput : public QLineEdit
{
    Q_OBJECT

public:
    SymbolSearchInput(QWidget *parent = 0);
    ~SymbolSearchInput();

    void setCompleter(SymbolCompleter *completer_);
    SymbolCompleter *completer() const;

protected:
    void keyPressEvent(QKeyEvent *e);

private Q_SLOTS:
    void insertCompletion(const QString &completion);

private:
    SymbolCompleter *completer_;
};


#endif // SYMBOL_SEARCH_INPUT_H
