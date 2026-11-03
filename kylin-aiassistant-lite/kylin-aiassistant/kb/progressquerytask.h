#ifndef PROGRESSQUERYTASK_H
#define PROGRESSQUERYTASK_H

#include <QObject>
#include <QString>
#include <QList>
#include <ukui-log4qt.h>
#include <QJsonObject>
#include <QJsonDocument>

#include "knowledgebasechat.h"

class FileListModel;

class VectorizationTask : public QObject
{
    Q_OBJECT
public:
    explicit VectorizationTask(QObject *parent = nullptr);
    static VectorizationTask *mInstance;
    static VectorizationTask *getInstance();
    void setReceiver(FileListModel *receiver);
    Q_INVOKABLE void updateProgress();
    Q_INVOKABLE void addVectorizationTask(const QString &docPath, QString docUuid);
    Q_INVOKABLE void deleteVectorizationTask(QString docUuid);
    Q_INVOKABLE void resetQueryTaskList(QString docUuid);
    Q_INVOKABLE void setDocList(const QList<QString> &docList);
signals:
private:
    QList<QString> mDocList;
    FileListModel *mReceiver = nullptr;
    void uploadProgressChanged(const QString &uuid, int progress, int status, int errorCode);
    void uploadFinished();
};

#endif // PROGRESSQUERYTASK_H
