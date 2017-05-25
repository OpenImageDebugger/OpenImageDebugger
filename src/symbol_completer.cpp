#include "symbol_completer.h"


SymbolCompleter::SymbolCompleter(QObject* parent) :
    QCompleter(parent), m_list(), m_model()
{
    setModel(&m_model);
}

void SymbolCompleter::update(QString word)
{
    QStringList filtered = m_list.filter(word, caseSensitivity());
    m_model.setStringList(filtered);
    m_word = word;
    complete();
}

void SymbolCompleter::updateSymbolList(QStringList& symbols) {
    m_list = symbols;
}

QString SymbolCompleter::word()
{
    return m_word;
}
