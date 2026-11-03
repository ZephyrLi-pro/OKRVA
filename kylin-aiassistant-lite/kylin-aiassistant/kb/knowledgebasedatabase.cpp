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
#include "knowledgebasedatabase.h"

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
#include <QQueue>

#define CREATE_TABLE_KNOWLEDGEBASE_SQL \
    "CREATE TABLE IF NOT EXISTS KNOWLEDGEBASE("\
    "itemId INTEGER PRIMARY KEY AUTOINCREMENT, "\
    "parentId INTEGER DEFAULT 0, "\
    "uuid TEXT, "\
    "documentPath TEXT, "\
    "documentTargetPath TEXT, "\
    "isDir INTEGER,"\
    "status INTEGER, "\
    "percent INTEGER, "\
    "errorCode INTEGER, "\
    "createTime TEXT, "\
    "documentSize INTEGER DEFAULT 0)"

KnowledgeBaseDatabase* KnowledgeBaseDatabase::g_instance = nullptr;

class KnowledgeBaseDatabase::Private
{
public:
    bool is_open;
    QSqlDatabase base;
};

KnowledgeBaseDatabase::KnowledgeBaseDatabase():
    d(std::make_shared<KnowledgeBaseDatabase::Private>())
{
    initBase();
    createKbTable();
}

KnowledgeBaseDatabase::~KnowledgeBaseDatabase()
{
    if (d->is_open)
        d->base.close();
}

KnowledgeBaseDatabase* KnowledgeBaseDatabase::getInstance()
{
    if (g_instance == nullptr) {
        g_instance = new KnowledgeBaseDatabase;
    }
    return g_instance;
}

void KnowledgeBaseDatabase::initBase()
{
    d->base = QSqlDatabase::addDatabase("QSQLITE", "KnowledgeBaseConnection");
    QString config_path = QDir::homePath().append("/.config/kylin-aiassistant");
    if (!QDir().exists(config_path))
        QDir().mkdir(config_path);

    d->base.setDatabaseName(config_path.append("/knowledgebase_database.db"));
    if (!d->base.open())
         KyInfo() << "Error: open knowledgebase database failed!";
    d->is_open = d->base.isOpen();
}

void KnowledgeBaseDatabase::createKbTable()
{
    if(!d->is_open){
        KyInfo() << "Error: open knowledgebase database failed!";
        return;
    }
    QSqlQuery sql_query(d->base);
    if (!sql_query.exec(CREATE_TABLE_KNOWLEDGEBASE_SQL))
    {
        KyInfo() << "create table knowledgeBase error";
    }

    return;
}

bool KnowledgeBaseDatabase::isKbTableEmpty()
{
    if (!d->is_open) {
        KyInfo() << "The database is not open.";
        return true;
    }

    QSqlQuery query(d->base);
    QString str = QString("SELECT COUNT(*) FROM %1").arg("KNOWLEDGEBASE");
    query.prepare(str);
    bool ret = query.exec();
    if(!ret) {
        KyInfo() << "Error: isKbTableEmpty " << query.lastError().text();
        return true;
    }

    if (query.next()){
        int cnt = query.value(0).toInt();
        return cnt == 0;
    }

    return true;
}

QStringList KnowledgeBaseDatabase::selectAllKnowledgeBase()
{
    if (!d->is_open)
        return {};
    QStringList result;
    QSqlQuery query(d->base);
    bool flag = query.exec(QString("SELECT documentPath FROM KNOWLEDGEBASE"));
    if(!flag){
        KyInfo() << "Error: select knowledgeBase error";
        return {};
    }
    while(query.next()){
        QString str = query.value(0).toString();
        result.append(str);
    }

    return result;
}

int KnowledgeBaseDatabase::getItemIdByDocumentargetPath(QString documentTargetPath)
{
    if (!d->is_open) {
        KyInfo() << "The database is not open.";
        return -1;
    }
    QSqlQuery query(d->base);
    QString str = ("SELECT itemId FROM KNOWLEDGEBASE WHERE documentTargetPath = ?");
    query.prepare(str);
    query.addBindValue(documentTargetPath);
    bool flag = query.exec();
    if(!flag){
        KyInfo() << "Error: " << query.lastError().text();
        return -1;
    }

    if (query.next()){
        return query.value("itemId").toInt();;
    }

    return -1;
}

