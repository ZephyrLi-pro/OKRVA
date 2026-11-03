#ifndef DOCUMENTREFERENCEDATABASE_H
#define DOCUMENTREFERENCEDATABASE_H

#include <QString>
#include <QSqlDatabase>
#include <memory>

#include "global/kaglobal.h"

class DocumentReferenceDatabase
{
public:
    DocumentReferenceDatabase();
    ~DocumentReferenceDatabase();

    // 获取单例实例
    static DocumentReferenceDatabase* getInstance();

    // 插入文件引用信息（每个会话只保留最新的文件引用）
    bool insertDocumentReference(const QString &sessionID, const QString &fileType, const QString &documents);

    // 根据会话ID查询文件引用信息
    QString selectDocumentReferenceBySessionID(const QString &sessionID);

    // 根据会话ID删除文件引用信息
    bool deleteDocumentReferenceBySessionID(const QString &sessionID);

private:
    DocumentReferenceDatabase(const DocumentReferenceDatabase&) = delete;
    DocumentReferenceDatabase& operator=(const DocumentReferenceDatabase&) = delete;

    // 初始化数据库连接
    void initBase();

    // 创建文件引用表
    void createTable();

    // 检查列是否存在
    bool columnExists(const QSqlDatabase &db, const QString &tableName, const QString &columnName);

private:
    class Private;
    std::shared_ptr<Private> d;
    static DocumentReferenceDatabase* g_instance;
};

#endif // DOCUMENTREFERENCEDATABASE_H

