#include "symbol_completer.h"


SymbolCompleter::SymbolCompleter(const QStringList& words,
                                 QObject* parent) :
    QCompleter(parent), m_list(words), m_model()
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

QString SymbolCompleter::word()
{
    return m_word;
}