int KnowledgeBaseDatabase::getItemIdByDocumentPath(QString documentPath)
{
    if (!d->is_open) {
        KyInfo() << "The database is not open.";
        return -1;
    }
    QSqlQuery query(d->base);
    QString str = ("SELECT itemId FROM KNOWLEDGEBASE WHERE documentTargetPath = ?");
    query.prepare(str);
    query.addBindValue(documentPath);
    bool flag = query.exec();
    if(!flag){
        KyInfo() << "Error: " << query.lastError().text();
        return -1;
    }

    if (query.next()){
        return query.value("itemId").toInt();;
    }

    return -1;
}

KbDocumentItem KnowledgeBaseDatabase::getItemByDocumentTargetPath(QString documentTargetPath)
{
    KbDocumentItem item;
    if (!d->is_open) {
        KyInfo() << "The database is not open.";
        return item;
    }

    QSqlQuery query(d->base);
    query.prepare(
        "SELECT itemId, uuid, documentPath, documentTargetPath, status, percent, errorCode, createTime, documentSize, isDir "
        "FROM KNOWLEDGEBASE "
        "WHERE documentTargetPath = ?"
    );
    query.addBindValue(documentTargetPath);

    if (!query.exec()) {
        KyInfo() << "SELECT Error: " << query.lastError().text();
        return item;
    }

    if (query.next()) {
        item.itemId = query.value("itemId").toInt();
        item.uuid = query.value("uuid").toString();
        item.documentPath = query.value("documentPath").toString();
        item.documentTargetPath = query.value("documentTargetPath").toString();
        item.status = query.value("status").toInt();
        item.percent = query.value("percent").toInt();
        item.errorCode = query.value("errorCode").toInt();
        item.createTime = query.value("createTime").toString();
        item.documentSize = query.value("documentSize").toLongLong();
        item.isDir = query.value("isDir").toBool();
    }

    return item;
}

int KnowledgeBaseDatabase::selectKnowledgeFileTotelCount(bool isDir)
{
    if (!d->is_open) {
        KyInfo() << "The database is not open.";
        return 0;
    }
    QSqlQuery query(d->base);
    QString str = ("select count(*) from KNOWLEDGEBASE where isDir = ?");
    query.prepare(str);
    if(isDir){
        query.addBindValue(1);
    }else{
        query.addBindValue(0);
    }
    bool flag = query.exec();
    if(!flag){
        KyInfo() << "Error: " << query.lastError().text();
        return 0;
    }

    if (query.next()){
        int count = query.value(0).toInt();
        return count;
    }

    return 0;
}

int KnowledgeBaseDatabase::selectMaxHierarchyByPath(QString documentPath)
{
    KyInfo() << "selectMaxHierarchyByPath documentPath is"  << documentPath;
    if (documentPath == nullptr) {
        KyInfo() << "Error: documentPath is null.";
        return false;
    }

    if (!d->is_open) {
        KyInfo() << "The database is not open.";
        return 0;
    }
    QSqlQuery query(d->base);
    QString str = R"(
        WITH RECURSIVE
        root_node AS (
            SELECT itemId AS root_id
            FROM KNOWLEDGEBASE
            WHERE documentTargetPath = ?
        ),
        child_nodes AS (
            -- 初始查询：先把根节点自身加进来
            SELECT rn.root_id AS itemId FROM root_node rn
            UNION ALL
            -- 再递归查所有子节点
            SELECT kb.itemId
            FROM KNOWLEDGEBASE kb
            JOIN child_nodes cn ON kb.parentId = cn.itemId
            WHERE kb.isDir = 1
        )
        SELECT COUNT(*) AS total_child_count FROM child_nodes;
    )";
    query.prepare(str);
    query.addBindValue(documentPath);
    bool flag = query.exec();
    if(!flag){
        KyInfo() << "selectMaxHierarchyByPath Error: " << query.lastError().text();
        return 0;
    }

    if (query.next()){
        int maxHierarchy = query.value(0).toInt();
        KyInfo() << "selectMaxHierarchyByPath maxHierarchy: " << maxHierarchy;
        return maxHierarchy;
    }

    return 0;

}

