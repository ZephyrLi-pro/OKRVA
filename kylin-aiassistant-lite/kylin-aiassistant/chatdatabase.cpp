/*
 * Copyright (C) 2020, KylinSoft Co., Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "chatdatabase.h"

#include <QDir>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlDatabase>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QJsonArray>
#include <QMap>
#include <ukui-log4qt.h>
#include <QDateTime>
#include <QFileInfo>

#define CREATE_TABLE_RECORD_SQL "CREATE TABLE IF NOT EXISTS RECORD(ID INT AUTO_INCREMENT, sessionID VARCHAR(36), msgIndex INT, message TEXT, operateTime INTEGER,PRIMARY KEY (ID))"
#define CREATE_TABLE_MEETING_RECORD_SQL "CREATE TABLE IF NOT EXISTS MEETINGRECORD(ID INTEGER PRIMARY KEY AUTOINCREMENT,meetingID VARCHAR(36), msgIndex INT, message TEXT,summary  TEXT,fileName TEXT,filePath TEXT)"
#define CREATE_TABLE_HISTORY_ID_SQL "CREATE TABLE IF NOT EXISTS HISTORY_ID(ID INT AUTO_INCREMENT, history_id INT, PRIMARY KEY (ID))"
#define ALTER_TABLE_ADD_RECORD_COLUMN_CHAT_TYPE_SQL "ALTER TABLE RECORD ADD COLUMN chat_type INT DEFAULT 0"
#define ALTER_TABLE_ADD_RECORD_COLUMN_IS_COLLECT_SQL "ALTER TABLE RECORD ADD COLUMN is_collect INT DEFAULT 0"
#define ALTER_TABLE_ADD_RECORD_COLUMN_REQUEST_DATA_SQL "ALTER TABLE RECORD ADD COLUMN request_data TEXT DEFAULT NULL"
#define ALTER_TABLE_ADD_RECORD_COLUMN_MODE_TYPE_SQL "ALTER TABLE RECORD ADD COLUMN mode_type INT DEFAULT 0"
#define ALTER_TABLE_ADD_RECORD_COLUMN_SESSION_UUID_SQL "ALTER TABLE RECORD ADD COLUMN session_uuid VARCHAR(64) DEFAULT NULL"
#define ALTER_TABLE_ADD_RECORD_COLUMN_HAS_UNREAD_SQL "ALTER TABLE RECORD ADD COLUMN has_unread INTEGER DEFAULT 0"
#define ALTER_TABLE_ADD_HISTORY_ID_COLUMN_MODE_TYPE_SQL "ALTER TABLE HISTORY_ID ADD COLUMN mode_type INT DEFAULT 0"
#define INSERT_HISTORY_ID_SQL "INSERT OR IGNORE INTO HISTORY_ID(ID, history_id, mode_type) VALUES (1, 0, 0), (2, 0, 0), (3, 0, 0), (4, 0, 1)"

static ChatDatabase *g_chatDatabase = nullptr;

class ChatDatabase::Private
{
public:
    bool is_open;
    QSqlDatabase base;
};

ChatDatabase::ChatDatabase():
    d(std::make_shared<ChatDatabase::Private>())
{
    initBase();
    createTable();
    createMeetingTable();
}

ChatDatabase::~ChatDatabase()
{
    if (d->is_open)
        d->base.close();
}

bool ChatDatabase::insertOne(QString sessionID, int index, QString message, int chatType)
{
    // 当历史记录大于500条时，先删除，在插入
    if(index == 0){
        int historyCount = selectHistoryCount(0);
        if(historyCount >= 500){
            deleteTheEarliestHistory();
        }
    }
    if (!d->is_open)
        return false;
    QSqlQuery query(d->base);
    QString str = ("INSERT INTO RECORD(sessionID, msgIndex, message, chat_type, operateTime) VALUES (?, ?, ?, ?, ?)");

    // 获取当前时间戳
    qint64 timestamp = QDateTime::currentDateTime().toSecsSinceEpoch();
    query.prepare(str);
    query.addBindValue(sessionID);
    query.addBindValue(index);
    query.addBindValue(message);
    query.addBindValue(chatType);
    query.addBindValue(timestamp);

    KyInfo() << "message " << message;

    bool flag = query.exec();
    if(!flag){
        KyInfo() << "Error: insert record error";
        return false;
    }

    return true;
}

bool ChatDatabase::insertOneWithModeType(QString sessionID, int index, QString message, int chatType, int modeType)
{
    if(index == 0){
        int historyCount = selectHistoryCount(0);
        if(historyCount >= 500){
            deleteTheEarliestHistory();
        }
    }
    if (!d->is_open)
        return false;
    QSqlQuery query(d->base);
    QString str = ("INSERT INTO RECORD(sessionID, msgIndex, message, chat_type, mode_type, operateTime) VALUES (?, ?, ?, ?, ?, ?)");

    qint64 timestamp = QDateTime::currentDateTime().toSecsSinceEpoch();
    query.prepare(str);
    query.addBindValue(sessionID);
    query.addBindValue(index);
    query.addBindValue(message);
    query.addBindValue(chatType);
    query.addBindValue(modeType);
    query.addBindValue(timestamp);

    bool flag = query.exec();
    if(!flag){
        KyInfo() << "Error: insert record with mode_type error";
        return false;
    }
    return true;
}

bool ChatDatabase::insertOneWithModeTypeAndUuid(QString sessionID, int index, QString message, int chatType, int modeType, const QString& sessionUuid)
{
    if(index == 0){
        int historyCount = selectHistoryCount(0);
        if(historyCount >= 500){
            deleteTheEarliestHistory();
        }
    }
    if (!d->is_open)
        return false;
    QSqlQuery query(d->base);
    QString str = ("INSERT INTO RECORD(sessionID, msgIndex, message, chat_type, mode_type, operateTime, session_uuid) VALUES (?, ?, ?, ?, ?, ?, ?)");

    qint64 timestamp = QDateTime::currentDateTime().toSecsSinceEpoch();
    query.prepare(str);
    query.addBindValue(sessionID);
    query.addBindValue(index);
    query.addBindValue(message);
    query.addBindValue(chatType);
    query.addBindValue(modeType);
    query.addBindValue(timestamp);
    query.addBindValue(sessionUuid);

    bool flag = query.exec();
    if(!flag){
        KyInfo() << "Error: insert record with mode_type and uuid error";
        return false;
    }
    return true;
}

// 保留请求信息，包含文件内容
bool ChatDatabase::insertOneWithRequestData(QString sessionID, int index, QString message, int chatType, QString requestData)
{
    // 当历史记录大于500条时，先删除，在插入
    if(index == 0){
        int historyCount = selectHistoryCount(0);
        if(historyCount >= 500){
            deleteTheEarliestHistory();
        }
    }
    if (!d->is_open)
        return false;
    QSqlQuery query(d->base);
    QString str = ("INSERT INTO RECORD(sessionID, msgIndex, message, chat_type, operateTime, request_data) VALUES (?, ?, ?, ?, ?, ?)");

    // 获取当前时间戳
    qint64 timestamp = QDateTime::currentDateTime().toSecsSinceEpoch();
    query.prepare(str);
    query.addBindValue(sessionID);
    query.addBindValue(index);
    query.addBindValue(message);
    query.addBindValue(chatType);
    query.addBindValue(timestamp);
    query.addBindValue(requestData);

    bool flag = query.exec();
    if(!flag){
        KyInfo() << "Error: insert record with request_data error";
        return false;
    }
    return true;
}

bool ChatDatabase::updateOperateTime(QString sessionID, int index)
{
    Q_UNUSED(index)
    if (!d->is_open)
        return false;
    QSqlQuery query(d->base);
    QString str = ("UPDATE RECORD SET operateTime = ? WHERE sessionID = ? and msgIndex = ?");

    // 获取当前时间戳
    qint64 timestamp = QDateTime::currentDateTime().toSecsSinceEpoch();

    query.prepare(str);
    query.addBindValue(timestamp);
    query.addBindValue(sessionID);
    query.addBindValue(0);

    bool flag = query.exec();
    if(!flag){
        KyInfo() << "Error: UPDATE record operateTime error";
        return false;
    }
    return true;
}

bool ChatDatabase::updateOperateTime(QString sessionID, int index, qint64 operateTime)
{
    if (!d->is_open)
        return false;
    QSqlQuery query(d->base);
    QString str = ("UPDATE RECORD SET operateTime = ? WHERE sessionID = ? and msgIndex = ?");

    qint64 timestamp = operateTime > 0 ? operateTime : QDateTime::currentDateTime().toSecsSinceEpoch();

    query.prepare(str);
    query.addBindValue(timestamp);
    query.addBindValue(sessionID);
    query.addBindValue(index);

    bool flag = query.exec();
    if(!flag){
        KyInfo() << "Error: UPDATE record operateTime error";
        return false;
    }
    return true;
}

bool ChatDatabase::deleteBySessionID(QString sessionID)
{
    if (!d->is_open)
        return false;
    QSqlQuery query(d->base);
    QString str = ("DELETE FROM RECORD WHERE sessionID = ?");
    query.prepare(str);
    query.addBindValue(sessionID);
    bool flag = query.exec();
    if(!flag){
        KyInfo() << "Error: delete record error";
        return false;
    }

    return true;
}

bool ChatDatabase::deleteBySessionIDAndIndex(QString sessionID, int index)
{
    if (!d->is_open)
        return false;
    QSqlQuery query(d->base);
    d->base.transaction();

    // 删除
    QString str = ("DELETE FROM RECORD WHERE sessionID = ? AND msgIndex = ?");
    query.prepare(str);
    query.addBindValue(sessionID);
    query.addBindValue(index);
    bool flag = query.exec();
    if(!flag){
        KyInfo() << "Error: delete record by sessionID and index error";
        d->base.rollback();
        return false;
    }

    // 更新索引
    QString updateStr = ("UPDATE RECORD SET msgIndex = msgIndex - 1 WHERE sessionID = ? AND msgIndex > ?");
    query.prepare(updateStr);
    query.addBindValue(sessionID);
    query.addBindValue(index);
    bool updateFlag = query.exec();
    if(!updateFlag) {
        KyInfo() << "Error: update subsequent indices error";
        d->base.rollback();
        return false;
    }

    if(!d->base.commit()) {
        KyInfo() << "Error: commit transaction failed";
        d->base.rollback();
        return false;
    }

    return true;
}

bool ChatDatabase::updateFeedback(QString sessionID, int index, QString message)
{
    if (!d->is_open)
        return false;
    QSqlQuery query(d->base);
    QString str = ("UPDATE RECORD SET message = ? WHERE sessionID = ? and msgIndex = ?");

    query.prepare(str);
    query.addBindValue(message);
    query.addBindValue(sessionID);
    query.addBindValue(index);

    bool flag = query.exec();
    if(!flag){
        KyInfo() << "Error: UPDATE record message error";
        return false;
    }
    return true;
}

QStringList ChatDatabase::selectBySessionID(QString sessionID)
{
    if (!d->is_open)
        return {};
    QStringList result;
    QSqlQuery query(d->base);
    QString str = ("SELECT message FROM RECORD WHERE sessionID = ? ORDER BY msgIndex ASC");
    query.prepare(str);
    query.addBindValue(sessionID);
    bool flag = query.exec();
    if(!flag){
        KyInfo() << "Error: select record error";
        return {};
    }
    // 遍历查询结果
    while(query.next()){
        QString msgStr = query.value(0).toString();
        QJsonDocument jsonDoc;
        QJsonParseError jsonErr;

        // 解析原始JSON
        jsonDoc = QJsonDocument::fromJson(msgStr.toUtf8(), &jsonErr);
        // 解析失败/非对象格式，直接保留原内容
        if (jsonErr.error != QJsonParseError::NoError || !jsonDoc.isObject()) {
            result.append(msgStr);
            continue;
        }

        // 保留原始所有字段
        QJsonObject newRootObj = jsonDoc.object();

        // 判断是否包含目标四个字段
        bool hasAllFields = newRootObj.contains("documentName")
                && newRootObj.contains("documentSize")
                && newRootObj.contains("documentIcon")
                && newRootObj.contains("documentPath");

        if (hasAllFields) {
            // 字段齐全，构建documents数组项
            QJsonObject docItemObj;
            // 字段映射：原字段 -> 新字段
            docItemObj["childCount"] = 0;          // 固定默认值
            docItemObj["icon"] = newRootObj["documentIcon"].toString();
            docItemObj["isDir"] = false;           // 固定默认值
            docItemObj["name"] = newRootObj["documentName"].toString();
            docItemObj["path"] = newRootObj["documentPath"].toString();
            docItemObj["size"] = newRootObj["documentSize"].toString();
            docItemObj["uuid"] = "";               // 固定默认值

            // 构建documents数组
            QJsonArray docArray;
            docArray.append(docItemObj);

            // 核心修改：将数组序列化为JSON字符串（紧凑格式）
            QJsonDocument docArrayDoc(docArray);
            QString docArrayStr = "";
            if(newRootObj["documentName"].toString() != ""){
                docArrayStr = docArrayDoc.toJson(QJsonDocument::Compact);
            }else{

            }
            // 新增/覆盖documents字段，值为数组的字符串形式
            newRootObj["documents"] = docArrayStr;
            newRootObj["targetDocument"] = "";
        }

        // 序列化为最终的JSON字符串（保留所有原始字段 + 新增documents字符串字段）
        QJsonDocument newJsonDoc(newRootObj);
        QString newMsgStr = newJsonDoc.toJson(QJsonDocument::Compact);
        result.append(newMsgStr);
    }
    return result;
}

QStringList ChatDatabase::selectBySessionIDAndChatType(QString sessionID, int chatType, int msgIndex)
{
    if (!d->is_open)
            return {};
        QStringList result;
        QSqlQuery query(d->base);
        QString str = "";
        if(msgIndex == 0){
            str = ("SELECT message FROM RECORD WHERE sessionID = ? and chat_type = ? and msgIndex >= ?");
        }else{
            str = ("SELECT message FROM RECORD WHERE sessionID = ? and chat_type = ? and msgIndex > ?");
        }
        query.prepare(str);
        query.addBindValue(sessionID);
        query.addBindValue(chatType);
        query.addBindValue(msgIndex);
        bool flag = query.exec();
        if(!flag){
            KyInfo() << "Error: selectBySessionIDAndChatType error";
            return {};
        }
        while(query.next()){
            QString str = query.value(0).toString();
            result.append(str);
        }
        return result;
}

QString ChatDatabase::selectRequestDataBySessionIDAndIndex(QString sessionID, int index)
{
    if (!d->is_open)
        return QString();
    QSqlQuery query(d->base);
    QString str = ("SELECT request_data FROM RECORD WHERE sessionID = ? and msgIndex = ?");
    query.prepare(str);
    query.addBindValue(sessionID);
    query.addBindValue(index);
    bool flag = query.exec();
    if(!flag){
        KyInfo() << "Error: selectRequestDataBySessionIDAndIndex error";
        return QString();
    }
    if(query.next()){
        return query.value(0).toString();
    }
    return QString();
}

QStringList ChatDatabase::selectAll()
{
    if (!d->is_open)
        return {};
    QStringList result;
    QSqlQuery query(d->base);
    bool flag = query.exec(QString("SELECT message FROM RECORD"));
    if(!flag){
        KyInfo() << "Error: select record error";
        return {};
    }
    while(query.next()){
        QString str = query.value(0).toString();
        result.append(str);
    }
    return result;
}

QStringList ChatDatabase::selectAllDialogue()
{
    if (!d->is_open)
        return {};

    QSqlQuery query(d->base);
    QStringList dialogueList;
    QSet<QString> addedSessions;

    // 查询所有记录，准备遍历
    bool flag = query.exec(QString("SELECT sessionID, msgIndex, message, operateTime, chat_type, mode_type, is_collect, has_unread FROM RECORD where msgIndex = 0 ORDER BY operateTime desc"));
    if (!flag) {
        KyInfo() << "Error: select record error";
        return {};
    }
    // 遍历每一项，检查 msgIndex 是否为 0，且 sessionID 是否已添加
    while (query.next()) {
        QString sessionID = query.value("sessionID").toString();
        int msgIndex = query.value("msgIndex").toInt();
        QString messageJson = query.value("message").toString();
        int chatType = query.value("chat_type").toInt();
        int modeType = query.value("mode_type").toInt();
        if (sessionID.startsWith(QStringLiteral("v_")) && chatType == 0) {
            modeType = 1;
        }
        int isCollect = query.value("is_collect").toInt();
        int hasUnread = query.value("has_unread").toInt();
        qint64 operateTimeStamp = query.value("operateTime").toLongLong();

        if (msgIndex == 0 && !addedSessions.contains(sessionID)) {
            // 解析 messageJson 字符串以提取 msgHead 和 message 字段
            QJsonDocument jsonDoc = QJsonDocument::fromJson(messageJson.toUtf8());
            QJsonObject jsonObj = jsonDoc.object();
            QString msgHead = jsonObj.value("msgHead").toString();
            QString promptIconPath = jsonObj.value("promptIconPath").toString();
            QString message = jsonObj.value("message").toString();
            QString imagePath = jsonObj.value("imagePath").toString();//图片路径
            message = message.replace("\n", "\\n");
            message = message.replace("\t", "\\t");
            message = message.replace("\"", "\\\"");

            // ========== 核心修改：解析documents字段（JSON数组字符串） ==========
            QString documentsStr = jsonObj.value("documents").toString(); // 提取documents字符串
            QString documentName = ""; // 存储解析后的文档名称
            QJsonArray documentArray;  // 存储解析后的documents数组

            // 解析documents字符串为JSON数组
            if (!documentsStr.isEmpty()) {
                QJsonDocument docJsonDoc = QJsonDocument::fromJson(documentsStr.toUtf8());
                if (docJsonDoc.isArray()) {
                    documentArray = docJsonDoc.array();
                    // 解析第一个文档的名称（根据业务需求，可遍历所有文档）
                    if (!documentArray.isEmpty()) {
                        QJsonObject firstDocObj = documentArray.first().toObject();
                        documentName = firstDocObj.value("name").toString(); // 提取文档名称（如"1.txt"）
                        KyInfo() << "解析到文档名称: " << documentName;
                    }
                } else {
                    KyInfo() << "documents不是合法的JSON数组: " << documentsStr;
                }
            }

            // 若文档名称不为空，并且消息提为空则展示文件名称
            if(message == "" && documentName != ""){
                message = documentName;
            }
            // 若图片路径不为空，并且消息提为空则展示图片路径
            if(message == "" && imagePath != ""){
                QFileInfo fileInfo(imagePath);
                QString fileName = fileInfo.fileName();
                if(fileName != ""){
                    message = fileName;
                }else{
                    message = imagePath;
                }
            }
            // 格式化时间戳为"2025/09/08 15:15"
            QDateTime operateTime = QDateTime::fromSecsSinceEpoch(operateTimeStamp); // 假设是秒级时间戳
            // 如果是毫秒级时间戳，使用 fromMSecsSinceEpoch
            // QDateTime operateTime = QDateTime::fromMSecsSinceEpoch(operateTimeStamp);
            QString formattedTime = operateTime.toString("yyyy/MM/dd HH:mm");
            // 构建 JSON 字符串
            QString dialogueItem = QString("{\"sessionID\":\"%1\",\"msgHead\":\"%2\",\"message\":\"%3\",\"promptIconPath\":\"%4\",\"chatType\":%5,\"isCollect\":%6,\"modeType\":%7,\"hasUnread\":%8,\"isReplying\":false,\"operateTime\":\"%9\"}").arg(sessionID).arg(msgHead).arg(message).arg(promptIconPath).arg(chatType).arg(isCollect).arg(modeType).arg(hasUnread).arg(formattedTime);
            dialogueList.append(dialogueItem);
            addedSessions.insert(sessionID);  // 记录已添加的 sessionID
        }
    }

    return dialogueList;
}
bool ChatDatabase::insertMeetingSingleMsg(QString meetingID, int index, QString message)
{
//    KyInfo() << "本次插入数据库的会议记录消息为:"<<meetingID << index << message;
    if(!d->is_open)
        return false;
    QSqlQuery query(d->base);

    //ID和索引相同则更新数据
    if(activeMeetingID == meetingID && activeMeetingIndex == index)
    {
        KyInfo() << "本次更新数据库的会议记录消息为:"<<meetingID << index << message;
        QString str = ("UPDATE MEETINGRECORD SET message = ? WHERE meetingID = ? and msgIndex = ?");
        query.prepare(str);
        query.addBindValue(message);
        query.addBindValue(meetingID);
        query.addBindValue(index);
        bool flag = query.exec();
        if(!flag){
            KyInfo() << "Error: meeting msg update error";
            return false;
        }
        return true;
    }
    else//否则插入新数据
    {
        KyInfo() << "本次插入数据库的会议记录消息为:"<<meetingID << index << message;
        QString str = ("INSERT  INTO MEETINGRECORD(meetingID,msgIndex,message,summary,fileName,filePath) VALUES (?, ?, ?, ?, ?, ?)");
        query.prepare(str);
        query.addBindValue(meetingID);
        query.addBindValue(index);
        query.addBindValue(message);
        query.addBindValue("");
        query.addBindValue("");
        query.addBindValue("");

        bool flag = query.exec();
        if(!flag){
            KyInfo() << "Error: meeting msg insert error";
            return false;
        }
        activeMeetingID = meetingID;
        activeMeetingIndex = index;
        return true;
    }


}

bool ChatDatabase::insertMeetingSingleInfo(QString meetingID, QString summary, QString fileName, QString filePath)
{
    KyInfo() << "本次插入数据库的会议记录信息为:"<< meetingID  << summary << fileName << filePath;
    if(!d->is_open)
        return false;
    QSqlQuery query(d->base);
    QString str = ("INSERT INTO MEETINGRECORD(meetingID,msgIndex,message,summary,fileName,filePath) VALUES (?, ?, ?, ?, ?, ?)");

    query.prepare(str);
    query.addBindValue(meetingID);
    query.addBindValue(-1);
    query.addBindValue("");
    query.addBindValue(summary);
    query.addBindValue(fileName);
    query.addBindValue(filePath);

    bool flag = query.exec();
    if(!flag){
        KyInfo() << "Error: meeting msg insert error:" << query.lastError();
        return false;
    }
    return true;
}

QString ChatDatabase::selectHumanMessagesByMeetingName(QString meetingName)
{
    if (!d->is_open)
           return {};
       QJsonArray targetArray;
       QSqlQuery query(d->base);
       QString meetingID;
       QString str = ("SELECT meetingID FROM MEETINGRECORD WHERE fileName = ? AND msgIndex = -1");
       query.prepare(str);
       query.addBindValue(meetingName);
       bool flag = query.exec();
       if(!flag){
           KyInfo() << "Error: select meetingRecord error";
           return {};
       }
       while(query.next()){
           meetingID = query.value(0).toString();
           if(!meetingID.isEmpty()){
               QString str2 = ("SELECT message FROM MEETINGRECORD WHERE meetingID = ?");
               QSqlQuery query2(d->base);
               query2.prepare(str2);
               query2.addBindValue(meetingID);
               bool flag2 = query2.exec();
               if(!flag2){
                   KyInfo() << "Error:select meetingRecord message error";
                   return {};
               }
               else{
                   while(query2.next()){
                       QString str = query2.value(0).toString();
                       if(!str.isEmpty()){
                           KyInfo() << "message:"<<str;
                           // 将 JSON 字符串解析为 QJsonDocument
                           QJsonDocument doc = QJsonDocument::fromJson(str.toUtf8());
                           //json解析
                           if(doc.isArray())
                           {
                               QJsonArray jsonArray = doc.array();  // 解析为 QJsonArray
                               QJsonValue value = jsonArray.at(0);
                               if (value.isObject()){
                                   QJsonObject jsonObj = value.toObject();
                                   // 访问键值对
                                   QString role = jsonObj["role"].toString();
                                   QString name = jsonObj["name"].toString();
                                   QString start = jsonObj["start"].toString();
                                   QString end = jsonObj["end"].toString();
                                   QString content = jsonObj["content"].toString();
                                   if(role == "human"){ //搜集人员信息
                                        targetArray.append(jsonObj);
                                   }
                               }
                               }
                           }

                       }
                   }

               }
           }
       QJsonDocument doc(targetArray);
       QString jsonString = doc.toJson(QJsonDocument::Indented);  // 使用 Indented 格式（美化输出）

       return jsonString;
}


QJsonArray ChatDatabase::selectAllErrorMeeting()
{
    if (!d->is_open)
        return {};

    QSqlQuery query(d->base);
    QString str = ("SELECT * FROM MEETINGRECORD WHERE msgIndex = -1 AND fileName IS NOT NULL");
    query.prepare(str);

    QJsonArray jsonArray;

    if (!query.exec()) {
            KyInfo() << "Query error: " << query.lastError().text();
            return jsonArray;
        } else {
            while (query.next()) {
                int id = query.value("ID").toInt(); // 假设有 ID 列
                QString fileName = query.value("fileName").toString();
                QString filePath = query.value("filePath").toString();

                QJsonObject jsonObject;
                QString index =QString::number(-1);
                jsonObject["index"] = index;
                jsonObject["path"] = filePath;
                jsonObject["name"] = fileName.mid(0,23);
                jsonObject["time"] = formatMeetingTime(fileName.mid(0,23));
                jsonObject["generating"] = "false";
                jsonArray.append(jsonObject);
            }


            // 打印结果
            //KyInfo() << jsonArray;
    }
    return jsonArray;

}

int ChatDatabase::getLastSaveMeetingID()
{
    if(!d->is_open)
        return false;
    QSqlQuery query(d->base);
    QString str = ("SELECT * FROM MEETINGRECORD ORDER BY ID DESC LIMIT 1");
    if (!query.exec()) {
           KyInfo() << "Query error: " << query.lastError().text();
           return -1;
       }
    if(query.next()){
        int meetingID = query.value("meetingID").toInt();
        if(meetingID != -1)
        {
            return meetingID;
        }
    }
}

int ChatDatabase::getMeetingIDFromName(const QString meetingName)
{
    if (!d->is_open)
        return {};
    int resultID = -1;
    QSqlQuery query(d->base);
    QString str = ("SELECT meetingID FROM MEETINGRECORD WHERE fileName = ?");
    query.prepare(str);
    query.addBindValue(meetingName);
    bool flag = query.exec();
    if(!flag){
        KyInfo() << "Error: select meetingRecord error";
        return resultID;
    }
    while(query.next()){
        int  id = query.value(0).toInt();
        resultID = id;
    }
    return resultID;
}

bool ChatDatabase::deleteByMeetingID(const int meetingID)
{
    if (!d->is_open)
        return false;
    QSqlQuery query(d->base);
    QString str = ("DELETE FROM MEETINGRECORD WHERE meetingID = ?");
    query.prepare(str);
    query.addBindValue(meetingID);
    bool flag = query.exec();
    if(!flag){
        KyInfo() << "Error: delete meetingrecord error";
        return false;
    }

    return true;
}

bool ChatDatabase::deleteByMeetingName(const QString meetingName)
{
    if (!d->is_open)
        return false;
    QSqlQuery query(d->base);
    QString str = ("DELETE FROM MEETINGRECORD WHERE fileName = ?");
    query.prepare(str);
    query.addBindValue(meetingName);
    bool flag = query.exec();
    if(!flag){
        KyInfo() << "Error: delete meetingrecord error";
        return false;
    }

    return true;
}

QString ChatDatabase::formatMeetingTime(const QString meetingName)
{
        // 分割字符串
        QStringList parts = meetingName.split('_');
        if (parts.size() < 3) {
            return QString(); // 返回空字符串，如果格式不正确
        }

        QString datePart = parts[1]; // "20241101"
        QString timePart = parts[2]; // "120518"

        // 提取日期部分
        QString year = datePart.mid(0, 4);
        QString month = datePart.mid(4, 2);
        QString day = datePart.mid(6, 2);

        // 提取时间部分
        QString hour = timePart.mid(0, 2);
        QString minute = timePart.mid(2, 2);
        QString second = timePart.mid(4, 2);

        // 格式化为目标字符串
        return QString("%1/%2/%3 · %4:%5:%6").arg(year, month, day, hour, minute, second);
}

int ChatDatabase::selectMaxHistoryId()
{
    // 查询最大history_id
    if (!d->is_open)
        return 0;
    QStringList result;
    QSqlQuery query(d->base);
    QString str = ("SELECT history_id FROM HISTORY_ID WHERE id = 1");
    query.prepare(str);
    bool flag = query.exec();
    if(!flag){
        KyInfo() << "Error: select selectMaxHistoryId error";
        return {};
    }
    int history_id = 0;
    if(query.next()){
        history_id = query.value(0).toInt();
    }
    return history_id;
}

void ChatDatabase::updateMaxHistoryId(int history_id)
{
    KyInfo() << "UPDATE HISTORY_ID ==" << history_id;
    if (!d->is_open)
        return;
    QSqlQuery query(d->base);
    QString str = ("UPDATE HISTORY_ID SET history_id = ? WHERE ID = 1");

    query.prepare(str);
    query.addBindValue(history_id);

    bool flag = query.exec();
    if(!flag){
        KyInfo() << "Error: UPDATE HISTORY_ID error";
    }
}

ChatDatabase *ChatDatabase::getChatDatabase()
{
    if (g_chatDatabase== nullptr) {
        g_chatDatabase = new ChatDatabase;
    }

    return g_chatDatabase;
}

int ChatDatabase::selectLastRecordChatType(QString sessionID)
{
    // 根据sessionID查询最后一条数据的聊天类型
    if (!d->is_open)
        return 0;
    int chat_type;
    QSqlQuery query(d->base);
    QString str = ("SELECT chat_type FROM RECORD WHERE sessionID = ? order by msgIndex desc limit 1");
    query.prepare(str);
    query.addBindValue(sessionID);
    bool flag = query.exec();
    if(!flag){
        KyInfo() << "Error: selectLastRecordChatType error";
        return false;
    }
    if(query.next()){
        chat_type = query.value(0).toInt();
    }
    return chat_type;
}

int ChatDatabase::selectLastChatId(QString sessionID, int chatType)
{
    // 查询要恢复的历史对话最后一段的第一条数据
    if (!d->is_open)
        return 0;
    int msgIndex;
    QSqlQuery query(d->base);
    QString str = ("SELECT msgIndex FROM RECORD WHERE sessionID = ? and chat_type = ? order by msgIndex desc limit 1");
    query.prepare(str);
    query.addBindValue(sessionID);
    query.addBindValue(chatType);

    bool flag = query.exec();
    if(!flag){
        KyInfo() << "Error: selectLastRecordChatType error";
        return false;
    }
    if(query.next()){
        msgIndex = query.value(0).toInt();
    }
    return msgIndex;
}

int ChatDatabase::selectMaxVoiceHistoryId()
{
    // 查询最大VOICE_id
    if (!d->is_open)
        return 0;
    QStringList result;
    QSqlQuery query(d->base);
    QString str = ("SELECT history_id FROM HISTORY_ID WHERE id = 2");
    query.prepare(str);
    bool flag = query.exec();
    if(!flag){
        KyInfo() << "Error: select selectMaxVoiceHistoryId error";
        return {};
    }
    int history_id = 0;
    if(query.next()){
        history_id = query.value(0).toInt();
    }

    return history_id;
}

void ChatDatabase::updateMaxVoiceHistoryId(int voiceHistory_id)
{
    KyInfo() << "UPDATE VOICEHISTORY_ID ==" << voiceHistory_id;
    if (!d->is_open)
        return;
    QSqlQuery query(d->base);
    QString str = ("UPDATE HISTORY_ID SET history_id = ? WHERE ID = 2");

    query.prepare(str);
    query.addBindValue(voiceHistory_id);

    bool flag = query.exec();
    if(!flag){
        KyInfo() << "Error: UPDATE HISTORY_ID error";
    }

    return;
}

void ChatDatabase::updateIsCollect(QString sessionID, int isCollect)
{
    //KyInfo() << "updateIsCollect sessionID ==" << sessionID << "-isCollect-" << isCollect;
    if (!d->is_open)
        return;
    QSqlQuery query(d->base);
    QString str = ("UPDATE record SET is_collect = ? WHERE sessionID = ? and msgIndex = 0");

    query.prepare(str);
    query.addBindValue(isCollect);
    query.addBindValue(sessionID);

    bool flag = query.exec();
    if(!flag){
        KyInfo() << "Error: updateIsCollect error";
    }
}

int ChatDatabase::selectHistoryCount(int isCollect)
{
    if (!d->is_open)
        return {};
    QSqlQuery query(d->base);

    int result = 0;
    QString selectSql = QString("SELECT count(*) FROM RECORD where msgIndex = 0");

    if(isCollect == 1){
        selectSql = selectSql + " AND is_collect = 1";
    }

    bool flag = query.exec(selectSql);
    if (!flag) {
        KyInfo() << "Error: selectHistoryCount record error";
        return result;
    }
    while(query.next()){
        result = query.value(0).toInt();
    }

    return result;
}

void ChatDatabase::deleteTheEarliestHistory()
{
    if (!d->is_open)
        return ;

    QSqlQuery query(d->base);
    QString result = "";

    // 查询所有记录，准备遍历
    bool flag = query.exec(QString("SELECT sessionID FROM RECORD where msgIndex = 0 and is_collect = 0 ORDER BY operateTime asc limit 1"));
    if (!flag) {
        KyInfo() << "Error: select record error";
        return ;
    }
    while(query.next()){
        result = query.value(0).toString();
    }
    KyInfo() << "Error: deleteTheEarliestHistory record error" <<result;

    if(result != ""){
        deleteBySessionID(result);
    }
}

void ChatDatabase::initBase()
{
    d->base = QSqlDatabase::addDatabase("QSQLITE");
    QString config_path = QDir::homePath().append("/.config/kylin-aiassistant");
    if (!QDir().exists(config_path))
        QDir().mkdir(config_path);

    d->base.setDatabaseName(config_path.append("/kylin_aiassistant_database.db"));
    if (!d->base.open())
         KyInfo() << "Error: open database failed!";
    d->is_open = d->base.isOpen();
}

void ChatDatabase::createTable()
{
    if(!d->is_open){
        KyInfo() << "Error: open database failed!";
    }
    QSqlQuery sql_query(d->base);
    if (!sql_query.exec(CREATE_TABLE_RECORD_SQL))
    {
        KyInfo() << "create table record error";
    }
    if (!sql_query.exec(CREATE_TABLE_HISTORY_ID_SQL))
    {
        KyInfo() << "create table HISTORY_ID error";
    }
    if(!columnExists(d->base, "HISTORY_ID", "mode_type")){
        if (!sql_query.exec(ALTER_TABLE_ADD_HISTORY_ID_COLUMN_MODE_TYPE_SQL)){
            KyInfo() << "ALTER_TABLE_ADD_HISTORY_ID_COLUMN_MODE_TYPE_SQL error";
        }
    }
    {
        QSqlQuery insert_query(d->base);
        if (!insert_query.exec(INSERT_HISTORY_ID_SQL)) {
            KyInfo() << "insert HISTORY_ID error";
        }
    }
    if(!columnExists(d->base, "RECORD", "chat_type")){
        if (!sql_query.exec(ALTER_TABLE_ADD_RECORD_COLUMN_CHAT_TYPE_SQL)){
            KyInfo() << "ALTER_TABLE_ADD_RECORD_COLUMN_CHAT_TYPE_SQL error";
        }
    }
    if(!columnExists(d->base, "RECORD", "is_collect")){
        if (!sql_query.exec(ALTER_TABLE_ADD_RECORD_COLUMN_IS_COLLECT_SQL)){
            KyInfo() << "ALTER_TABLE_ADD_RECORD_COLUMN_IS_COLLECT_SQL error";
        }
    }
    if(!columnExists(d->base, "RECORD", "request_data")){
        if (!sql_query.exec(ALTER_TABLE_ADD_RECORD_COLUMN_REQUEST_DATA_SQL)){
            KyInfo() << "ALTER_TABLE_ADD_RECORD_COLUMN_REQUEST_DATA_SQL error";
        }
    }
    if(!columnExists(d->base, "RECORD", "mode_type")){
        if (!sql_query.exec(ALTER_TABLE_ADD_RECORD_COLUMN_MODE_TYPE_SQL)){
            KyInfo() << "ALTER_TABLE_ADD_RECORD_COLUMN_MODE_TYPE_SQL error";
        }
    }
    if(!columnExists(d->base, "RECORD", "session_uuid")){
        if (!sql_query.exec(ALTER_TABLE_ADD_RECORD_COLUMN_SESSION_UUID_SQL)){
            KyInfo() << "ALTER_TABLE_ADD_RECORD_COLUMN_SESSION_UUID_SQL error";
        }
    }
    if(!columnExists(d->base, "RECORD", "has_unread")){
        if (!sql_query.exec(ALTER_TABLE_ADD_RECORD_COLUMN_HAS_UNREAD_SQL)){
            KyInfo() << "ALTER_TABLE_ADD_RECORD_COLUMN_HAS_UNREAD_SQL error";
        }
    }
}

void ChatDatabase::createMeetingTable()
{
    if(!d->is_open){
        KyInfo() << "Error: open database failed!";
    }
    QSqlQuery sql_query(d->base);
    if (!sql_query.exec(CREATE_TABLE_MEETING_RECORD_SQL))
    {
        KyInfo() << "create table meeting_record error";
    }
}

bool ChatDatabase::columnExists(const QSqlDatabase &db, const QString &tableName, const QString &columnName)
{
    // 查询表的信息
    QSqlQuery sql_query(d->base);
    sql_query.prepare("PRAGMA table_info(" + tableName + ")");

    if (!sql_query.exec()) {
        qWarning() << "Failed to get table info:" << sql_query.lastError().text();
        return false;
    }

    // 检查列是否存在
    while (sql_query.next()) {
        QString existingColumn = sql_query.value(1).toString();  // 1 是列名的位置
        if (existingColumn == columnName) {
            //KyInfo() << "FIND " << columnName << "sucess!!";
            return true;  // 找到列
        }
    }
    return false;  // 没有找到列
}

int ChatDatabase::getSessionModeType(const QString& sessionID)
{
    if (!d->is_open)
        return 0;

    QSqlQuery query(d->base);
    query.prepare("SELECT mode_type, chat_type FROM RECORD WHERE sessionID = ? LIMIT 1");
    query.addBindValue(sessionID);

    if (query.exec() && query.next()) {
        int modeType = query.value(0).toInt();
        int chatType = query.value(1).toInt();
        if (sessionID.startsWith(QStringLiteral("v_")) && chatType == 0) {
            return 1;
        }
        return modeType;
    }
    return 0;
}

void ChatDatabase::setUnreadFlag(const QString& sessionID, int hasUnread)
{
    if (!d->is_open) return;
    QSqlQuery query(d->base);
    query.prepare("UPDATE RECORD SET has_unread = ? WHERE sessionID = ?");
    query.addBindValue(hasUnread);
    query.addBindValue(sessionID);
    query.exec();
}

QString ChatDatabase::selectSessionUuidBySessionID(const QString& sessionID)
{
    if (!d->is_open)
        return QString();
    QSqlQuery query(d->base);
    query.prepare("SELECT session_uuid FROM RECORD WHERE sessionID = ? LIMIT 1");
    query.addBindValue(sessionID);
    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    return QString();
}

QString ChatDatabase::selectSessionIDBySessionUuid(const QString& sessionUuid)
{
    if (!d->is_open || sessionUuid.isEmpty())
        return QString();
    QSqlQuery query(d->base);
    query.prepare("SELECT sessionID FROM RECORD WHERE session_uuid = ? LIMIT 1");
    query.addBindValue(sessionUuid);
    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    return QString();
}

void ChatDatabase::markSessionSource(const QString& sessionID, const QString& source)
{
    if (!d->is_open) return;
    QSqlQuery query(d->base);
    query.prepare("UPDATE RECORD SET session_uuid = ? WHERE sessionID = ?");
    query.addBindValue(source);
    query.addBindValue(sessionID);
    query.exec();
}

bool ChatDatabase::sessionExists(const QString& sessionID)
{
    if (!d->is_open) return false;
    QSqlQuery query(d->base);
    query.prepare("SELECT 1 FROM RECORD WHERE sessionID = ? LIMIT 1");
    query.addBindValue(sessionID);
    if (query.exec() && query.next()) {
        return true;
    }
    return false;
}

void ChatDatabase::setOperateTime(const QString& sessionID, int index, qint64 timestampSecs)
{
    if (!d->is_open) return;
    QSqlQuery query(d->base);
    query.prepare("UPDATE RECORD SET operateTime = ? WHERE sessionID = ? AND msgIndex = ?");
    query.addBindValue(timestampSecs);
    query.addBindValue(sessionID);
    query.addBindValue(index);
    query.exec();
}
