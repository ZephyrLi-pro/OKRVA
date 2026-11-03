#include "progressquerytask.h"

#include "filelistmodel.h"

VectorizationTask *VectorizationTask::mInstance = nullptr;

VectorizationTask::VectorizationTask(QObject *parent)
    : QObject{parent}
{

}

VectorizationTask *VectorizationTask::getInstance()
{
    if (mInstance == nullptr) {
        mInstance = new VectorizationTask();
    }

    return mInstance;
}

void VectorizationTask::updateProgress()
{
    KyInfo() << "updateProgress executed.";

    for (const QString &uuid : mDocList) {
        KyInfo() << "query uuid " << uuid;
    }

    auto it = mDocList.begin();
    while (it != mDocList.end()) {
        QString uuid = *it;
        KbDocumentItem uploadItem = KnowledgeBase::getPtr()->getUploadProgress(uuid);

        uploadProgressChanged(uuid, uploadItem.percent, uploadItem.status, uploadItem.errorCode);

        if ((uploadItem.status == DocumentStatus::UPLOAD_ERROR) ||
            (uploadItem.status == DocumentStatus::UPLOAD_COMPLETE)) {
            // 已上传完成，或者发生了上传错误，则不需要在继续保留在上传线程进行继续查询上传
            // cleanTempCopyFile(waitingUpdateItem.documentTargetPath);
            it = mDocList.erase(it);
        } else {
            ++it;
        }
    }

    if (mDocList.isEmpty()) {
        // 如果处理完了，则认为进度刷新任务已经完成了。
        uploadFinished();
    }

    return;
}

void VectorizationTask::addVectorizationTask(const QString &docPath, QString docUuid)
{
    // 拷贝完成后调用此接口，在处理之前调用模型的接口进行上传
    bool ret = KnowledgeBase::getPtr()->addDocumentByUuid(docPath, docUuid);
    if (!ret) {
        KyInfo() << "Failed to add doc. docPath " << docPath << " docUuid " << docUuid;
        return;
    }

    mDocList.append(docUuid);

    // 向量化任务添加成功后，将状态设置为DocumentStatus::UPLOAD_ERROR
    uploadProgressChanged(docUuid, 0, DocumentStatus::UPLOADING, 0);

    return;
}

void VectorizationTask::deleteVectorizationTask(QString docUuid)
{
    KyInfo() << "deleteVectorizationTask: " << docUuid;
    
    // 从进度查询列表中删除该任务
    auto it = mDocList.begin();
    while (it != mDocList.end()) {
        if (*it == docUuid) {
            KyInfo() << "Removed docUuid from progress query list: " << docUuid;
            it = mDocList.erase(it);
            break;
        } else {
            ++it;
        }
    }
    
    // 如果列表为空，通知上传完成
    if (mDocList.isEmpty()) {
        uploadFinished();
    }
    
    return;
}

void VectorizationTask::resetQueryTaskList(QString docUuid)
{

}

void VectorizationTask::uploadProgressChanged(const QString &uuid, int progress, int status, int errorCode)
{
    if (mReceiver == nullptr) {
        KyInfo() << "Receiver is null.";
        return;
    }

    // 创建请求JSON
    QJsonObject request;
    request["msgType"] = "sendToNotifyUploadChanged";

    QJsonObject msgBody;
    msgBody["uuid"] = uuid;
    msgBody["progress"] = progress;
    msgBody["status"] = status;
    msgBody["errorCode"] = errorCode;

    request["msgBody"] = msgBody;

    // 发送异步请求
    QString req = QJsonDocument(request).toJson();

    // 使用QMetaObject::invokeMethod异步调用
    bool result = QMetaObject::invokeMethod(mReceiver,
                                            "sendMessage",
                                            Qt::QueuedConnection,
                                            Q_ARG(const QString, req));


    return;
}

void VectorizationTask::uploadFinished()
{
    if (mReceiver == nullptr) {
        KyInfo() << "Receiver is null.";
        return;
    }

    // 创建请求JSON
    QJsonObject request;
    request["msgType"] = "sendToNotifyUploadFinished";

    QJsonObject msgBody;

    request["msgBody"] = msgBody;

    // 发送异步请求
    QString req = QJsonDocument(request).toJson();

    // 使用QMetaObject::invokeMethod异步调用
    bool result = QMetaObject::invokeMethod(mReceiver,
                                            "sendMessage",
                                            Qt::QueuedConnection,
                                            Q_ARG(const QString, req));


    return;
}

void VectorizationTask::setReceiver(FileListModel *receiver)
{
    mReceiver = receiver;
    return;
}

void VectorizationTask::setDocList(const QList<QString> &docList)
{
    mDocList = docList;
    KyInfo() << "Set docList with " << mDocList.size() << " items";
    return;
}