QStringList KnowledgeBaseDatabase::selectDocumentPathListByItemId(int itemId)
{
    if (!d->is_open)
        return {};
    QStringList result;
    QSqlQuery query(d->base);
    QString selectSql = R"(
            WITH RECURSIVE child_nodes AS (
                -- 初始：源节点自身（第一个禁用路径）
                SELECT documentTargetPath FROM KNOWLEDGEBASE WHERE itemId = ?
                UNION ALL
                -- 递归：所有子节点的路径
                SELECT kb.documentTargetPath
                FROM KNOWLEDGEBASE kb
                JOIN child_nodes cn ON kb.parentId = (SELECT itemId FROM KNOWLEDGEBASE WHERE documentTargetPath = cn.documentTargetPath)
            )
            SELECT documentTargetPath FROM child_nodes;
        )";
    query.prepare(selectSql);
    query.addBindValue(itemId);
    bool flag = query.exec();
    if(!flag){
        KyInfo() << "Error: selectDocumentPathListByItemId error";
        return {};
    }
    while(query.next()){
        QString str = query.value(0).toString();
        KyInfo() << "selectDocumentPathListByItemId======" << str;
        result.append(str);
    }

    return result;
}

QStringList KnowledgeBaseDatabase::selectDocumentTargetPathListByParentId(int itemId)
{
    if (!d->is_open)
        return {};
    QStringList result;
    QSqlQuery query(d->base);
    QString selectSql = "SELECT documentTargetPath FROM KNOWLEDGEBASE WHERE parentId = ?";
    query.prepare(selectSql);
    query.addBindValue(itemId);
    bool flag = query.exec();
    if(!flag){
        KyInfo() << "Error: selectDocumentPathListByItemId error";
        return {};
    }
    while(query.next()){
        QString str = query.value(0).toString();
        KyInfo() << "selectDocumentPathListByItemId======" << str;
        result.append(str);
    }

    return result;
}

QList<int> KnowledgeBaseDatabase::selectAllChildDirItemIdByFilepath(int itemId)
{
    if (!d->is_open)
        return {};
    QList<int> result;
    QSqlQuery query(d->base);
    QString selectSql = R"(
    WITH RECURSIVE child_nodes AS (
                    -- 初始：源节点自身（第一个禁用路径）
                    SELECT documentTargetPath,itemId FROM KNOWLEDGEBASE WHERE itemId = ?
                    UNION ALL
                    -- 递归：所有子节点的路径
                    SELECT kb.documentTargetPath,kb.itemId
                    FROM KNOWLEDGEBASE kb
                    JOIN child_nodes cn ON kb.parentId = (SELECT itemId FROM KNOWLEDGEBASE WHERE documentTargetPath = cn.documentTargetPath) and isDir = 1
                )
                SELECT itemId FROM child_nodes;
            )";
    query.prepare(selectSql);
    query.addBindValue(itemId);
    bool flag = query.exec();
    if(!flag){
        KyInfo() << "Error: selectAllChildDirItemIdByFilepath error";
        return {};
    }
    while(query.next()){
        int str = query.value(0).toInt();
        result.append(str);
    }

    return result;
}

int KnowledgeBaseDatabase::selectFileCountByFilePath(QList<int> list, bool isSuccess)
{
    if (list.isEmpty()) {
        return 0;
    }
    if (!d->is_open)
        return {};
    int result = 0;

    QString placeholderStr;
    int listSize = list.size();
    for (int i = 0; i < listSize; ++i) {
        if (i > 0) {
            placeholderStr += ", "; // 非第一个元素前加逗号分隔
        }
        placeholderStr += "?"; // 每个元素对应一个?
    }

    QSqlQuery query(d->base);
    QString selectSql = QString("SELECT COUNT(*) FROM KNOWLEDGEBASE WHERE parentId IN (%1) AND isDir = ?").arg(placeholderStr);
    if(isSuccess){
        selectSql = selectSql + " AND percent = 100";
    }else{
        selectSql = selectSql + " AND percent != 100";
    }


    KyInfo() << "selectFileCountByFilePath  sql = " << selectSql;
    query.prepare(selectSql);
    for (int i = 0; i < listSize; ++i) {
        query.bindValue(i, list[i]);
    }
    query.bindValue(listSize, 0);
    bool flag = query.exec();
    if(!flag){
        KyInfo() << "Error: selectFileCountByFilePath error";
        return {};
    }
    while(query.next()){
        result = query.value(0).toInt();
    }

    return result;
}

