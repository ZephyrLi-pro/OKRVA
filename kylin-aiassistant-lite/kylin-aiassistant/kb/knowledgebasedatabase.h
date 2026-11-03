#ifndef KNOWLEDGEBASEDATABASE_H
#define KNOWLEDGEBASEDATABASE_H

#include <memory>
#include <QSqlDatabase>
#include <QStringList>
#include <QList>
#include "global/kaglobal.h"

class KnowledgeBaseDatabase : public QObject
{
    Q_OBJECT

public:
    static KnowledgeBaseDatabase* getInstance();
    
    // 知识库表操作
    bool isKbTableEmpty();
    QStringList selectAllKnowledgeBase();
    bool insertKnowledgeBaseInfo(int parentId, KbDocumentItem kbItem, int *itemId);
    bool insertPreviousKnowledgeBaseInfo(int parentId, KbDocumentItem kbItem, int *itemId); // 插入到原来的位置
    bool updateKbTableById(QString updateInfo, int itemId);
    QList<KbDocumentItem> getChildrenByParentId(int parentId);
    QList<KbDocumentItem> getChildrenByParentIdRecursively(int folderId);
    bool deleteBatchFromKbTable(const QList<int> &batchIds);
    bool deleteFromKbTable(const int &itemId);
    int getItemIdByDocumentargetPath(QString documentTargetPath);
    int getItemIdByDocumentPath(QString documentPath);
    KbDocumentItem getItemByDocumentTargetPath(QString documentTargetPath);
    int selectKnowledgeFileTotelCount(bool isDir);
    int selectMaxHierarchyByPath(QString documentPath);
    QStringList selectDocumentPathListByItemId(int itemId);
    QList<KbDocumentItem> getPendingDocuments();
    int getItemIdByUuid(const QString &uuid);
    KbDocumentItem getItemByUuid(const QString &uuid);
    QList<KbDocumentItem> getUploadingDocuments();
    QStringList selectDocumentTargetPathListByParentId(int itemId);
    QList<int> selectAllChildDirItemIdByFilepath(int itemId);
    int selectFileCountByFilePath(QList<int> list, bool isSuccess);
    QStringList selectAllFileIdsByFilepath(QList<int> list);
    Q_INVOKABLE QVariantList getLatestFilesByFolderIdRecursively(int folderId, int limit = 5);

private:
    KnowledgeBaseDatabase();
    ~KnowledgeBaseDatabase();
    KnowledgeBaseDatabase(const KnowledgeBaseDatabase&) = delete;
    KnowledgeBaseDatabase& operator=(const KnowledgeBaseDatabase&) = delete;
    
    void initBase();
    void createKbTable();
    bool columnExists(const QSqlDatabase &db, const QString &tableName, const QString &columnName);
    QStringList getKbTableFields();
    QString buildUpdateKbTableSql(int itemId, const QJsonObject &jsonObj);

private:
    class Private;
    std::shared_ptr<Private> d;
    static KnowledgeBaseDatabase* g_instance;
};

#endif // KNOWLEDGEBASEDATABASE_H

