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

#include "symbol_completer.h"
#include <QAbstractItemView>
#include <QDateTime>
#include <QSet>
#include <QString>
#include <fstream>
#include <sstream>

namespace oid
{

SymbolCompleter::SymbolCompleter(QObject* parent)
    : QCompleter{parent}
{
    setModel(&model_);
}


void SymbolCompleter::update(const QString& word)
{
    // #region agent log
    {
        std::ofstream log("/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
                          std::ios::app);
        QString wordStr = word;
        wordStr.replace("\"", "\\\"").replace("\n", "\\n");
        std::stringstream ss;
        ss << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\","
              "\"hypothesisId\":\"A\",\"location\":\"symbol_completer.cpp:38\","
              "\"message\":\"SymbolCompleter::update "
              "entry\",\"data\":{\"word\":\""
           << wordStr.toStdString() << "\",\"list_size\":" << list_.size()
           << ",\"widget_set\":" << (widget() != nullptr ? "true" : "false")
           << "},\"timestamp\":" << QDateTime::currentMSecsSinceEpoch()
           << "}\n";
        log << ss.str();
    }
    // #endregion

    const auto filtered = list_.filter(word, caseSensitivity());

    // #region agent log
    {
        std::ofstream log("/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
                          std::ios::app);
        std::stringstream ss;
        ss << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\","
              "\"hypothesisId\":\"C\",\"location\":\"symbol_completer.cpp:45\","
              "\"message\":\"after filtering\",\"data\":{\"filtered_count\":"
           << filtered.size()
           << ",\"completion_mode\":" << static_cast<int>(completionMode())
           << "},\"timestamp\":" << QDateTime::currentMSecsSinceEpoch()
           << "}\n";
        log << ss.str();
    }
    // #endregion

    model_.setStringList(filtered);
    word_ = word;
    complete();

    // #region agent log
    {
        std::ofstream log("/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
                          std::ios::app);
        std::stringstream ss;
        bool popupVisible = (popup() != nullptr && popup()->isVisible());
        ss << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\","
              "\"hypothesisId\":\"C\",\"location\":\"symbol_completer.cpp:58\","
              "\"message\":\"after complete()\",\"data\":{\"popup_visible\":"
           << (popupVisible ? "true" : "false")
           << ",\"completion_count\":" << completionCount()
           << "},\"timestamp\":" << QDateTime::currentMSecsSinceEpoch()
           << "}\n";
        log << ss.str();
    }
    // #endregion
}


void SymbolCompleter::update_symbol_list(const QStringList& symbols)
{
    // #region agent log
    {
        std::ofstream log("/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
                          std::ios::app);
        QString first5 = symbols.mid(0, 5).join(", ");
        first5.replace("\"", "\\\"").replace("\n", "\\n");
        std::stringstream ss;
        ss << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\","
              "\"hypothesisId\":\"A\",\"location\":\"symbol_completer.cpp:47\","
              "\"message\":\"update_symbol_list "
              "called\",\"data\":{\"symbol_count\":"
           << symbols.size() << ",\"first_5_symbols\":\""
           << first5.toStdString()
           << "\"},\"timestamp\":" << QDateTime::currentMSecsSinceEpoch()
           << "}\n";
        log << ss.str();
    }
    // #endregion

    // Deduplicate symbols using QSet to remove any duplicates, then sort
    // to maintain consistent order
    QSet<QString> unique_symbols;
    for (const auto& symbol : symbols) {
        unique_symbols.insert(symbol);
    }
    list_ = QStringList(unique_symbols.begin(), unique_symbols.end());
    list_.sort();

    // #region agent log
    {
        std::ofstream log("/Users/bruno/ws/OpenImageDebugger/.cursor/debug.log",
                          std::ios::app);
        QString first5 = list_.mid(0, 5).join(", ");
        first5.replace("\"", "\\\"").replace("\n", "\\n");
        std::stringstream ss;
        ss << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\","
              "\"hypothesisId\":\"A\",\"location\":\"symbol_completer.cpp:"
              "100\",\"message\":\"update_symbol_list after "
              "deduplication\",\"data\":{\"symbol_count\":"
           << list_.size() << ",\"first_5_symbols\":\"" << first5.toStdString()
           << "\"},\"timestamp\":" << QDateTime::currentMSecsSinceEpoch()
           << "}\n";
        log << ss.str();
    }
    // #endregion
}


const QString& SymbolCompleter::word() const
{
    return word_;
}

} // namespace oid
