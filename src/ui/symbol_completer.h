#ifndef SYMBOL_COMPLETER_H
#define SYMBOL_COMPLETER_H

#include <QStringList>
#include <QStringListModel>
#include <QCompleter>

class SymbolCompleter : public QCompleter
{
    Q_OBJECT

public:
    SymbolCompleter(QObject * parent = nullptr);

    void update(QString word);

    void updateSymbolList(QStringList &symbols);

    QString word();

private:
    QStringList m_list;
    QStringListModel m_model;
    QString m_word;
};


#endif // SYMBOL_COMPLETER_H
