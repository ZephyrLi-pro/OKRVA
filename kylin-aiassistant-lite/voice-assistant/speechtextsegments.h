#ifndef SPEECHTEXTSEGMENTS_H
#define SPEECHTEXTSEGMENTS_H

#include <QString>
#include <QStringList>

namespace SpeechText {
// End a TTS utterance only at an actual punctuation/paragraph boundary. In
// particular, a character budget or elapsed timer must never finish a phrase.
inline bool takeChunk(QString &pending, QString *out, bool final = false)
{
    if (!out || pending.isEmpty()) return false;
    int cut = -1;
    for (int i = 0; i < pending.size(); ++i) {
        const QChar ch = pending.at(i);
        const bool strong = QStringLiteral("。！？!?；;\n").contains(ch);
        bool period = ch == QLatin1Char('.');
        if (period) {
            // Preserve decimals, versions, domain names and file extensions.
            if (!final && i + 1 == pending.size()) period = false;
            else if (i > 0 && i + 1 < pending.size()
                     && pending.at(i - 1).isLetterOrNumber() && pending.at(i + 1).isLetterOrNumber()) period = false;
        }
        const bool clause = (ch == QChar(0xff0c) || ch == QLatin1Char(',')) && i + 1 >= 16;
        // Enumeration marks remain inside a phrase for natural list prosody.
        if (strong || period || clause) { cut = i + 1; break; }
    }
    if (cut < 0) {
        if (!final) return false;
        cut = pending.size();
    }
    while (cut < pending.size() && QStringLiteral("”’」』）》）\"')]").contains(pending.at(cut))) ++cut;
    *out = pending.left(cut).trimmed();
    pending.remove(0, cut);
    return true;
}

inline QStringList splitFinal(const QString &text)
{
    QString pending = text;
    QStringList result;
    QString part;
    while (takeChunk(pending, &part, true)) if (!part.isEmpty()) result.append(part);
    return result;
}
}
#endif
