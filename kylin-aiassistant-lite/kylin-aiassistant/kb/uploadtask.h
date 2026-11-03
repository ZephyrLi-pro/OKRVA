#ifndef UPLOADTASK_H
#define UPLOADTASK_H

#include "global/kaglobal.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QString>
#include <QTime>
#include <QTimer>
#include <QJsonObject>
#include <QJsonDocument>
#include <QMutex>

class FileListModel;

class UploadTask : public QObject
{
    Q_OBJECT
public:
    explicit UploadTask(QObject *parent = nullptr);

    void setReceiver(FileListModel *receiver);

    void startWork();

    bool setFileReadOnly(const QString &filePath);

    static UploadTask *mInstance;
    static UploadTask *getInstance();
public slots:
    void copyFile(const QString &filePath, const QString &targetPath);

private:
    FileListModel *mReceiver = nullptr;
    KbDocumentItem mActiveFile; //当前活跃的正在上传文件
    int m_progress; //当前文件的拷贝进度
    bool m_isCanceled;
    void copyProgressChanged(const QString &src, const QString &dest, int progress, int status, int errorCode);
    void copyNext();
    bool isFileReadable(const QString &filePath);
    bool isTargetFile(const QString &documentPath);
    bool isFileLessThan200MB(const QString &filePath);
    bool isStorageSpaceEnough(const QString &filePath, const QString &storageDir);
    QString getMimeType(const QString &filePath);
};

#endif // UPLOADTASK_H