QStringList KnowledgeBaseDatabase::selectAllFileIdsByFilepath(QList<int> list)
{
    if (!d->is_open)
        return {};
    QStringList result;

    QString placeholderStr;
    int listSize = list.size();
    for (int i = 0; i < listSize; ++i) {
        if (i > 0) {
            placeholderStr += ", "; // 非第一个元素前加逗号分隔
        }
        placeholderStr += "?"; // 每个元素对应一个?
    }

    QSqlQuery query(d->base);
    QString selectSql = QString("SELECT uuid FROM KNOWLEDGEBASE WHERE parentId IN (%1) AND isDir = ? AND percent = 100").arg(placeholderStr);
    query.prepare(selectSql);
    for (int i = 0; i < listSize; ++i) {
        query.bindValue(i, list[i]);
    }
    query.bindValue(listSize, 0);
    bool flag = query.exec();
    if(!flag){
        KyInfo() << "Error: selectFileCountByFilePath error";
        return {};
    }
    while(query.next()){
        QString str = query.value(0).toString();
        KyInfo() << "str====" << str;
        result.append(str);
    }
    return result;
}

QVariantList KnowledgeBaseDatabase::getLatestFilesByFolderIdRecursively(int folderId, int limit)
{
    KyInfo() << "getLatestFilesByFolderIdRecursively called, folderId:" << folderId << "limit:" << limit;

    QVariantList allFiles;
    if (!d->is_open) {
        KyInfo() << "Error: database is not inited.";
        return allFiles;
    }

    // 第一步：获取所有子文件夹的ID（包括自身）
    QList<int> folderIds;
    QQueue<int> queue;
    queue.enqueue(folderId);

    while (!queue.isEmpty()) {
        int currentId = queue.dequeue();
        folderIds.append(currentId);

        // 获取当前文件夹的直接子文件夹
        QSqlQuery subQuery(d->base);
        subQuery.prepare(
            "SELECT itemId FROM KNOWLEDGEBASE "
            "WHERE parentId = ? AND isDir = 1"
        );
        subQuery.addBindValue(currentId);

        if (subQuery.exec()) {
            while (subQuery.next()) {
                int subFolderId = subQuery.value("itemId").toInt();
                queue.enqueue(subFolderId);
            }
        }
    }

    if (folderIds.isEmpty()) {
        return allFiles;
    }

    // 第二步：构建IN查询的占位符
    QStringList placeholders;
    QVariantList bindValues;

    for (int i = 0; i < folderIds.size(); ++i) {
        placeholders.append("?");
        bindValues.append(folderIds[i]);
    }

    // 第三步：查询所有项目（包括文件和文件夹），按itemId降序获取最近的limit个
    QString sql = QString(
        "SELECT itemId, uuid, documentPath, documentTargetPath, status, percent, "
        "errorCode, createTime, documentSize, isDir "
        "FROM KNOWLEDGEBASE "
        "WHERE parentId IN (%1) AND status = 6 "  // 只查询上传成功的项目和文件夹
        "ORDER BY itemId DESC "  // 按itemId降序获取最新的项目
        "LIMIT ?"
    ).arg(placeholders.join(", "));

    QSqlQuery query(d->base);
    query.prepare(sql);

    // 绑定所有文件夹ID
    for (const QVariant &value : bindValues) {
        query.addBindValue(value);
    }

    // 绑定限制数量
    query.addBindValue(limit);

    if (!query.exec()) {
        KyInfo() << "Error: getLatestFilesByFolderIdRecursively failed: " << query.lastError().text();
        return allFiles;
    }

    // 第四步：获取所有查询结果
    QList<QVariantMap> allItems;

    while (query.next()) {
        QVariantMap item;
        item["itemId"] = query.value("itemId").toInt();
        item["uuid"] = query.value("uuid").toString();
        item["documentPath"] = query.value("documentPath").toString();
        QFileInfo sourceInfo(query.value("documentTargetPath").toString());
        QString fileName = sourceInfo.fileName();
        item["documentName"] = fileName;
        item["documentTargetPath"] = query.value("documentTargetPath").toString();
        item["status"] = query.value("status").toInt();
        item["percent"] = query.value("percent").toInt();
        item["errorCode"] = query.value("errorCode").toInt();
        item["createTime"] = query.value("createTime").toString();
        item["isDir"] = query.value("isDir").toBool();
        // 使用 documentTargetPath 获取文件大小和图标（知识库内实际路径），documentPath 为上传时的源路径可能已失效
        QString targetPath = item["documentTargetPath"].toString();
        if (item["isDir"].toBool()) {
            item["documentSize"] = QString("0");
        } else {
            item["documentSize"] = Globals::getinstance()->getFileSize(targetPath);
        }
        item["fileIcon"] = Globals::getFileIconPath(targetPath);

        allItems.append(item);
    }

    // 第六步：添加到返回列表
    for (int i = 0; i < allItems.size(); i++) {
        allFiles.append(allItems[i]);
    }

    return allFiles;
}

