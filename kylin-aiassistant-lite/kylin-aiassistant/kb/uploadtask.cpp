#include "uploadtask.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>

#include "kylin-ai/advai/knowledgebase/chatconfig.h"
#include "kylin-ai/advai/knowledgebase/chatresult.h"
#include "kylin-ai/advai/knowledgebase/chatsource.h"
#include "kylin-ai/advai/knowledgebase/session.h"

#include "filelistmodel.h"

UploadTask *UploadTask::mInstance = nullptr;

UploadTask::UploadTask(QObject *parent)
    : QObject(parent)
    , m_isCanceled(false)
{

}

UploadTask *UploadTask::getInstance()
{
    if (mInstance == nullptr) {
        mInstance = new UploadTask();
    }

    return mInstance;
}

void UploadTask::copyFile(const QString &filePath, const QString &targetPath)
{
    KyInfo() << "待拷贝文件路径分别为" << filePath << targetPath;

    mActiveFile.documentPath = filePath;
    mActiveFile.documentTargetPath = targetPath;
    mActiveFile.status =  DocumentStatus::COPYING;
    mActiveFile.errorCode = 0;
    mActiveFile.percent = 0;

    // 检查源文件是否存在
    QFile sourceFile(filePath);
    if (!sourceFile.exists()) {
        mActiveFile.percent = 0;
        mActiveFile.status =  DocumentStatus::COPY_ERROR;
        mActiveFile.errorCode = KB_ERROR_UPLOAD_DOC_NOT_EXISTS;
        copyProgressChanged(filePath, targetPath, mActiveFile.percent, mActiveFile.status, mActiveFile.errorCode);
        // 拷贝完成后（无论是拷贝错误还是拷贝成功），抛出请求，表示可以继续拷贝下一个。
        copyNext();
        return;
    }

    if (!isFileReadable(filePath)) {     // 检查文件可读性
        mActiveFile.status = COPY_ERROR;
        // 文件无法打开
        mActiveFile.errorCode = KB_ERROR_UPLOAD_DOC_CANNOT_OPEN;
        mActiveFile.statusStr = Globals::getKnowledgeUploadErrorMsg(mActiveFile.errorCode);

        KyInfo() << filePath << "   isFileReadable Failed!";

        copyProgressChanged(filePath, targetPath, mActiveFile.percent, mActiveFile.status, mActiveFile.errorCode);
        // 拷贝完成后（无论是拷贝错误还是拷贝成功），抛出请求，表示可以继续拷贝下一个。
        copyNext();
        return;
    } else if (!isTargetFile(filePath)) {     // 检查文件类型
        mActiveFile.status = COPY_ERROR;
        // 文件类型不支持
        mActiveFile.errorCode = KB_ERROR_UPLOAD_DOC_INVALID_FORMAT;
        mActiveFile.statusStr = Globals::getKnowledgeUploadErrorMsg(mActiveFile.errorCode);

        KyInfo() << filePath << "  is not TargetFile!";

        copyProgressChanged(filePath, targetPath, mActiveFile.percent, mActiveFile.status, mActiveFile.errorCode);
        // 拷贝完成后（无论是拷贝错误还是拷贝成功），抛出请求，表示可以继续拷贝下一个。
        copyNext();
        return;
    } else if (!isFileLessThan200MB(filePath)) {     // 检查文件大小
        mActiveFile.status = COPY_ERROR;
        // 文件过大
        mActiveFile.errorCode = KB_ERROR_UPLOAD_DOC_TOO_LARGE;
        mActiveFile.statusStr = Globals::getKnowledgeUploadErrorMsg(mActiveFile.errorCode);

        KyInfo() << filePath << "   LessThan200MB Failed!";

        copyProgressChanged(filePath, targetPath, mActiveFile.percent, mActiveFile.status, mActiveFile.errorCode);
        // 拷贝完成后（无论是拷贝错误还是拷贝成功），抛出请求，表示可以继续拷贝下一个。
        copyNext();
        return;
    } else if (!isStorageSpaceEnough(filePath, Globals::uploadDirPath)) {     // 检查磁盘空间大小
        mActiveFile.status = COPY_ERROR;
        // 磁盘空间不足
        mActiveFile.errorCode = KB_ERROR_UPLOAD_LOW_DISK_SPACE;
        mActiveFile.statusStr = Globals::getKnowledgeUploadErrorMsg(mActiveFile.errorCode);

        KyInfo() << filePath << "  StorageSpaceEnough Failed!";

        copyProgressChanged(filePath, targetPath, mActiveFile.percent, mActiveFile.status, mActiveFile.errorCode);
        // 拷贝完成后（无论是拷贝错误还是拷贝成功），抛出请求，表示可以继续拷贝下一个。
        copyNext();
        return;
    }

    // 打开源文件
    if (!sourceFile.open(QIODevice::ReadOnly)) {
        mActiveFile.percent = 0;
        mActiveFile.status =  DocumentStatus::COPY_ERROR;
        mActiveFile.errorCode = KB_ERROR_UPLOAD_DOC_CANNOT_OPEN;
        copyProgressChanged(filePath, targetPath, mActiveFile.percent, mActiveFile.status, mActiveFile.errorCode);
        // 拷贝完成后（无论是拷贝错误还是拷贝成功），抛出请求，表示可以继续拷贝下一个。
        copyNext();
        return;
    }

    // 创建备份目标文件
    QFile destFile(targetPath);
    if (destFile.exists()) {
        // 如果备份文件已经存在则先删除再创建新的
        destFile.remove();
    }

    if (!destFile.open(QIODevice::WriteOnly)) {
        sourceFile.close();
        mActiveFile.percent = 0;
        mActiveFile.status =  DocumentStatus::COPY_ERROR;
        mActiveFile.errorCode = KB_ERROR_UPLOAD_DOC_COPYFILE_CANNOT_CREATE;
        copyProgressChanged(filePath, targetPath, mActiveFile.percent, mActiveFile.status, mActiveFile.errorCode);
        // 拷贝完成后（无论是拷贝错误还是拷贝成功），抛出请求，表示可以继续拷贝下一个。
        copyNext();
        return ;
    }

    // 文件大小为0kb
    if (sourceFile.size() == 0) {
        m_progress = 100;
        mActiveFile.percent = m_progress * 0.1;
        mActiveFile.status =  DocumentStatus::COPY_COMPLETE;
        copyProgressChanged(filePath, targetPath, mActiveFile.percent, mActiveFile.status, mActiveFile.errorCode);
        // 拷贝完成后（无论是拷贝错误还是拷贝成功），抛出请求，表示可以继续拷贝下一个。
        copyNext();
        return;
    }

    // 获取文件总大小
    qint64 totalSize = sourceFile.size();
    qint64 copiedSize = 0;

    // 设置缓冲区大小（8KB）
    const int bufferSize = 8192;
    char *buffer = new char[bufferSize];

    QElapsedTimer elapsedTimer;
    elapsedTimer.start();

    // 循环读取并写入文件
    while (!m_isCanceled && !sourceFile.atEnd()) {
        //源文件被删除
        if (!sourceFile.exists()) {
            mActiveFile.percent = m_progress * 0.1;
            mActiveFile.status =  DocumentStatus::COPY_ERROR;
            mActiveFile.errorCode = KB_ERROR_UPLOAD_DOC_NOT_EXISTS;
            destFile.close();
            destFile.remove();
            break;
        }

        //备份文件丢失
        if (!destFile.exists()) {
            mActiveFile.percent = m_progress * 0.1;
            mActiveFile.status =  DocumentStatus::COPY_ERROR;
            mActiveFile.errorCode = KB_ERROR_UPLOAD_DOC_COPYFILE_CANNOT_CREATE;
            break;
        }

        // 读取数据
        qint64 bytesRead = sourceFile.read(buffer, bufferSize);
        if (bytesRead <= 0) {
            break;
        }

        // 写入数据
        qint64 bytesWritten = destFile.write(buffer, bytesRead);
        if (bytesWritten != bytesRead) {
            delete[] buffer;
            sourceFile.close();
            destFile.close();
            destFile.remove();
            mActiveFile.percent = m_progress * 0.1;
            mActiveFile.status =  DocumentStatus::COPY_ERROR;
            mActiveFile.errorCode = KB_ERROR_UPLOAD_DOC_COPYFILE_CANNOT_CREATE;
            copyProgressChanged(filePath, targetPath, mActiveFile.percent, mActiveFile.status, mActiveFile.errorCode);
            break;
        }

        // 更新已拷贝大小
        copiedSize += bytesWritten;

        // 计算并发送进度（避免除零错误）
        if (totalSize > 0) {
            int progress = static_cast<int>((copiedSize * 100) / totalSize);
            m_progress = progress;
            mActiveFile.percent = progress;
            if (progress == 100) {
                // 设置文件为只读
                bool readOnly = setFileReadOnly(targetPath);
                mActiveFile.status = DocumentStatus::COPY_COMPLETE;
                KyInfo() <<"当前文件拷贝完成,状态为拷贝完成，拷贝百分比为：" << progress << "备份文件设置只读状态：" << readOnly;
            }

            if (elapsedTimer.hasExpired(KB_UPDATE_PROGRESS_TIMER_INTERVAL) || progress == 100) {
                // 避免频繁给主线程发送拷贝进度变化。只每隔2s或者拷贝完成，或者拷贝错误报告进度
                copyProgressChanged(filePath, targetPath, mActiveFile.percent, mActiveFile.status, mActiveFile.errorCode);
                elapsedTimer.restart();
            }
        }
    }

    // 拷贝完成后（无论是拷贝错误还是拷贝成功），抛出请求，表示可以继续拷贝下一个。
    copyNext();

    return;
}

