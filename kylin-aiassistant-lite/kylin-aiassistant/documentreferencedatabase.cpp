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
#include "documentreferencedatabase.h"

#include <QDir>
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <ukui-log4qt.h>

#define CREATE_TABLE_DOCUMENT_REFERENCE_SQL "CREATE TABLE IF NOT EXISTS DOCUMENT_REFERENCE(ID INTEGER PRIMARY KEY AUTOINCREMENT, sessionID VARCHAR(36), fileType VARCHAR(50), documents TEXT)"

DocumentReferenceDatabase* DocumentReferenceDatabase::g_instance = nullptr;

class DocumentReferenceDatabase::Private
{
public:
    bool is_open;
    QSqlDatabase base;
};

DocumentReferenceDatabase::DocumentReferenceDatabase():
    d(std::make_shared<DocumentReferenceDatabase::Private>())
{
    initBase();
    createTable();
}

DocumentReferenceDatabase::~DocumentReferenceDatabase()
{
    if (d->is_open)
        d->base.close();
}

DocumentReferenceDatabase* DocumentReferenceDatabase::getInstance()
{
    if (g_instance == nullptr) {
        g_instance = new DocumentReferenceDatabase();
    }
    return g_instance;
}

bool DocumentReferenceDatabase::insertDocumentReference(const QString &sessionID, const QString &fileType, const QString &documents)
{
    if (!d->is_open)
        return false;
    QSqlQuery query(d->base);
    // 先删除该会话的旧记录（每个会话只保留最新的文件引用）
    QString deleteStr = "DELETE FROM DOCUMENT_REFERENCE WHERE sessionID = ?";
    query.prepare(deleteStr);
    query.addBindValue(sessionID);
    query.exec();

    // 插入新记录
    QString str = "INSERT INTO DOCUMENT_REFERENCE(sessionID, fileType, documents) VALUES (?, ?, ?)";
    query.prepare(str);
    query.addBindValue(sessionID);
    query.addBindValue(fileType);
    query.addBindValue(documents);
    bool flag = query.exec();
    if (!flag) {
        KyInfo() << "Error: insertDocumentReference error" << query.lastError().text();
        return false;
    }
    return true;
}

QString DocumentReferenceDatabase::selectDocumentReferenceBySessionID(const QString &sessionID)
{
    if (!d->is_open)
        return QString();
    QSqlQuery query(d->base);
    QString str = "SELECT fileType, documents FROM DOCUMENT_REFERENCE WHERE sessionID = ?";
    query.prepare(str);
    query.addBindValue(sessionID);
    bool flag = query.exec();
    if (!flag) {
        KyInfo() << "Error: selectDocumentReferenceBySessionID error" << query.lastError().text();
        return QString();
    }
    if (query.next()) {
        QString fileType = query.value("fileType").toString();
        QString documents = query.value("documents").toString();

        if (!documents.isEmpty()) {
            // 构造文档引用信息 JSON
            QJsonObject docObj;
            docObj["type"] = fileType;

            // 解析 documents JSON 字符串
            QJsonDocument docsDoc = QJsonDocument::fromJson(documents.toUtf8());
            if (docsDoc.isArray()) {
                QJsonArray docsArray = docsDoc.array();
                QJsonArray docList;
                for (int i = 0; i < docsArray.size(); ++i) {
                    QJsonObject docItem = docsArray[i].toObject();
                    QJsonObject formattedDocItem;
                    formattedDocItem["documentName"] = docItem.value("name").toString();
                    formattedDocItem["documentSize"] = docItem.value("size").toString();
                    formattedDocItem["documentIcon"] = docItem.value("icon").toString();
                    formattedDocItem["documentPath"] = docItem.value("path").toString();
                    formattedDocItem["documentUuid"] = docItem.value("uuid").toString();
                    formattedDocItem["isDir"] = docItem.value("isDir").toBool(false);
                    formattedDocItem["childCount"] = docItem.value("childCount").toInt(0);
                    docList.append(formattedDocItem);
                }
                docObj["list"] = docList;
            }
            return QJsonDocument(docObj).toJson(QJsonDocument::Compact);
        }
    }
    return QString();
}

bool DocumentReferenceDatabase::deleteDocumentReferenceBySessionID(const QString &sessionID)
{
    if (!d->is_open)
        return false;
    QSqlQuery query(d->base);
    QString str = "DELETE FROM DOCUMENT_REFERENCE WHERE sessionID = ?";
    query.prepare(str);
    query.addBindValue(sessionID);
    bool flag = query.exec();
    if (!flag) {
        KyInfo() << "Error: deleteDocumentReferenceBySessionID error" << query.lastError().text();
        return false;
    }
    return true;
}

void DocumentReferenceDatabase::initBase()
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

void DocumentReferenceDatabase::createTable()
{
    if (!d->is_open) {
        KyInfo() << "Error: open database failed!";
        return;
    }
    QSqlQuery sql_query(d->base);
    if (!sql_query.exec(CREATE_TABLE_DOCUMENT_REFERENCE_SQL))
    {
        KyInfo() << "create table DOCUMENT_REFERENCE error";
    }
}

bool DocumentReferenceDatabase::columnExists(const QSqlDatabase &db, const QString &tableName, const QString &columnName)
{
    Q_UNUSED(db);
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
            return true;  // 找到列
        }
    }
    return false;  // 没有找到列
}