bool KnowledgeBaseDatabase::insertKnowledgeBaseInfo(int parentId, KbDocumentItem kbItem, int *itemId)
{
    if (itemId == nullptr) {
        KyInfo() << "Error: itemId is null.";
        return false;
    }

    if (!d->is_open) {
        KyInfo() << "Error: database is not inited.";
        return false;
    }

    QSqlQuery query(d->base);
    query.prepare(
        "INSERT INTO KNOWLEDGEBASE "
        "(parentId, uuid, documentPath, documentTargetPath, isDir, status, percent, errorCode, createTime, documentSize) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
    );
    query.addBindValue(parentId);
    query.addBindValue(kbItem.uuid);
    query.addBindValue(kbItem.documentPath);
    query.addBindValue(kbItem.documentTargetPath);
    query.addBindValue(kbItem.isDir);
    query.addBindValue(kbItem.status);
    query.addBindValue(kbItem.percent);
    query.addBindValue(kbItem.errorCode);
    query.addBindValue(kbItem.createTime);
    query.addBindValue(kbItem.documentSize);

    if (!query.exec()) {
        KyInfo() << "Failed to add file:" << query.lastError().text();
        return false;
    }

    // 将当前插入数据库文件或者文件夹的id作为出参返回
    *itemId = query.lastInsertId().toInt();

    return true;
}

bool KnowledgeBaseDatabase::insertPreviousKnowledgeBaseInfo(int parentId, KbDocumentItem kbItem, int *itemId)
{
    if (itemId == nullptr) {
        KyInfo() << "Error: itemId is null.";
        return false;
    }

    if (!d->is_open) {
        KyInfo() << "Error: database is not inited.";
        return false;
    }

    QSqlQuery query(d->base);
    query.prepare(
        "INSERT INTO KNOWLEDGEBASE "
        "(itemId, parentId, uuid, documentPath, documentTargetPath, isDir, status, percent, errorCode, createTime, documentSize) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
    );
    query.addBindValue(kbItem.itemId);
    query.addBindValue(parentId);
    query.addBindValue(kbItem.uuid);
    query.addBindValue(kbItem.documentPath);
    query.addBindValue(kbItem.documentTargetPath);
    query.addBindValue(kbItem.isDir);
    query.addBindValue(kbItem.status);
    query.addBindValue(kbItem.percent);
    query.addBindValue(kbItem.errorCode);
    query.addBindValue(kbItem.createTime);
    query.addBindValue(kbItem.documentSize);

    if (!query.exec()) {
        KyInfo() << "Failed to add file:" << query.lastError().text();
        return false;
    }

    // 将当前插入数据库文件或者文件夹的id作为出参返回
    *itemId = kbItem.itemId;

    return true;
}

QStringList KnowledgeBaseDatabase::getKbTableFields()
{
    QStringList fields;
    if (!d->is_open) {
        KyInfo() << "Error: database is not inited.";
        return fields;
    }

    QSqlQuery query(d->base);
    QString sql = QString("SELECT * FROM KNOWLEDGEBASE LIMIT 0");

    if (query.exec(sql)) {
        QSqlRecord record = query.record();
        for (int i = 0; i < record.count(); ++i) {
            fields.append(record.fieldName(i));
        }
    }

    return fields;
}

