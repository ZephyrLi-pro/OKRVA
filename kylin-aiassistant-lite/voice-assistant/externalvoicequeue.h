#ifndef EXTERNALVOICEQUEUE_H
#define EXTERNALVOICEQUEUE_H

#include <QQueue>
#include <QString>

struct ExternalVoiceInput {
    quint64 id = 0;
    QString text;
    QString context;
    bool contextTruncated = false;
    bool contextReady = false;
};

// Keep user utterances in order; never replace an older question with a newer one.
class ExternalVoiceQueue {
public:
    enum Result { Accepted, Empty, TooLong, Full };
    static int capacity() { return 3; }
    static int maxTextChars() { return 2000; }
    Result append(ExternalVoiceInput input, quint64 *id = nullptr) {
        if (input.text.trimmed().isEmpty()) return Empty;
        if (input.text.size() > maxTextChars()) return TooLong;
        if (m_inputs.size() >= capacity()) return Full;
        input.id = ++m_nextId;
        if (id) *id = input.id;
        m_inputs.enqueue(input);
        return Accepted;
    }
    bool resolve(quint64 id, const QString &context, bool truncated) {
        for (ExternalVoiceInput &input : m_inputs) {
            if (input.id != id) continue;
            input.context = context;
            input.contextTruncated = truncated;
            input.contextReady = true;
            return true;
        }
        return false;
    }
    bool frontReady() const { return !m_inputs.isEmpty() && m_inputs.head().contextReady; }
    bool isEmpty() const { return m_inputs.isEmpty(); }
    int size() const { return m_inputs.size(); }
    ExternalVoiceInput take() { return m_inputs.dequeue(); }
    void clear() { m_inputs.clear(); }
private:
    QQueue<ExternalVoiceInput> m_inputs;
    quint64 m_nextId = 0;
};

#endif
