#include "chatmessage.h"

KbChatMessage::KbChatMessage(QObject *parent)
    : QObject{parent}
{
    m_fileMap = new QMap<QString, QString>;
    m_chunkIndexMap = new QMap<int, int>;
    m_fileIdsMap = new QMap<int, QString>;
}
