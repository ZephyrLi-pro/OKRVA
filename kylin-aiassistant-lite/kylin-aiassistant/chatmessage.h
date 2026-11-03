#ifndef CHATMESSAGE_H
#define CHATMESSAGE_H

#include <QObject>
#include <QMap>

class KbChatMessage : public QObject
{
    Q_OBJECT
public:
    explicit KbChatMessage(QObject *parent = nullptr);

public:
    QString m_content = "";
    bool m_isEnd = false;
    QString m_model;
    QString m_feedbackURL;
    QMap<QString, QString> *m_fileMap = nullptr;
    QMap<int, QString> *m_fileIdsMap = nullptr;
    QMap<int, int> *m_chunkIndexMap = nullptr;
    QStringList m_fileIds;
    QStringList m_fileContents;


    QString m_reasonContent = "";
    bool m_isReasoningEnd = false;
    // QString feedbackURL;



};

#endif // CHATMESSAGE_H
