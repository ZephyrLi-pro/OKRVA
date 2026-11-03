#include "plaintextsanitizer.h"

#include <QRegularExpression>

namespace {
QString simplifyWhitespace(QString text)
{
    text.replace(QRegularExpression("[ \\t]+"), " ");
    text.replace(QRegularExpression("\\n{3,}"), "\n\n");
    return text.trimmed();
}
}

QString stripMarkdownForDisplay(const QString &input)
{
    QString text = input;

    text.replace(QRegularExpression("```[\\s\\S]*?```"), "");
    text.replace(QRegularExpression("`([^`]+)`"), "\\1");
    text.replace(QRegularExpression("!\\[([^\\]]*)\\]\\([^\\)]*\\)"), "\\1");
    text.replace(QRegularExpression("\\[([^\\]]+)\\]\\([^\\)]*\\)"), "\\1");
    text.replace(QRegularExpression("(?m)^\\s{0,3}(#{1,6})\\s+"), "");
    text.replace(QRegularExpression("(?m)^\\s{0,3}[-*_]{3,}\\s*$"), "");
    text.replace(QRegularExpression("(?m)^\\s{0,3}[-*+]\\s+"), "");
    text.replace(QRegularExpression("(?m)^\\s*\\d+\\.\\s+"), "");
    text.replace(QRegularExpression("\\*\\*([^*]+)\\*\\*"), "\\1");
    text.replace(QRegularExpression("__([^_]+)__"), "\\1");
    text.replace(QRegularExpression("\\*([^*]+)\\*"), "\\1");
    text.replace(QRegularExpression("_([^_]+)_"), "\\1");
    text.replace(QRegularExpression("~~([^~]+)~~"), "\\1");

    return simplifyWhitespace(text);
}

QString stripModelThinkingForDisplay(const QString &input)
{
    QString text = input;

    text.replace(QRegularExpression("<think>[\\s\\S]*?</think>",
                                    QRegularExpression::CaseInsensitiveOption),
                 "");
    const QRegularExpression openThink(QStringLiteral("<think>"),
                                       QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch openMatch = openThink.match(text);
    if (openMatch.hasMatch()) {
        text.truncate(openMatch.capturedStart());
    }
    text.replace(QRegularExpression("</think>", QRegularExpression::CaseInsensitiveOption), "");

    return simplifyWhitespace(text);
}