bool UploadTask::setFileReadOnly(const QString &filePath)
{
    QFile file(filePath);
    if (!file.exists()) {
        return false; // 文件不存在
    }

    // 获取当前文件权限
    QFile::Permissions permissions = file.permissions();
    // 移除写权限（所有者、组和其他用户）
    permissions &= ~(QFile::WriteOwner | QFile::WriteGroup | QFile::WriteOther | QFile::WriteUser);
    bool result = file.setPermissions(permissions);
    if(result) {
        KyInfo() << filePath << "文件可写权限设置成功";
    }
    else {
        KyInfo() << filePath << "文件可写权限设置失败";
    }

    return result;
}

void UploadTask::copyProgressChanged(const QString &src, const QString &dest, int progress, int status, int errorCode)
{
    if (mReceiver == nullptr) {
        KyInfo() << "Receiver is null.";
        return;
    }

    // 创建请求JSON
    QJsonObject request;
    request["msgType"] = "sendToNotifyCopyChanged";

    QJsonObject msgBody;
    msgBody["src"] = src;
    msgBody["dest"] = dest;
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

void UploadTask::copyNext()
{
    if (mReceiver == nullptr) {
        KyInfo() << "Receiver is null.";
        return;
    }

    // 创建请求JSON
    QJsonObject request;
    request["msgType"] = "sendToCopyNext";

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

bool UploadTask::isFileReadable(const QString &filePath)
{
    QFile file(filePath);
    bool result = true;
    if (!file.open(QIODevice::ReadOnly)) {
        result = false;
    }
    file.close();

    return result;
}

bool UploadTask::isTargetFile(const QString &documentPath)
{
    QFileInfo fileInfo(documentPath);
    if (!fileInfo.exists()) {
        return false; // 文件不存在，直接返回false
    }

    // 后缀名校验
    QString suffix = fileInfo.suffix().toLower();
    if (!Globals::targetSuffixes.contains(suffix)) {
        return false; // 后缀名不在目标列表中
    }

    QString mimeType = getMimeType(documentPath);
    if (mimeType == "") {
        return false;
    }

    qInfo() << "文件真实类型： " << mimeType;
    // 检查MIME类型是否匹配目标类型
    bool result = Globals::targetMimeTypes.contains(mimeType);

    return result;
}

bool UploadTask::isFileLessThan200MB(const QString &filePath)
{
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        qInfo() << "文件不存在:" << filePath;
        return false;
    }

    if (!fileInfo.isFile()) {
        qDebug() << "路径不是一个文件:" << filePath;
        return false;
    }

    const qint64 maxSize = 200 * 1024 * 1024; // 200MB
    qint64 fileSize = fileInfo.size();

    return fileSize <= maxSize;
}

bool UploadTask::isStorageSpaceEnough(const QString &filePath, const QString &storageDir)
{
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        return false;
    }

    qint64 fileSize = fileInfo.size();
    if (fileSize <= 0) {
        return true; // 空文件，任何有空间的目录都能容纳
    }

    // 检查存储目录所在磁盘的可用空间
    QStorageInfo storage(storageDir);
    if (!storage.isValid() || !storage.isReady()) {
        return false; // 存储目录无效或磁盘不可用
    }

    qint64 availableSpace = storage.bytesAvailable();
    return availableSpace > fileSize;
}

QString UploadTask::getMimeType(const QString &filePath)
{
    QProcess process;
    // 执行file命令，参数为--mime-type和文件路径
    process.start("file", QStringList() << "--mime-type" << filePath);
    // 等待命令执行完成
    if (!process.waitForFinished()) {
        return QString(); // 执行失败返回空字符串
    }

    // 读取命令输出
    QByteArray output = process.readAllStandardOutput();
    QString result = QString::fromLocal8Bit(output).trimmed();
    // 输出格式通常是"路径: MIME类型"，只需要MIME类型部分
    int colonIndex = result.indexOf(':');
    if (colonIndex != -1) {
        result = result.mid(colonIndex + 1).trimmed();
    }

    return result;
}

void UploadTask::setReceiver(FileListModel *receiver)
{
    mReceiver = receiver;
    return;
}
