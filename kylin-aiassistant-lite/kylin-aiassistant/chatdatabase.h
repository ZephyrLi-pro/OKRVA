#ifndef CHATDATABASE_H
#define CHATDATABASE_H

#include <memory>
#include <QDateTime>
#include <QVariantList>
#include <QSqlDatabase>
#include <QQueue>

#include "global/kaglobal.h"

class ChatDatabase
{
public:
    ChatDatabase();
    ~ChatDatabase();

    bool insertOne(QString sessionID, int index, QString message, int chatType);
    bool insertOneWithModeType(QString sessionID, int index, QString message, int chatType, int modeType);
    bool insertOneWithModeTypeAndUuid(QString sessionID, int index, QString message, int chatType, int modeType, const QString& sessionUuid);
    bool insertOneWithRequestData(QString sessionID, int index, QString message, int chatType, QString requestData);
    bool updateOperateTime(QString sessionID, int index);
    bool updateOperateTime(QString sessionID, int index, qint64 operateTime);
    bool deleteBySessionID(QString sessionID);
    bool deleteBySessionIDAndIndex(QString sessionID, int index);
    bool updateFeedback(QString sessionID, int index, QString message);

    QStringList selectBySessionID(QString sessionID);
    QStringList selectBySessionIDAndChatType(QString sessionID, int chatType, int msgIndex);
    QString selectRequestDataBySessionIDAndIndex(QString sessionID, int index); // 获取指定消息的请求数据
    QStringList selectAll();
    QStringList selectAllDialogue();
    //会议助手
    bool insertMeetingSingleMsg(QString meetingID,int index , QString message);
    bool insertMeetingSingleInfo(QString meetingID, QString summary, QString fileName, QString filePath );
    QString selectHumanMessagesByMeetingName(QString meetingName);
    QJsonArray selectAllErrorMeeting();
    int getLastSaveMeetingID();
    int getMeetingIDFromName(const QString meetingName );
    bool deleteByMeetingID(const int  meetingID);
    bool deleteByMeetingName(const QString meetingName);
    QString formatMeetingTime(const QString meetingName);
    int selectMaxHistoryId();
    void updateMaxHistoryId(int history_id);
    static ChatDatabase* getChatDatabase();
    int selectLastRecordChatType(QString sessionID);
    int selectLastChatId(QString sessionID, int chatType);// 查询历史对话恢复最后的符合条件的id
    
    //语音通话
    int selectMaxVoiceHistoryId(); // 查询最大的语音通话id
    void updateMaxVoiceHistoryId(int voiceHistory_id); // 更新最大的语音通话id

    void updateIsCollect(QString sessionID, int isCollect);
    int selectHistoryCount(int isCollect);
    void deleteTheEarliestHistory();

    int getSessionModeType(const QString& sessionID);
    void setUnreadFlag(const QString& sessionID, int hasUnread);
    QString selectSessionUuidBySessionID(const QString& sessionID);
    QString selectSessionIDBySessionUuid(const QString& sessionUuid);
    void migrateTaskSessionId(const QString& oldSessionId, const QString& newSessionId, const QString& sessionUuid);

    // 把会话的来源标记写入 RECORD.session_uuid 列（该列已存在但历史未使用）。
    // 用于区分助手原生会话与外部导入会话（如 kybot cron），不引入新 schema。
    void markSessionSource(const QString& sessionID, const QString& source);
    bool sessionExists(const QString& sessionID);
    // 用显式时间戳更新 RECORD.operateTime，便于外部导入按真实发生时间排序。
    void setOperateTime(const QString& sessionID, int index, qint64 timestampSecs);

private:
    void initBase();
    void createTable();
    void createMeetingTable();
    bool columnExists(const QSqlDatabase &db, const QString &tableName, const QString &columnName);

private:
    class Private;
    std::shared_ptr<Private> d;
    QString activeMeetingID ;
    int activeMeetingIndex = 0;
};

#endif // CHATDATABASE_H
