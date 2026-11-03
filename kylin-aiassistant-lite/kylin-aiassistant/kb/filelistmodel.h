#ifndef FILELISTMODEL_H
#define FILELISTMODEL_H

#include <QObject>
#include <QAbstractListModel>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <ukui-log4qt.h>
#include <QJsonObject>
#include <QJsonDocument>
#include <QStorageInfo>
#include <QTimer>
#include <QThread>
#include <QDateTime>
#include "global/kaglobal.h"
#include "kb/knowledgebasedatabase.h"
#include "kb/uploadtask.h"
#include "cmainwnd.h"
#include <QUrl>
#include <QQueue>
#include <QIcon>
#include <QFileIconProvider>

#include "progressquerytask.h"

#define KB_REQ_PARA_ERROR R"({"msgType": "error", "msgBody":{"errorMsg" : "Request parameter error."}})"
#define KB_RET_SUCCESS R"({"msgType": "success"})"

class FileListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(bool isKbEmpty READ isKbStatusEmpty NOTIFY isKbEmptyChanged)
    Q_PROPERTY(QString uploadRootPath READ uploadRootPath CONSTANT)
    Q_PROPERTY(QString currentFolderPathJson READ currentFolderPathJson NOTIFY folderPathChanged)
    Q_PROPERTY(int fileTotelCount READ fileTotelCounts NOTIFY fileTotelCountyChanged)

public:
    enum KbDocumentItemRoles {
        UuidRole = Qt::UserRole + 1,
        DocumentNameRole,
        DocumentPathRole,
        DocumentTargetPathRole,
        DocumentSizeRole,
        DocumentTypeRole,
        StatusRole,
        PercentRole,
        ErrorCodeRole,
        CreateTimeRole,
        DocumentSizeStrRole,
        IsDirRole,
        Checked,
        FileIconRole,
        StatusStrRole,
        ItemIdRole
    };

    explicit FileListModel(QObject *parent = nullptr);
    explicit FileListModel(const QString directoryPath, QObject *parent = nullptr);

    Q_INVOKABLE void loadDirectory(const QString &directoryPath);
    QString uploadRootPath() const { return m_uploadRootPath; }
    QVariantList currentFolderPath() const { return m_folderHierarchy; }
    QString currentFolderPathJson() const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Qt::ItemFlags flags(const QModelIndex &index) const;

    bool setData(const QModelIndex &index, const QVariant &value, int role);

    Q_INVOKABLE bool isKbStatusEmpty();
    /* 除了重写或者覆盖基类 QAbstractListModel 的方法，所有消息都通过这个接口从QML端发送消息，禁止添加额外接口，应通过在参数里新增字段。该参数为一个json对象。
     * 遵守的格式如下，其中msgType为字符串，标识当前消息的分类。msgBody为一个json对象，用于携带具体的参数。
     * {"msgType":"xxx", "msgBody":{}}
     * 返回值格式为： {"msgType":"xxx", "msgBody":{}}
     */
    Q_INVOKABLE QString sendMessage(const QString message);
    Q_INVOKABLE int fileTotelCounts();
signals:
    void countChanged();
    void isKbEmptyChanged();
    void folderPathChanged();
    void fileTotelCountyChanged();

private:
    QList<KbDocumentItem> mFileList;
    QString mDirectoryPath;
    KnowledgeBaseDatabase *mDatabase = nullptr;    //数据库
    QTimer *mUpdateTimer = nullptr;
    int mCurrentFolderId = 0;
    void loadFromDirectory(const QString &directoryPath);
    QString getFileCreateTime(const QFileInfo &fileInfo);
    QString getFileSize(const QFileInfo &fileInfo);
    QString getFileSizeFromBytes(qint64 size);
    QString sendToAddDocument(const QJsonObject &data);
    QString sendToDeleteDocument(const QJsonObject &data);
    QString sendToCreateDir(const QJsonObject &data);
    QString sendToNotifyCopyChanged(const QJsonObject &data);
    QString sendToNotifyUploadChanged(const QJsonObject &data);
    QString sendToNotifyUploadFinished(const QJsonObject &data);
    QString sendToCopyNext(const QJsonObject &data);
    QString sendToInitUploadThread(const QJsonObject &data);
    QString sendToGetCurrentFiles(const QJsonObject &data);
    QString sendToRename(const QJsonObject &data);
    QString sendToMove(const QJsonObject &data);
    QString sendToCopy(const QJsonObject &data);
    QString sendToGetDirHierarchy(const QJsonObject &data);
    QString sendToIsCanMove(const QJsonObject &data);
    QString sendToGetRepeatFileNames(const QJsonObject &data);
    QString sendToGetAllFileNames(const QJsonObject &data);
    QString sendToCheckSpaceCapacity(const QJsonObject &data);
    QString sendToGetAllFileCountByPath(const QJsonObject &data);
    QString sendToGetIsAllComplete(const QJsonObject &data);
    void onCopyDocument(const KbDocumentItem &item);
    void initUploadThread();
    void updateUploadMsg();
    void rebuildWaitingQueue();
    void rebuildVectorizationTaskList();
    void removeFileFromDisk(const QString &copyFilePath);
    void removeFromWaitingQueue(const QString &uuid, const QString &documentTargetPath);
    void cancelUploadingDocument(const QString &uuid);
    void deleteUploadedDocument(const QString &uuid);
    bool deleteUploadedDocumentList(const QStringList &uuidList); // 批量删除接口
    void deleteDocument(const QString &uuid);
    void addDir(const QString &dirPath);
    QString getStatusStr(int status);
    QString getDocumentTargetPathByName(const QString name);
    QString getDocumentTargetPathByNameAndTargetPath(const QString targetPath, const QString name);
    void addSingleDocument(const QString &filePath);
    QVariantList getFolderHierarchy(const QString &fullPath);
    QString m_uploadRootPath;
    QVariantList m_folderHierarchy;
    QString getIconPathBySuffix(const QString &suffix) const;
    QString getFileIconPath(const QString &filePath) const;
    int getItemIdByPath(const QString &path);
    KbDocumentItem *getKbDocumentItemByDocumentTargetPath(const QString &documentTargetPath);
    int getIndexByTargetPath(const QString &documentTargetPath);
    int getDirCount();
    void deleteDir(const KbDocumentItem &folderItem);
    void copyAsync(const KbDocumentItem &item);
    void addUploadTask(const QString &docPath, const QString docUuid);
    bool renameSingleFile(int rowIndex, QString newName);
    bool renameSingleDir(int rowIndex, QString newName);
    bool moveSingleFile(const QString targetPath, const QString &sourcePath);
    bool moveSingleDir(const QString targetPath, const QString &sourcePath);
    bool copySingleFile(const QString targetPath, const QString &sourcePath);
    bool copySingleDir(const QString targetPath, const QString &sourcePath);
    void copyDirChildrenRecursively(int sourceDirId, int targetDirId, const QString &targetDirPath,
                                    const QList<KbDocumentItem> &allChildren,
                                    const QMap<int, KbDocumentItem> &descendantItemMap);
    KbDocumentItem *getKbDocumentItemByPath(const QString &documentTargetPath);
    int selectMaxHierarchyByPath(const QString &sourcePath);
    bool checkDirIsCanMove(const QString &sourcePath);
    QString getCommonRootDirectory(const QStringList& pathList);
    QStringList getAllFileNameByDocumentPath(const QString &sourcePath);
    int getAllFileCountByDIr(int itemId);

};

#endif // FILELISTMODEL_H