QString KnowledgeBaseDatabase::buildUpdateKbTableSql(int itemId, const QJsonObject &jsonObj)
{
    if (jsonObj.isEmpty()) {
        return QString();
    }

    // 获取表字段列表
    QStringList tableFields = getKbTableFields();
    if (tableFields.isEmpty()) {
        return QString();
    }

    KyInfo() << "tableFields : " << tableFields;

    // 构建SET子句
    QStringList setParts;
    for (auto it = jsonObj.begin(); it != jsonObj.end(); ++it) {
        QString field = it.key();
        QJsonValue value = it.value();

        // 只处理存在于表中的字段
        if (!tableFields.contains(field)) {
            continue;
        }

        QString setValue;
        if (value.isString()) {
            setValue = QString("'%1'").arg(value.toString().replace("'", "''"));
        } else if (value.isDouble()) {
            setValue = QString::number(value.toDouble());
        } else {
            // 其他字段跳过
            continue;
        }

        setParts.append(QString("%1 = %2").arg(field, setValue));
    }

    if (setParts.isEmpty()) {
        return QString();
    }

    // 构建完整SQL语句
    QString sql = QString("UPDATE KNOWLEDGEBASE SET %1 WHERE itemId = %2")
                  .arg(setParts.join(", "))
                  .arg(itemId);

    return sql;
}

bool KnowledgeBaseDatabase::updateKbTableById(QString updateInfo, int itemId)
{
    KyInfo() << "updateInfo : " << updateInfo << " itemId : " << itemId;

    if (!d->is_open) {
        KyInfo() << "Error: database is not inited.";
        return false;
    }

    QJsonParseError parseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(updateInfo.toUtf8(), &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        KyInfo() << "parseError: " << parseError.error;
        return false;
    }

    if (!jsonDoc.isObject()) {
        KyInfo() << "Input is not an object";
        return false;
    }

    // 解析JSON字符串
    QJsonObject jsonObj = jsonDoc.object();
    if (jsonObj.isEmpty()) {
        KyInfo() << "Input object is empty";
        return false;
    }

    // 构建更新SQL语句
    QString sql = buildUpdateKbTableSql(itemId, jsonObj);
    if (sql.isEmpty()) {
        KyInfo() << "Cannot to build sql.";
        return false;
    }

    KyInfo() << "sql : " << sql;

    // 执行SQL更新
    QSqlQuery query(d->base);
    if (!query.exec(sql)) {
        KyInfo() << "UPDATE Error: " << query.lastError().text();
        return false;
    }

    return true;
}

QList<KbDocumentItem> KnowledgeBaseDatabase::getChildrenByParentId(int parentId)
{
    KyInfo() << "parentId is " << parentId;
    QList<KbDocumentItem> children;
    if (!d->is_open) {
        KyInfo() << "Error: database is not inited.";
        return children;
    }

    QSqlQuery query(d->base);
    query.prepare(
        "SELECT itemId, uuid, documentPath, documentTargetPath, status, percent, errorCode, createTime, documentSize, isDir "
        "FROM KNOWLEDGEBASE "
        "WHERE parentId = ? "
        "ORDER BY itemId DESC "
    );
    query.addBindValue(parentId);

    bool ret = query.exec();
    if (!ret){
        KyInfo() << "SELECT Error: " << query.lastError().text();
        return children;
    }

    while (query.next()) {
        KbDocumentItem item;
        item.itemId = query.value("itemId").toInt();
        item.uuid = query.value("uuid").toString();
        item.documentPath = query.value("documentPath").toString();

        item.documentTargetPath = query.value("documentTargetPath").toString();
        KyInfo() << "documentTargetPath is " << item.documentTargetPath;

        item.status = query.value("status").toInt();
        item.percent = query.value("percent").toInt();
        item.errorCode = query.value("errorCode").toInt();

        item.createTime = query.value("createTime").toString();
        item.documentSize = query.value("documentSize").toLongLong();
        item.isDir = query.value("isDir").toBool();

        children.append(item);
    }

    return children;
}

QList<KbDocumentItem> KnowledgeBaseDatabase::getChildrenByParentIdRecursively(int folderId)
{
    KyInfo() << "parentId is " << folderId;
    QList<KbDocumentItem> children;
    if (!d->is_open) {
        KyInfo() << "Error: database is not inited.";
        return children;
    }

    QQueue<int> queue;
    queue.enqueue(folderId);

    while (!queue.isEmpty()) {
        int currentId = queue.dequeue();

        // 获取所有子项
        QSqlQuery query(d->base);
        query.prepare(
            "SELECT itemId, uuid, documentPath, documentTargetPath, status, percent, errorCode, createTime, documentSize, isDir "
            "FROM KNOWLEDGEBASE "
            "WHERE parentId = ? ");
        query.addBindValue(currentId);

        KyInfo() << "currentId is " << currentId;

        if (query.exec()) {
            while (query.next()) {
                KbDocumentItem item;
                item.itemId = query.value("itemId").toInt();
                item.uuid = query.value("uuid").toString();
                item.documentPath = query.value("documentPath").toString();

                item.documentTargetPath = query.value("documentTargetPath").toString();
                KyInfo() << "documentTargetPath is " << item.documentTargetPath << " folderId is " << folderId;

                item.status = query.value("status").toInt();
                item.percent = query.value("percent").toInt();
                item.errorCode = query.value("errorCode").toInt();

                item.createTime = query.value("createTime").toString();
                item.documentSize = query.value("documentSize").toLongLong();
                item.isDir = query.value("isDir").toBool();

                children.append(item);

                queue.enqueue(item.itemId);
            }
        } else {
            KyInfo() << "query error: " << query.lastError().text() << " currentId " << currentId;
        }
    }

    return children;
}

bool KnowledgeBaseDatabase::deleteBatchFromKbTable(const QList<int> &batchIds)
{
    KyInfo() << "batchIds is " << batchIds;

    if (!d->is_open) {
        KyInfo() << "Error: database is not inited.";
        return false;
    }

    if (batchIds.isEmpty()) {
        KyInfo() << "batchIds is empty.";
        return true;
    }

    QStringList placeholders;
    QVariantList bindValues;

    for (int i = 0; i < batchIds.size(); ++i) {
        placeholders.append("?");
        bindValues.append(batchIds[i]);
    }

    QString deleteSql = QString(
        "DELETE FROM KNOWLEDGEBASE WHERE itemId IN (%1)"
    ).arg(placeholders.join(", "));

    QSqlQuery query(d->base);
    query.prepare(deleteSql);

    for (const QVariant &value : bindValues) {
        query.addBindValue(value);
    }

    if (!query.exec()) {
        KyInfo() << "Batch delete failed:" << query.lastError().text();
        return false;
    }

    return true;
}

bool KnowledgeBaseDatabase::deleteFromKbTable(const int &itemId)
{
    // 只能单个文件的调用此接口删除
    KyInfo() << "deleteFromKbTable " << itemId;

    if (!d->is_open) {
        KyInfo() << "Error: database is not inited.";
        return false;
    }

    QSqlQuery query(d->base);
    QString str = ("DELETE FROM KNOWLEDGEBASE WHERE itemId = ?");
    query.prepare(str);
    query.addBindValue(itemId);
    bool ret = query.exec();
    if (!ret){
        KyInfo() << "Error: delete knowledgeBase error" << query.lastError().text();
        return false;
    }

    return true;
}

QList<KbDocumentItem> KnowledgeBaseDatabase::getPendingDocuments()
{
    QList<KbDocumentItem> pendingList;
    if (!d->is_open) {
        KyInfo() << "Error: database is not inited.";
        return pendingList;
    }

    QSqlQuery query(d->base);
    query.prepare(
        "SELECT itemId, uuid, documentPath, documentTargetPath, status, percent, errorCode, createTime, isDir, documentSize "
        "FROM KNOWLEDGEBASE "
        "WHERE status = ? AND isDir = 0 "
        "ORDER BY itemId ASC"
    );
    query.addBindValue(static_cast<int>(DocumentStatus::PENDING));

    if (!query.exec()) {
        KyInfo() << "Error: getPendingDocuments failed: " << query.lastError().text();
        return pendingList;
    }

    while (query.next()) {
        KbDocumentItem item;
        item.itemId = query.value("itemId").toInt();
        item.uuid = query.value("uuid").toString();
        item.documentPath = query.value("documentPath").toString();
        item.documentTargetPath = query.value("documentTargetPath").toString();
        item.status = query.value("status").toInt();
        item.percent = query.value("percent").toInt();
        item.errorCode = query.value("errorCode").toInt();
        item.createTime = query.value("createTime").toString();
        item.isDir = query.value("isDir").toBool();
        item.documentSize = query.value("documentSize").toLongLong();
        
        pendingList.append(item);
    }

    KyInfo() << "Found " << pendingList.size() << " pending documents in database";
    return pendingList;
}

int KnowledgeBaseDatabase::getItemIdByUuid(const QString &uuid)
{
    if (!d->is_open) {
        KyInfo() << "The database is not open.";
        return -1;
    }

    QSqlQuery query(d->base);
    query.prepare("SELECT itemId FROM KNOWLEDGEBASE WHERE uuid = ?");
    query.addBindValue(uuid);

    if (!query.exec()) {
        KyInfo() << "Error: " << query.lastError().text();
        return -1;
    }

    if (query.next()) {
        return query.value("itemId").toInt();
    }

    return -1;
}

KbDocumentItem KnowledgeBaseDatabase::getItemByUuid(const QString &uuid)
{
    KbDocumentItem item;
    if (!d->is_open) {
        KyInfo() << "The database is not open.";
        return item;
    }

    QSqlQuery query(d->base);
    query.prepare(
        "SELECT itemId, uuid, documentPath, documentTargetPath, status, percent, errorCode, createTime, documentSize, isDir "
        "FROM KNOWLEDGEBASE "
        "WHERE uuid = ?"
    );
    query.addBindValue(uuid);

    if (!query.exec()) {
        KyInfo() << "SELECT Error: " << query.lastError().text();
        return item;
    }

    if (query.next()) {
        item.itemId = query.value("itemId").toInt();
        item.uuid = query.value("uuid").toString();
        item.documentPath = query.value("documentPath").toString();
        item.documentTargetPath = query.value("documentTargetPath").toString();
        item.status = query.value("status").toInt();
        item.percent = query.value("percent").toInt();
        item.errorCode = query.value("errorCode").toInt();
        item.createTime = query.value("createTime").toString();
        item.documentSize = query.value("documentSize").toLongLong();
        item.isDir = query.value("isDir").toBool();
        // 从documentPath提取文件名
        if (!item.documentPath.isEmpty()) {
            QFileInfo fileInfo(item.documentPath);
            item.documentName = fileInfo.fileName();
        }
    }

    return item;
}

QList<KbDocumentItem> KnowledgeBaseDatabase::getUploadingDocuments()
{
    QList<KbDocumentItem> uploadingList;
    if (!d->is_open) {
        KyInfo() << "Error: database is not inited.";
        return uploadingList;
    }

    QSqlQuery query(d->base);
    query.prepare(
        "SELECT itemId, uuid, documentPath, documentTargetPath, status, percent, errorCode, createTime, isDir, documentSize "
        "FROM KNOWLEDGEBASE "
        "WHERE status = ? AND isDir = 0 "
        "ORDER BY itemId ASC"
    );
    query.addBindValue(static_cast<int>(DocumentStatus::UPLOADING));

    if (!query.exec()) {
        KyInfo() << "Error: getUploadingDocuments failed: " << query.lastError().text();
        return uploadingList;
    }

    while (query.next()) {
        KbDocumentItem item;
        item.itemId = query.value("itemId").toInt();
        item.uuid = query.value("uuid").toString();
        item.documentPath = query.value("documentPath").toString();
        item.documentTargetPath = query.value("documentTargetPath").toString();
        item.status = query.value("status").toInt();
        item.percent = query.value("percent").toInt();
        item.errorCode = query.value("errorCode").toInt();
        item.createTime = query.value("createTime").toString();
        item.isDir = query.value("isDir").toBool();
        item.documentSize = query.value("documentSize").toLongLong();
        
        uploadingList.append(item);
    }

    KyInfo() << "Found " << uploadingList.size() << " uploading documents in database";
    return uploadingList;
}

bool KnowledgeBaseDatabase::columnExists(const QSqlDatabase &db, const QString &tableName, const QString &columnName)
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
            return true;  // 找到列
        }
    }
    return false;  // 没有找到列
}

