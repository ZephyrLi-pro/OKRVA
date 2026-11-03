#include <QJsonArray>
#include <QJsonDocument>
#include "filelistmodel.h"
#include "knowledgebasechat.h"
#include "knowledgebasespacemanager.h"

#define MAX_DOC_COUNT 100
#define MAX_ACTIVE_DOC_COUNT 10

// 只会在主线程访问和修改待上传列表，因此不会存在数据竞争
QQueue<KbDocumentItem> g_waitingDocList;
UploadTask *g_uploadTask = nullptr;
VectorizationTask *g_vectorizationTask = nullptr;
QThread *g_progressQueryThread = nullptr;
QThread *g_uploadThread = nullptr;

FileListModel::FileListModel(QObject *parent) : QAbstractListModel (parent)
{
    qint64 totalStartTime = QDateTime::currentMSecsSinceEpoch();

    QString homePath = QDir::homePath();
    QString configPath = homePath + KB_UPLOAD_DIR;
    m_uploadRootPath = configPath;

    // 检查并创建文件夹
    QDir configDir(configPath);
    if (!configDir.exists()) {
        bool success = configDir.mkpath(".");
        if (!success) {
            KyInfo() << "Failed to create directory:" << configPath;
        } else {
            KyInfo() << "Directory created successfully:" << configPath;
        }
    }

    mDirectoryPath = configPath;

    mDatabase = KnowledgeBaseDatabase::getInstance();

    // 初始化知识库空间管理器
    KnowledgeBaseSpaceManager::getInstance()->initialize();

    loadFromDirectory(configPath);

    qint64 totalEndTime = QDateTime::currentMSecsSinceEpoch();
    KyInfo() << "*** KnowledgeBaseDialog create end *** 总结束时间=" << totalEndTime << "ms，总耗时=" << (totalEndTime - totalStartTime) << "ms";
}

FileListModel::FileListModel(const QString directoryPath, QObject *parent) : QAbstractListModel (parent)
{
    mDirectoryPath = directoryPath;
    loadFromDirectory(directoryPath);
}

int FileListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return mFileList.size();
}

QVariant FileListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= mFileList.size()) {
        return QVariant();
    }

    const KbDocumentItem &item = mFileList.at(index.row());

    switch (role) {
        case UuidRole:
            return item.uuid;
        case DocumentNameRole:
            return item.documentName;
        case DocumentPathRole:
            return item.documentPath;
        case DocumentTargetPathRole:
            return item.documentTargetPath;
        case DocumentSizeRole:
            return item.documentSize;
        case DocumentTypeRole:
            return item.documentType;
        case StatusRole:
            return item.status;
        case PercentRole:
            return item.percent;
        case ErrorCodeRole:
            return item.errorCode;
        case CreateTimeRole:
            return item.createTime;
        case DocumentSizeStrRole:
            return item.documentSizeStr;
        case IsDirRole:
            return item.isDir;
        case Checked:
            return item.checked;
        case StatusStrRole:
            return item.statusStr;
        case FileIconRole:
            return item.fileIcon;
        case ItemIdRole:
            return item.itemId;
        default:
            return QVariant();
    }

    return QVariant();
}

QHash<int, QByteArray> FileListModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[UuidRole] = "uuid";
    roles[DocumentNameRole] = "documentName";
    roles[DocumentPathRole] = "documentPath";
    roles[DocumentTargetPathRole] = "documentTargetPath";
    roles[DocumentSizeRole] = "documentSize";
    roles[DocumentTypeRole] = "documentType";
    roles[StatusRole] = "status";
    roles[PercentRole] = "fileProgressNum";
    roles[ErrorCodeRole] = "errorCode";
    roles[CreateTimeRole] = "createTime";
    roles[DocumentSizeStrRole] = "fileSize";
    roles[IsDirRole] = "isDir";
    roles[Checked] = "checked";
    roles[FileIconRole] = "fileIcon";
    roles[StatusStrRole] = "statusStr";
    roles[ItemIdRole] = "itemId";

    return roles;
}

void FileListModel::loadDirectory(const QString &directoryPath)
{
    // 传入的必须是一个绝对路径
    KyInfo() << "directoryPath " << directoryPath;
    QDir configDir(directoryPath);
    if (!configDir.exists()) {
        return;
    }

    m_folderHierarchy = getFolderHierarchy(directoryPath);
    emit folderPathChanged();

    beginResetModel();
    loadFromDirectory(directoryPath);
    endResetModel();
    emit countChanged();
    Q_EMIT isKbEmptyChanged();

    mDirectoryPath = directoryPath;
}

QString FileListModel::currentFolderPathJson() const {
    QJsonArray jsonArray;
    for (const QVariant &v : m_folderHierarchy) {
        jsonArray.append(v.toString());
    }
    QJsonDocument doc(jsonArray);
    QString jsonStr = doc.toJson(QJsonDocument::Compact);
    return jsonStr;
}

Qt::ItemFlags FileListModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) return Qt::NoItemFlags;
    // 必须包含 Qt::ItemIsEditable
    return Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled;
}

bool FileListModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    // 1. 校验索引有效性
    if (!index.isValid() || index.row() < 0 || index.row() >= mFileList.size()) {
        KyInfo() << "Invalid index for setData:" << index.row();
        return false;
    }

    // 2. 获取当前项并修改对应角色的数据
    KbDocumentItem &item = mFileList[index.row()];  // 注意用引用（&），确保修改原数据
    bool dataChangedFlag = false;

    switch (role) {
        case Checked: {
            bool newChecked = value.toBool();
            if (item.checked != newChecked) {
                item.checked = newChecked;
                dataChangedFlag = true;
            }
            break;
        }
        case PercentRole: {
            int newProgress = value.toInt();
            // 校验进度范围（0-100）
            if (newProgress >= 0 && newProgress <= 100 && item.percent != newProgress) {
                item.percent = newProgress;
                dataChangedFlag = true;
            }
            break;
        }
        case StatusRole: {
            int newStatus = value.toInt();
            if (item.status != newStatus) {
                item.status = newStatus;
                dataChangedFlag = true;
            }
            break;
        }
        case DocumentNameRole: {
            QString newName = value.toString();
            if (!newName.isEmpty() && item.documentName != newName) {
                item.documentName = newName;
                dataChangedFlag = true;
            }
            break;
        }
        // 其他角色（如FileSize、CreateTime）如需修改，可在此补充case
        default:
            KyInfo() << "Unsupported role for setData:" << role;
            return false;
    }

    // 3. 若数据修改，发射信号通知QML视图刷新
    if (dataChangedFlag) {
        QModelIndex idx = this->index(index.row()); // 第3行（0开始）
        flags(idx);
        emit dataChanged(idx, idx, {role});  // 仅刷新当前项的当前角色
        return true;
    }

    return false;
}

void FileListModel::loadFromDirectory(const QString &directoryPath)
{
    QDir dir(directoryPath);
    if (!dir.exists()) {
        KyInfo() << "Directory does not exist:" << directoryPath;
        return;
    }

    // 根据路径查找需要加载的的文件夹的itemId，然后用itemId里去数据库检索，parentId为此itemId的所有记录。即为其包含的文件夹和文件。
    int itemId = getItemIdByPath(directoryPath);
    if (itemId == -1) {
        KyInfo() << "Cannot find the itemId :" << directoryPath;
        return;
    }

    if (itemId == 0) {
        // 0虚拟为upload文件夹。
        KyInfo() << "Invalid itemId :" << directoryPath;
    }

    // 更新为当前文件夹的 itemId
    mCurrentFolderId = itemId;
    // 加载文件夹的时候需要更新 mFileList 数据。g_waitingDocList数据需要保存全部的，不需要更新。
    mFileList.clear();

    QList<KbDocumentItem> children = mDatabase->getChildrenByParentId(itemId);

    // 顺序扫描从数据库返回的列表，文件夹存储在文件夹，文件的存储在文件临时列表，然后合并。
    QList<KbDocumentItem> dirList;
    QList<KbDocumentItem> fileList;

    for (KbDocumentItem &childrenItem : children) {
        KbDocumentItem item;
        item = childrenItem;

        QFileInfo fileInfo(item.documentTargetPath);
        item.fileIcon = getFileIconPath(item.documentTargetPath);
        item.documentName = fileInfo.fileName();
        item.isDir = fileInfo.isDir();
        // 从数据库读取文件大小
        item.documentSizeStr = getFileSizeFromBytes(item.documentSize);
        if (fileInfo.isDir()) {
            item.isDir = true;
        } else {
            item.isDir = false;
        }

        KyInfo() << "item documentTargetPath : " << item.documentTargetPath;

        if (!item.isDir) {
            // 优先使用数据库中保存的状态和错误码
            // 只有当文件状态是上传中时，才需要从SDK获取最新进度
            // 对于已上传错误或已完成的文件，使用数据库中保存的状态，避免错误信息丢失
            if (item.status == DocumentStatus::UPLOADING) {
                // 目前调试发现正在上传中的文件，助手退出后再次拿文档id去查询，会查不到。SDK给的结论是上传中的文件是跟session绑定的。
                KbDocumentItem uploadItem = KnowledgeBase::getPtr()->getUploadProgress(item.uuid);
                if (uploadItem.errorCode != static_cast<int>(AI_COMMON_SUCCESS)) {
                    //continue;
                }
                KyInfo() << "documentTargetPath : " << uploadItem.documentTargetPath << "errorCode : "  << uploadItem.errorCode;
                /* 文件的上传进度每次加载文件夹的时候，重新从SDK读取。待办：如果进度不是完成状态的，需要放到上传队列里。
                 * 如果助手没有退的情况下，只是加载新的文件夹，是不需要把没完成放入待上传队列里，因为此时待上传数据是完整的。
                 * 如果是助手退出的情况下，再次运行，初始的时候上传队列是空的，此时需要将上传进度没完成的加到上传队列里（退出时当前正在上传的那一个，其余的忽略不做处理）。
                 */
                item.percent = uploadItem.percent;
                item.status = uploadItem.status;
                item.errorCode = uploadItem.errorCode;
            } else {
                // 对于非上传中状态的文件（包括错误、完成等），使用数据库中保存的状态和错误码
                // 这样即使添加新文件导致重新加载列表，错误信息也不会丢失
                KyInfo() << "Using database saved status for item: " << item.documentTargetPath 
                         << " status: " << item.status << " errorCode: " << item.errorCode;
            }
            
            // 设置状态字符串
            if (item.errorCode == 0) {
                item.statusStr = getStatusStr(item.status);
            } else {
                item.statusStr = Globals::getKnowledgeUploadErrorMsg(item.errorCode);
            }
        }

        if (fileInfo.isDir()) {
            dirList.append(item);
        } else {
            fileList.append(item);
        }
    }

    mFileList.append(dirList);
    mFileList.append(fileList);

    return;
}

QString FileListModel::getFileCreateTime(const QFileInfo &fileInfo)
{
    // 显示的文件的时间为文件添加到列表的时间，也即此刻的时间。
    QDateTime createTime = QDateTime::currentDateTime();

    return createTime.toString("yyyy/MM/dd HH:mm:ss");
}

QString FileListModel::getFileSize(const QFileInfo &fileInfo)
{
    qint64 size = fileInfo.size();
    return getFileSizeFromBytes(size);
}

QString FileListModel::getFileSizeFromBytes(qint64 size)
{
    const double KB = 1024.0;
    const double MB = KB * 1024.0;

    double displaySize = 0.0;
    QString unit;

    // 确定单位和基础数值
    if (size < KB) {
        displaySize = static_cast<double>(size);
        unit = "B";
    } else if (size < MB) {
        displaySize = size / KB;
        unit = "KB";
    } else {
        displaySize = size / MB;
        unit = "MB";
    }

    // 核心逻辑：按规则格式化数值
    // 1. 先保留两位小数，再判断是否需要简化显示
    QString sizeStr = QString::number(displaySize, 'f', 2);

    // 去掉末尾无意义的0，但保留至少一位小数（确保.0的显示）
    if (sizeStr.endsWith(".00")) {
        sizeStr.replace(".00", ".0"); // 整数→X.0
    } else if (sizeStr.endsWith("0")) {
        sizeStr.chop(1); // 一位小数→X.X（去掉末尾的0）
    }

    return sizeStr + unit;
}

QString FileListModel::sendMessage(const QString message)
{
    QJsonObject data = QJsonDocument::fromJson(message.toUtf8()).object();

    KyInfo() << "knowledge sendMessage==" << data;

    if (!data.contains("msgType")) {
        KyInfo() << "Not carried msgType!";
        return KB_REQ_PARA_ERROR;
    }

    if (!data["msgType"].isString()) {
        KyInfo() << "The type is not a string!";
        return KB_REQ_PARA_ERROR;
    }

    QString msgType = data["msgType"].toString();
    if (msgType == "sendToAddDocument") {
        return sendToAddDocument(data);
    }

    if (msgType == "sendToDeleteDocument") {
        return sendToDeleteDocument(data);
    }

    if (msgType == "sendToCreateDir") {
        return sendToCreateDir(data);
    }

    if (msgType == "sendToNotifyCopyChanged"){
        return sendToNotifyCopyChanged(data);
    }

    if (msgType == "sendToNotifyUploadChanged"){
        return sendToNotifyUploadChanged(data);
    }

    if (msgType == "sendToNotifyUploadFinished"){
        return sendToNotifyUploadFinished(data);
    }

    if (msgType == "sendToCopyNext"){
        return sendToCopyNext(data);
    }

    if (msgType == "sendToInitUploadThread"){
        return sendToInitUploadThread(data);
    }

    if (msgType == "sendToGetCurrentFiles"){
        return sendToGetCurrentFiles(data);
    }

    if (msgType == "sendToRename"){
        return sendToRename(data);
    }

    if (msgType == "sendToMove"){
        return sendToMove(data);
    }

    if (msgType == "sendToCopy"){
        return sendToCopy(data);
    }
    // 获取文件层级
    if (msgType == "sendToGetDirHierarchy"){
        return sendToGetDirHierarchy(data);
    }

    // 文件夹是否可以移动
    if (msgType == "sendToIsCanMove"){
        return sendToIsCanMove(data);
    }

    // 移动复制操作获取重名文件
    if (msgType == "sendToGetRepeatFileNames"){
        return sendToGetRepeatFileNames(data);
    }

    // 获取当前上传路径下的全部文件名
    if (msgType == "sendToGetAllFileNames"){
        return sendToGetAllFileNames(data);
    }

    // 判断知识库空间容量是否够用
    if (msgType == "sendToCheckSpaceCapacity"){
        return sendToCheckSpaceCapacity(data);
    }

    // 获取路径下全部文件数量
    if (msgType == "sendToGetAllFileCountByPath"){
        return sendToGetAllFileCountByPath(data);
    }

    // 获取路径下全部文件是否都上传成功
    if (msgType == "sendToGetIsAllComplete"){
        return sendToGetIsAllComplete(data);
    }

    KyInfo() << "The type is not handled! " << msgType;

    return "";
}

int FileListModel::fileTotelCounts()
{
    return mDatabase->selectKnowledgeFileTotelCount(false);
}

void FileListModel::deleteDir(const KbDocumentItem &folderItem)
{
    KyInfo() << "deleteDir : " << folderItem.documentTargetPath << " itemId " << folderItem.itemId;

    QString directoryPath = folderItem.documentTargetPath;

    // 首先判断文件夹是否存在，如果存在则删除文件夹。如果不存在，可能是添加的错误项，继续后续的删除数据库信息。
    if (!directoryPath.isEmpty() && QDir().exists(directoryPath)) {
        QFileInfo fileInfo(directoryPath);
        if (fileInfo.isDir()) {
            QDir dir(directoryPath);
            if(!dir.removeRecursively()) {
                KyInfo() << "Failed to remove recursively.";
            }
        }
    }

    QList<KbDocumentItem> children = mDatabase->getChildrenByParentIdRecursively(folderItem.itemId);
    
    // 计算文件夹内所有文件的大小总和，并处理待上传队列中的文件
    qint64 totalSize = 0;
    for (const KbDocumentItem &child : children) {
        if (!child.isDir) {
            totalSize += child.documentSize;
            // 如果子文件是待上传状态，需要从待上传队列中删除
            if (child.status == DocumentStatus::PENDING) {
                removeFromWaitingQueue(child.uuid, child.documentTargetPath);
            }
            
            // 如果子文件是上传中状态，需要从进度查询线程中删除任务，并取消知识库的上传
            if (child.status == DocumentStatus::UPLOADING) {
                cancelUploadingDocument(child.uuid);
            }
            
            // 如果子文件已上传成功，需要调用知识库接口删除已向量化的文档
//            if (child.status == DocumentStatus::UPLOAD_COMPLETE) {
//                deleteUploadedDocument(child.uuid);
//            }
        }
    }
    
    // 更新知识库空间容量
    if (totalSize > 0) {
        KnowledgeBaseSpaceManager::getInstance()->removeSpace(totalSize);
    }
    
    QList<int> batchIds;
    for (int i = 0; i < children.size(); ++i) {
        batchIds.append(children[i].itemId);
    }

    batchIds.append(folderItem.itemId);

    bool ret = mDatabase->deleteBatchFromKbTable(batchIds);
    if (!ret) {
        KyInfo() << "Failed to delete batchIds " << batchIds;
    }

    Q_EMIT fileTotelCountyChanged();

    // TODO:删除向量化信息

    return;
}

void FileListModel::addSingleDocument(const QString &filePath)
{
    QFileInfo fileInfo(filePath);
    KbDocumentItem item;
    item.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    item.documentName = fileInfo.fileName();
    item.documentPath = filePath;
    item.documentSize = fileInfo.size();
    item.createTime = getFileCreateTime(fileInfo);
    item.documentSizeStr = getFileSize(fileInfo);

    QString iconPath = "file://" + filePath;
    if (fileInfo.isDir()){
        item.isDir = true;
        item.documentType = "inode-directory";
    } else {
        item.isDir = false;
        item.documentType = Globals::getFileIconName(iconPath);
    }

    item.status = PENDING; // 初始状态
    item.percent = 0;
    item.errorCode = 0;
    item.documentTargetPath = getDocumentTargetPathByName(item.documentName);
    item.statusStr = getStatusStr(item.status);

    int itemId = 0;
    // 保存到数据库
    bool ret = mDatabase->insertKnowledgeBaseInfo(mCurrentFolderId, item, &itemId);
    if (!ret) {
        // todo: 插入数据库失败应该给UI端提示
        KyInfo() << "Failed to insert file info.";
    }

    item.itemId = itemId;

    item.fileIcon = getFileIconPath(filePath);
    KyInfo() << "item.fileIcon" << item.fileIcon;
    if (item.isDir) {
        // 文件夹插入在首行
        beginInsertRows(QModelIndex(), 0, 0);
        mFileList.prepend(item);
        endInsertRows();
    } else {
        int dirCnt = getDirCount();
        // 普通文件插入在文件夹后的首行
        beginInsertRows(QModelIndex(), dirCnt, dirCnt);
        mFileList.insert(dirCnt, item);
        endInsertRows();
    }

    Q_EMIT fileTotelCountyChanged();
    Q_EMIT countChanged();

    // 更新知识库空间容量
    KnowledgeBaseSpaceManager::getInstance()->addSpace(fileInfo.size());

    if (item.status == PENDING) {
        // 如果是准备上传状态，提交给拷贝线程进行拷贝
        onCopyDocument(item);
    }

    return;
}

QString FileListModel::sendToAddDocument(const QJsonObject &data)
{
    KyInfo() << "sendToAddDocument begin ";

    if (!data.contains("msgBody")) {
        KyInfo() << "Not carried msgBody!";
        return KB_REQ_PARA_ERROR;
    }

    if (!data["msgBody"].isObject()) {
        KyInfo() << "The type is not an object!";
        return KB_REQ_PARA_ERROR;
    }

    QJsonObject msgBody = data["msgBody"].toObject();

    if(!msgBody.contains("uploadFilePath") || !msgBody["uploadFilePath"].isArray()) {
        KyInfo() << "Not carried uploadFilePath!";
        return KB_REQ_PARA_ERROR;
    }

    // 添加单个或者多个文件，都以数组的形式传入
    QJsonArray fileArray = msgBody["uploadFilePath"].toArray();
    for (const QJsonValue &filePath : fileArray) {
        if (!filePath.isString()) {
            KyInfo() << "Invalid file path";
        }

        QString filePathValue = filePath.toString();
        if (filePathValue.isEmpty() || !QFile::exists(filePathValue)) {
            KyInfo() << "Invalid file path:" << filePathValue;
        }

        addSingleDocument(filePathValue);
    }

    KyInfo() << "sendToAddDocument end ";

    return KB_RET_SUCCESS;
}

void FileListModel::onCopyDocument(const KbDocumentItem &item)
{
    KyInfo() << "onCopyDocument  " << item.documentTargetPath;
    KyInfo() << "g_waitingDocList.isEmpty()  " << g_waitingDocList.isEmpty();
    // 待拷贝队列为主线程维护和访问，不存在数据竞争
    if(!g_waitingDocList.isEmpty()) {
        // 已经有在排队的了，继续加入队列，一起排队
        g_waitingDocList.enqueue(item);
        return;
    }

    //启动拷贝线程
   if (g_uploadThread && !g_uploadThread->isRunning()) {
       g_uploadThread->start();
   }

   KyInfo() << "g_uploadTask  " << g_uploadTask;
    //执行拷贝任务
   if(g_uploadTask) {
       copyAsync(item);
   }

    return;
}

void FileListModel::initUploadThread()
{
    g_uploadThread = new QThread(this);

    g_uploadTask = UploadTask::getInstance();
    g_uploadTask->setReceiver(this);
    g_uploadTask->moveToThread(g_uploadThread);


    g_progressQueryThread = new QThread(this);
    g_vectorizationTask = VectorizationTask::getInstance();
    g_vectorizationTask->setReceiver(this);
    g_vectorizationTask->moveToThread(g_progressQueryThread);

    if (!mUpdateTimer) {
        mUpdateTimer = new QTimer(this);
    }
    mUpdateTimer->setInterval(KB_UPDATE_PROGRESS_TIMER_INTERVAL);
    connect(mUpdateTimer, &QTimer::timeout, this, &FileListModel::updateUploadMsg);

    // 重建上传队列：从数据库读取所有 PENDING 状态的记录并加入队列
    rebuildWaitingQueue();

    // 重建进度查询队列：从数据库读取所有 UPLOADING 状态的记录并加入队列
    rebuildVectorizationTaskList();

    return;
}

void FileListModel::rebuildWaitingQueue()
{
    KyInfo() << "Rebuilding waiting queue from database...";
    
    // 从数据库查询所有 PENDING 状态的记录
    QList<KbDocumentItem> pendingItems = mDatabase->getPendingDocuments();
    
    if (pendingItems.isEmpty()) {
        KyInfo() << "No pending documents found in database";
        return;
    }
    
    KyInfo() << "Found " << pendingItems.size() << " pending documents, adding to queue";
    
    // 将 PENDING 状态的记录加入到队列中
    // 注意：即使原文件路径不存在，也加入队列，拷贝线程会负责检查原文件是否存在
    for (const KbDocumentItem &item : pendingItems) {
        // 补充缺失的信息
        KbDocumentItem queueItem = item;
        
        queueItem.fileIcon = getFileIconPath(item.documentPath);
        queueItem.documentSizeStr = getFileSizeFromBytes(queueItem.documentSize);
        
        queueItem.statusStr = getStatusStr(queueItem.status);
        
        g_waitingDocList.enqueue(queueItem);
    }
    
    KyInfo() << "Rebuilt waiting queue with " << g_waitingDocList.size() << " items";
    
    // 如果队列不为空，启动拷贝线程并开始处理第一个任务
    if (!g_waitingDocList.isEmpty()) {
        // 启动拷贝线程
        if (g_uploadThread && !g_uploadThread->isRunning()) {
            g_uploadThread->start();
        }
        
        // 开始处理队列中的第一个任务
        if (g_uploadTask && !g_waitingDocList.isEmpty()) {
            KbDocumentItem firstItem = g_waitingDocList.dequeue();
            copyAsync(firstItem);
            KyInfo() << "Started copying first pending document: " << firstItem.documentPath;
        }
    }
    
    return;
}

void FileListModel::rebuildVectorizationTaskList()
{
    KyInfo() << "Rebuilding vectorization task list from database...";
    
    // 从数据库查询所有 UPLOADING 状态的记录
    QList<KbDocumentItem> uploadingItems = mDatabase->getUploadingDocuments();
    
    if (uploadingItems.isEmpty()) {
        KyInfo() << "No uploading documents found in database";
        return;
    }
    
    KyInfo() << "Found " << uploadingItems.size() << " uploading documents, adding to mDocList";
    
    // 构建 uuid 列表
    QList<QString> uuidList;
    for (const KbDocumentItem &item : uploadingItems) {
        if (!item.uuid.isEmpty()) {
            uuidList.append(item.uuid);
        }
    }
    
    if (uuidList.isEmpty()) {
        KyInfo() << "No valid uuids found in uploading documents";
        return;
    }
    
    // 设置到 VectorizationTask 的 mDocList
    if (g_vectorizationTask) {
        bool result = QMetaObject::invokeMethod(g_vectorizationTask,
                                                "setDocList",
                                                Qt::QueuedConnection,
                                                Q_ARG(const QList<QString>, uuidList));
        if (!result) {
            KyInfo() << "Failed to invoke setDocList method";
        } else {
            KyInfo() << "Rebuilt vectorization task list with " << uuidList.size() << " items";
            
            // 如果列表不为空，启动进度查询线程并开启定时器
            if (!uuidList.isEmpty()) {
                if (g_progressQueryThread && !g_progressQueryThread->isRunning()) {
                    g_progressQueryThread->start();
                }
                
                if (mUpdateTimer && !mUpdateTimer->isActive()) {
                    KyInfo() << "mUpdateTimer start";
                    mUpdateTimer->start();
                }
            }
        }
    }
    
    return;
}

void FileListModel::updateUploadMsg()
{
    if (!g_vectorizationTask || !g_progressQueryThread) {
        KyInfo() << "Upload worker not initialized";
        return;
    }

    if (g_progressQueryThread && !g_progressQueryThread->isRunning()) {
        g_progressQueryThread->start();
    }

    KyInfo() << "updateUploadMsg ";

    // 使用QMetaObject::invokeMethod异步调用
    bool result = QMetaObject::invokeMethod(g_vectorizationTask,
                                            "updateProgress",
                                            Qt::QueuedConnection);

    if (!result) {
        KyInfo() << "Failed to invoke updateProgress method";
    }

    return;
}

void FileListModel::addUploadTask(const QString &docPath, const QString docUuid)
{
    if (!g_vectorizationTask || !g_progressQueryThread) {
        KyInfo() << "Upload worker not initialized";
        return;
    }

    if (g_progressQueryThread && !g_progressQueryThread->isRunning()) {
        g_progressQueryThread->start();
    }

    KyInfo() << "addUploadTask : docPath " << docPath << " docUuid " << docUuid;

    // 使用QMetaObject::invokeMethod异步调用
    bool result = QMetaObject::invokeMethod(g_vectorizationTask,
                                            "addVectorizationTask",
                                            Qt::QueuedConnection,
                                            Q_ARG(const QString, docPath),
                                            Q_ARG(const QString, docUuid));
    if (!result) {
        KyInfo() << "Failed to invoke addVectorizationTask method";
    }

    // 添加向量化任务后，开启定时器，周期性获取进度。mUpdateTimer在主线程处理的，因此不存在数据竞争。
    if(mUpdateTimer && !mUpdateTimer->isActive()){
        KyInfo() << "mUpdateTimer start";
        mUpdateTimer->start();
    }

    return;
}

void FileListModel::removeFileFromDisk(const QString &copyFilePath)
{
    QFile tempFile(copyFilePath);
    if (!tempFile.exists()) {
        KyInfo() << "The file does not exist and does not need to be deleted. " << copyFilePath;
        return;
    }

    if(!tempFile.remove()) {
        KyInfo() << "Failed to delete " << copyFilePath;
    }

    return;
}

void FileListModel::removeFromWaitingQueue(const QString &uuid, const QString &documentTargetPath)
{
    // 从待上传队列中删除指定文件
    // 待上传队列为主线程维护和访问，不存在数据竞争
    if (g_waitingDocList.isEmpty()) {
        return;
    }

    // 遍历队列，找到匹配的文件并删除
    QQueue<KbDocumentItem> tempQueue;
    bool found = false;
    
    while (!g_waitingDocList.isEmpty()) {
        KbDocumentItem item = g_waitingDocList.dequeue();
        // 通过 uuid 或 documentTargetPath 匹配
        if (item.uuid == uuid || item.documentTargetPath == documentTargetPath) {
            KyInfo() << "Removed file from waiting queue: " << item.documentName 
                     << " uuid: " << item.uuid << " documentTargetPath: " << item.documentTargetPath;
            found = true;
            // 不将该项加入临时队列，即删除该项
        } else {
            // 保留其他项
            tempQueue.enqueue(item);
        }
    }
    
    // 将保留的项重新加入队列
    g_waitingDocList = tempQueue;
    
    if (found) {
        KyInfo() << "Waiting queue size after removal: " << g_waitingDocList.size();
    }
    
    return;
}


QString FileListModel::sendToDeleteDocument(const QJsonObject &data)
{
    if (!data.contains("msgBody")) {
        KyInfo() << "Not carried msgBody!";
        return KB_REQ_PARA_ERROR;
    }

    if (!data["msgBody"].isObject()) {
        KyInfo() << "The type is not an object!";
        return KB_REQ_PARA_ERROR;
    }

    QJsonObject msgBody = data["msgBody"].toObject();
    if(!msgBody.contains("documentTargetPath") || !msgBody["documentTargetPath"].isArray()) {
        KyInfo() << "Not carried uuid!";
        return KB_REQ_PARA_ERROR;
    }
    // 删除单个或者多个文件(文件夹)，都以数组的形式传入
    QJsonArray documentTargetPathArray = msgBody["documentTargetPath"].toArray();
    QStringList deleteUuidList;

    for (const QJsonValue &documentTargetPath : documentTargetPathArray) {
        if (!documentTargetPath.isString()) {
            KyInfo() << "Invalid uuid";
            continue;
        }

        QString documentTargetPathStr = documentTargetPath.toString();
        if (documentTargetPathStr.isEmpty()) {
            KyInfo() << "Uuid: is empty ";
            continue;
        }

        KyInfo() << "documentTargetPathStr is " << documentTargetPathStr;
        // 这里要区分文件夹还是普通文件，需要调用不同的接口去删除

        KbDocumentItem *item = getKbDocumentItemByDocumentTargetPath(documentTargetPathStr);
        if (item == nullptr) {
            KyInfo() << "Failed to delete.";
            continue;
        }

        if (item->isDir) {
            QList<int> list = mDatabase->selectAllChildDirItemIdByFilepath(item->itemId);
            QStringList uuidList = mDatabase->selectAllFileIdsByFilepath(list);
            KyInfo() << "documentTargetPathStr=" << documentTargetPathStr << "uuidList.size=" << uuidList.size();
            deleteUuidList.append(uuidList);
        } else {
            // 如果文件是待上传状态，需要从待上传队列中删除
            if (item->status == DocumentStatus::PENDING) {
                removeFromWaitingQueue(item->uuid, item->documentTargetPath);
            }
            
            // 如果文件是上传中状态，需要从进度查询线程中删除任务，并取消知识库的上传
            if (item->status == DocumentStatus::UPLOADING) {
                cancelUploadingDocument(item->uuid);
            }
            
            deleteUuidList.append(item->uuid);
        }
    }

    // 先调用SDK接口判断是否批量删除成功，在判断是否进行后面删除数据库和备份目录下文件的操作
    bool resultFlag = deleteUploadedDocumentList(deleteUuidList);
    if(!resultFlag){
        KyInfo() << "批量删除失败";
        return KB_REQ_PARA_ERROR;
    }

    for (const QJsonValue &documentTargetPath : documentTargetPathArray) {

        QString documentTargetPathStr = documentTargetPath.toString();
        if (documentTargetPathStr.isEmpty()) {
            KyInfo() << "Uuid: is empty ";
            continue;
        }

        KyInfo() << "documentTargetPathStr is " << documentTargetPathStr;
        // 这里要区分文件夹还是普通文件，需要调用不同的接口去删除

        KbDocumentItem *item = getKbDocumentItemByDocumentTargetPath(documentTargetPathStr);
        if (item == nullptr) {
            KyInfo() << "Failed to delete.";
            continue;
        }

        if (item->isDir) {
            QList<int> list = mDatabase->selectAllChildDirItemIdByFilepath(item->itemId);
            QStringList uuidList = mDatabase->selectAllFileIdsByFilepath(list);
            KyInfo() << "documentTargetPathStr=" << documentTargetPathStr << "uuidList.size=" << uuidList.size();
            // 删除文件夹
            deleteDir(*item);
        } else {
            // 如果文件是待上传状态，需要从待上传队列中删除
            if (item->status == DocumentStatus::PENDING) {
                removeFromWaitingQueue(item->uuid, item->documentTargetPath);
            }

            // 如果文件是上传中状态，需要从进度查询线程中删除任务，并取消知识库的上传
            if (item->status == DocumentStatus::UPLOADING) {
                cancelUploadingDocument(item->uuid);
            }

            // 如果文件已上传成功，需要调用知识库接口删除已向量化的文档
            if (item->status == DocumentStatus::UPLOAD_COMPLETE) {
                //deleteUploadedDocument(item->uuid);
            }

            //删除普通文件
            bool ret = mDatabase->deleteFromKbTable(item->itemId);
            removeFileFromDisk(item->documentTargetPath);
            if (!ret) {
                KyInfo() << "Failed to delete " << item->documentTargetPath;
            }

            Q_EMIT fileTotelCountyChanged();
        }

        // 从当前链表删除
        deleteDocument(item->uuid);

        delete item;
    }

    return KB_RET_SUCCESS;
}

QString FileListModel::sendToCreateDir(const QJsonObject &data)
{
    if (!data.contains("msgBody")) {
        KyInfo() << "Not carried msgBody!";
        return KB_REQ_PARA_ERROR;
    }

    if (!data["msgBody"].isObject()) {
        KyInfo() << "The type is not an object!";
        return KB_REQ_PARA_ERROR;
    }

    QJsonObject msgBody = data["msgBody"].toObject();
    if(!msgBody.contains("directoryPath") || !msgBody["directoryPath"].isString()) {
        KyInfo() << "Not carried directoryPath!";
        return KB_REQ_PARA_ERROR;
    }

    QString directoryPath = msgBody["directoryPath"].toString();
    if(directoryPath.isEmpty()) {
        KyInfo() << "directoryPath is empty!";
        return KB_REQ_PARA_ERROR;
    }

    KyInfo() << "Directory is " << directoryPath;

    QString fullDirPath = mDirectoryPath + "/" + directoryPath;

    addDir(fullDirPath);

    return KB_RET_SUCCESS;
}

void FileListModel::cancelUploadingDocument(const QString &uuid)
{
    KyInfo() << "cancelUploadingDocument: " << uuid;
    
    // 从进度查询线程中删除任务
    if (g_vectorizationTask) {
        bool result = QMetaObject::invokeMethod(g_vectorizationTask,
                                                "deleteVectorizationTask",
                                                Qt::QueuedConnection,
                                                Q_ARG(QString, uuid));
        if (!result) {
            KyInfo() << "Failed to invoke deleteVectorizationTask method";
        } else {
            KyInfo() << "Removed from progress query task: " << uuid;
        }
    }
    
    // 调用知识库接口取消上传
    bool cancelRet = KnowledgeBase::getPtr()->cancelDocumentAddition(uuid);
    if (!cancelRet) {
        KyInfo() << "Failed to cancel upload in knowledge base: " << uuid;
    } else {
        KyInfo() << "Canceled upload in knowledge base: " << uuid;
    }
    
    return;
}

void FileListModel::deleteUploadedDocument(const QString &uuid)
{
    KyInfo() << "deleteUploadedDocument: " << uuid;
    
    // 调用知识库接口删除已向量化的文档
    bool deleteRet = KnowledgeBase::getPtr()->deleteDocument(uuid);
    if (!deleteRet) {
        KyInfo() << "Failed to delete document from knowledge base: " << uuid;
    } else {
        KyInfo() << "Deleted document from knowledge base: " << uuid;
    }
    
    return;
}

bool FileListModel::deleteUploadedDocumentList(const QStringList &uuidList)
{
    KyInfo() << "deleteUploadedDocumentList: " << uuidList.size();

    // 调用知识库接口删除已向量化的文档
    bool deleteRet = KnowledgeBase::getPtr()->deleteDocumentList(uuidList);
    if (!deleteRet) {
        KyInfo() << "Failed to delete document from knowledge base: ";
        return false;
    } else {
        KyInfo() << "Deleted document from knowledge base: ";
        return true;
    }

    return false;
}

void FileListModel::deleteDocument(const QString &uuid)
{
    for (int i = 0; i < mFileList.size(); ++i) {
        KbDocumentItem item = mFileList[i];
        if (item.uuid == uuid) {
            beginRemoveRows(QModelIndex(), i, i);
            mFileList.removeAt(i);
            endRemoveRows();

            // 需要删除磁盘对应的文件
            removeFileFromDisk(item.documentTargetPath);

            // 每当删除文件的时候需要更新QML端count属性和isKbEmpty发生变化

            // 更新知识库空间容量
            KnowledgeBaseSpaceManager::getInstance()->removeSpace(item.documentSize);

            Q_EMIT countChanged();
            Q_EMIT isKbEmptyChanged();

            break;
        }
    }

    return;
}

void FileListModel::addDir(const QString &dirPath)
{
    QFileInfo fileInfo(dirPath);
    KbDocumentItem item;
    // 文件夹也需要添加uuid
    item.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    item.documentName = fileInfo.fileName();
    // 将创建的文件夹的原路径和目标路径都设置为相同的值
    item.documentPath = fileInfo.filePath();
    item.documentTargetPath = fileInfo.filePath();
    // 文件夹的大小设置为0
    item.documentSize = 0;
    item.documentType = "inode-directory";
    item.status = 0;
    // 文件夹本身不会上传，因此百分比设置为100
    item.percent = 100;
    item.errorCode = 0;
    item.createTime = getFileCreateTime(fileInfo);
    // 文件夹的大小设置为0
    item.documentSizeStr = "0";
    item.isDir = true;

    KyInfo() << "fileName: " << fileInfo.fileName();
    KyInfo() << "filePath: " << fileInfo.filePath();

    // 文件夹本身不会上传，因此状态设置为 UPLOAD_COMPLETE
    item.status = UPLOAD_COMPLETE;

    if (fileInfo.exists()) {
        // 如果添加的文件夹已经存在，需要删除。
        KyInfo() << "Directory is already exist: " << dirPath;
        if (fileInfo.isFile()) {
            QFile::remove(dirPath);
        } else {
            QDir dir(dirPath);
            dir.removeRecursively();
        }

    }

    // 如果文件夹不存在则进行创建
    QDir newDir(dirPath);
    // 返回新的model对象之前，需要新建文件夹
    bool ret = newDir.mkpath(".");
    if (!ret) {
        // 待办：这里的错误码需要跟设计确认
        item.statusStr = tr("Failed to create directory:");
        KyInfo() << "Failed to create directory:" << dirPath;
    }

    item.fileIcon = getFileIconPath(dirPath);

    int itemId = -1;
    // 添加的文件夹信息需要保存到数据库，在初始化加载文件夹的时候用于去校验，防止读取到直接从文件管理器界面添加的文件假
    ret = mDatabase->insertKnowledgeBaseInfo(mCurrentFolderId, item, &itemId);
    if (!ret) {
        // todo: 插入数据库失败应该给UI端提示
        KyInfo() << "Failed to insert file info.";
    }

    item.itemId = itemId;

    beginInsertRows(QModelIndex(), 0, 0); // 开始插入行
    mFileList.prepend(item);
    endInsertRows(); // 结束插入行

    Q_EMIT fileTotelCountyChanged();
    Q_EMIT countChanged();

    return;
}

QString FileListModel::getStatusStr(int status)
{
    KyInfo() << "getStatusStr " << status;
    if (status == DocumentStatus::PENDING) {
        return QString(tr("To be uploaded"));
    }

    if ((status == DocumentStatus::UPLOADING) ||
         status == DocumentStatus::COPYING) {
        // uploading不需要翻译，作为QML端的变量判断使用. 拷贝过程也设置为 uploading
        return QString("uploading");
    }

    if (status == DocumentStatus::UPLOAD_ERROR) {
        return QString(tr("Upload failed"));
    }

    return QString("");
}


QString FileListModel::getDocumentTargetPathByName(const QString name)
{
    return mDirectoryPath + "/" + name;
}


QString FileListModel::getDocumentTargetPathByNameAndTargetPath(const QString targetPath, const QString name)
{
    return targetPath + "/" + name;
}

bool FileListModel::isKbStatusEmpty()
{
    bool ret = mDatabase->isKbTableEmpty();

    KyInfo() << "isKbStatusEmpty: " << ret;

    return ret;
}

QVariantList FileListModel::getFolderHierarchy(const QString &fullPath)
{
    QString rootPath = QDir::cleanPath(m_uploadRootPath);
    QString targetPath = QDir::cleanPath(fullPath);

    QVariantList hierarchy;
    //hierarchy.append("全部文件");

    if (targetPath == rootPath) {
        return hierarchy;
    }

    QDir rootDir(rootPath);
    QString relativePath = rootDir.relativeFilePath(targetPath);

    if (relativePath == "." || relativePath.isEmpty()) {
        if (targetPath.startsWith(rootPath)) {
            relativePath = targetPath.mid(rootPath.length());
            if (relativePath.startsWith("/")) {
                relativePath = relativePath.mid(1);
            }
        }
    }

    if (!relativePath.isEmpty() && relativePath != ".") {
        QStringList subDirs = relativePath.split("/", QString::SkipEmptyParts);
        for (const QString &subDir : subDirs) {
            hierarchy.append(subDir);
        }
    }

    if (hierarchy.size() > 4) {
        hierarchy = hierarchy.mid(0, 4);
    }

    return hierarchy;
}

QString FileListModel::getIconPathBySuffix(const QString &suffix) const
{
    QString lowerSuffix = suffix.toLower();
    
    if (lowerSuffix == "txt") {
        return "qrc:/icon/icon/text-plain.svg";
    } else if (lowerSuffix == "doc" || lowerSuffix == "docx" || lowerSuffix == "wps") {
        return "qrc:/icon/icon/application-msword.svg";
    } else if (lowerSuffix == "pdf") {
        return "qrc:/icon/icon/application-x-pdf.svg";
    } else if (lowerSuffix == "ppt" || lowerSuffix == "pptx") {
        return "qrc:/icon/icon/application-mspowerpoint.svg";
    } else if (lowerSuffix == "ofd") {
        return "qrc:/icon/icon/application-ofd.svg";
    } else {
        // 其他文件类型使用默认图标
        return "qrc:/icon/icon/application-x-php.svg";
    }
}

QString FileListModel::getFileIconPath(const QString &filePath) const
{
    return Globals::getFileIconPath(filePath);
}

QString FileListModel::sendToNotifyCopyChanged(const QJsonObject &data)
{
    if (!data.contains("msgBody")) {
        KyInfo() << "Not carried msgBody!";
        return KB_REQ_PARA_ERROR;
    }

    if (!data["msgBody"].isObject()) {
        KyInfo() << "The type is not an object!";
        return KB_REQ_PARA_ERROR;
    }

    QJsonObject msgBody = data["msgBody"].toObject();
    if(!msgBody.contains("src") || !msgBody["src"].isString()) {
        KyInfo() << "Not carried src!";
        return KB_REQ_PARA_ERROR;
    }

    QString src = msgBody["src"].toString();

    if(!msgBody.contains("dest") || !msgBody["dest"].isString()) {
        KyInfo() << "Not carried dest!";
        return KB_REQ_PARA_ERROR;
    }

    QString dest = msgBody["dest"].toString();

    if(!msgBody.contains("progress") || !msgBody["progress"].isDouble()) {
        KyInfo() << "Not carried progress!";
        return KB_REQ_PARA_ERROR;
    }

    int progress = msgBody["progress"].toInt();

    if(!msgBody.contains("status") || !msgBody["status"].isDouble()) {
        KyInfo() << "Not carried progress!";
        return KB_REQ_PARA_ERROR;
    }

    int status = msgBody["status"].toInt();

    if(!msgBody.contains("errorCode") || !msgBody["errorCode"].isDouble()) {
        KyInfo() << "Not carried errorCode!";
        return KB_REQ_PARA_ERROR;
    }

    int errorCode = msgBody["errorCode"].toInt();

    KyInfo() << "Copy changed, src : " << src << " dest : " << dest << " progress : " << progress << " status : " << status << " errorCode : " << errorCode;

    bool foundInList = false;
    for (int i = 0; i < mFileList.size(); ++i) {
        KbDocumentItem &item = mFileList[i];
        if ((item.documentTargetPath == dest) && (item.documentPath == src)) {
            item.status = status;
            // 拷贝进度只占整个进度的10%
            item.percent = progress * 0.1;
            item.errorCode = errorCode;

            if (item.errorCode == 0) {
                item.statusStr = getStatusStr(status);
            } else {
                item.statusStr = Globals::getKnowledgeUploadErrorMsg(errorCode);
            }

            QModelIndex modelIndex = this->index(i);
            KyInfo() << "progress dataChanged  " << item.percent;
            emit dataChanged(modelIndex, modelIndex, {KbDocumentItemRoles::StatusRole, KbDocumentItemRoles::PercentRole, KbDocumentItemRoles::StatusStrRole});

            // 拷贝完成后需要提交到进度查询进程，周期性查询和更新进度。
            if (item.status == DocumentStatus::COPY_COMPLETE) {
                addUploadTask(item.documentTargetPath, item.uuid);
            }

            // 拷贝失败时，删除备份文件
            if (item.status == DocumentStatus::COPY_ERROR) {
                removeFileFromDisk(item.documentTargetPath);
            }

            foundInList = true;
            break;
        }
    }

    // 即使没有在 mFileList 列表里找到项，也需要更新数据库信息，防止信息丢失
    // 因为 mFileList 列表只是映射当前文件夹的项目信息，而拷贝线程负责处理所有的拷贝任务
    if (!foundInList) {
        KyInfo() << "Item not found in mFileList, updating database directly";
        // 从数据库获取完整的 item 信息，包括 uuid
        KbDocumentItem *itemPtr = getKbDocumentItemByDocumentTargetPath(dest);
        if (itemPtr != nullptr) {
            int itemId = itemPtr->itemId;
            QJsonObject updateInfo;
            updateInfo["status"] = status;
            updateInfo["percent"] = static_cast<int>(progress * 0.1);
            updateInfo["errorCode"] = errorCode;
            bool ret = mDatabase->updateKbTableById(QJsonDocument(updateInfo).toJson(), itemId);
            if (!ret) {
                KyInfo() << "Failed to update database for itemId: " << itemId;
            } else {
                KyInfo() << "Updated database for itemId: " << itemId;
            }
            
            // 拷贝完成后需要提交到进度查询进程，周期性查询和更新进度
            // 即使当前列表没有找到对应的项，也需要调用知识库的接口进行向量化
            if (status == DocumentStatus::COPY_COMPLETE && errorCode == 0) {
                KyInfo() << "Copy completed for item not in current list, adding to vectorization task: " << itemPtr->documentTargetPath << " uuid: " << itemPtr->uuid;
                addUploadTask(itemPtr->documentTargetPath, itemPtr->uuid);
            }
            
            // 拷贝失败时，删除备份文件
            if (status == DocumentStatus::COPY_ERROR) {
                removeFileFromDisk(dest);
            }

            delete itemPtr;
        } else {
            KyInfo() << "Cannot find item for documentTargetPath: " << dest;
        }
    } else {
        // 如果找到了，也要更新数据库
        int itemId = mDatabase->getItemIdByDocumentargetPath(dest);
        if (itemId > 0) {
            QJsonObject updateInfo;
            updateInfo["status"] = status;
            updateInfo["percent"] = static_cast<int>(progress * 0.1);
            updateInfo["errorCode"] = errorCode;
            mDatabase->updateKbTableById(QJsonDocument(updateInfo).toJson(), itemId);

            // 拷贝失败时，删除备份文件
            if (status == DocumentStatus::COPY_ERROR) {
                removeFileFromDisk(dest);
            }
        }
    }

    return KB_RET_SUCCESS;
}

QString FileListModel::sendToNotifyUploadChanged(const QJsonObject &data)
{
    if (!data.contains("msgBody")) {
        KyInfo() << "Not carried msgBody!";
        return KB_REQ_PARA_ERROR;
    }

    if (!data["msgBody"].isObject()) {
        KyInfo() << "The type is not an object!";
        return KB_REQ_PARA_ERROR;
    }

    QJsonObject msgBody = data["msgBody"].toObject();
    if(!msgBody.contains("uuid") || !msgBody["uuid"].isString()) {
        KyInfo() << "Not carried uuid!";
        return KB_REQ_PARA_ERROR;
    }

    QString uuid = msgBody["uuid"].toString();

    if(!msgBody.contains("progress") || !msgBody["progress"].isDouble()) {
        KyInfo() << "Not carried progress!";
        return KB_REQ_PARA_ERROR;
    }

    int progress = msgBody["progress"].toInt();

    if(!msgBody.contains("status") || !msgBody["status"].isDouble()) {
        KyInfo() << "Not carried progress!";
        return KB_REQ_PARA_ERROR;
    }

    int status = msgBody["status"].toInt();

    if(!msgBody.contains("errorCode") || !msgBody["errorCode"].isDouble()) {
        KyInfo() << "Not carried errorCode!";
        return KB_REQ_PARA_ERROR;
    }

    int errorCode = msgBody["errorCode"].toInt();

    KyInfo() << "Upload changed, uuid : " << uuid << " progress : " << progress << " status : " << status << " errorCode : " << errorCode;

    bool foundInList = false;
    for (int i = 0; i < mFileList.size(); ++i) {
        KbDocumentItem &item = mFileList[i];
        if ((item.uuid == uuid)) {
            item.status = status;
            // 知识库向量化的过程中，进度占后90%的进度
            item.percent = 10 + (progress * 0.9);
            item.errorCode = errorCode;

            if (item.errorCode == 0) {
                item.statusStr = getStatusStr(status);
            } else {
                item.statusStr = Globals::getKnowledgeUploadErrorMsg(errorCode);
            }

            QModelIndex modelIndex = this->index(i);
            emit dataChanged(modelIndex, modelIndex, {KbDocumentItemRoles::StatusRole, KbDocumentItemRoles::PercentRole, KbDocumentItemRoles::StatusStrRole});

            // 上传失败时，删除备份文件
            if (item.status == DocumentStatus::UPLOAD_ERROR) {
                removeFileFromDisk(item.documentTargetPath);
            }

            foundInList = true;
            break;
        }
    }

    // 即使没有在 mFileList 列表里找到项，也需要更新数据库信息，防止信息丢失
    // 因为 mFileList 列表只是映射当前文件夹的项目信息，而进度查询线程负责处理所有的拷贝任务
    int itemId = mDatabase->getItemIdByUuid(uuid);
    if (itemId > 0) {
        QJsonObject updateInfo;
        updateInfo["status"] = status;
        updateInfo["percent"] = static_cast<int>(10 + (progress * 0.9));
        updateInfo["errorCode"] = errorCode;
        bool ret = mDatabase->updateKbTableById(QJsonDocument(updateInfo).toJson(), itemId);
        if (!ret) {
            KyInfo() << "Failed to update database for itemId: " << itemId;
        } else {
            if (!foundInList) {
                KyInfo() << "Item not found in mFileList, updated database directly for itemId: " << itemId;
            } else {
                KyInfo() << "Updated database for itemId: " << itemId;
            }

            // 上传失败时，删除备份文件
            if (status == DocumentStatus::UPLOAD_ERROR) {
                KbDocumentItem item = mDatabase->getItemByUuid(uuid);
                if (!item.documentTargetPath.isEmpty()) {
                    removeFileFromDisk(item.documentTargetPath);
                }
            }
        }
    } else {
        KyInfo() << "Cannot find itemId for uuid: " << uuid;
    }

    return KB_RET_SUCCESS;
}

QString FileListModel::sendToNotifyUploadFinished(const QJsonObject &data)
{
    if (!data.contains("msgBody")) {
        KyInfo() << "Not carried msgBody!";
        return KB_REQ_PARA_ERROR;
    }

    // 上传进度刷新定时器，只在主线程操作因此不存在数据竞争。如果上传完成了，则停止进度刷新定时器。
    if((mUpdateTimer != nullptr) && mUpdateTimer->isActive()){
        mUpdateTimer->stop();
    }

    return KB_RET_SUCCESS;
}

QString FileListModel::sendToCopyNext(const QJsonObject &data)
{
    if (!data.contains("msgBody")) {
        KyInfo() << "Not carried msgBody!";
        return KB_REQ_PARA_ERROR;
    }

    // 待拷贝队列为主线程维护和访问，不存在数据竞争
    if(g_waitingDocList.isEmpty()) {
        return KB_RET_SUCCESS;
    }

    KbDocumentItem nextItem = g_waitingDocList.dequeue();

    copyAsync(nextItem);

    // 可以拷贝下一个了
    return KB_RET_SUCCESS;
}

QString FileListModel::sendToInitUploadThread(const QJsonObject &data)
{
    if (!data.contains("msgBody")) {
        KyInfo() << "Not carried msgBody!";
        return KB_REQ_PARA_ERROR;
    }

    initUploadThread();

    return KB_RET_SUCCESS;
}

QString FileListModel::sendToGetCurrentFiles(const QJsonObject &data)
{
    if (!data.contains("msgBody")) {
        KyInfo() << "Not carried msgBody!";
        return KB_REQ_PARA_ERROR;
    }

    QJsonObject recvMsg;
    recvMsg["msgType"] = "recvFileList";

    QJsonArray jsonArray;
    for (int i = 0; i < mFileList.size(); ++i) {
        KbDocumentItem &item = mFileList[i];
        jsonArray.append(item.documentPath);
    }

    QJsonObject recvMsgbody;
    recvMsgbody["fileList"] = jsonArray;

    recvMsg["msgBody"] = recvMsgbody;

    return QJsonDocument(recvMsg).toJson();
}

QString FileListModel::sendToRename(const QJsonObject &data)
{
    if (!data.contains("msgBody")) {
        KyInfo() << "Not carried msgBody!";
        return KB_REQ_PARA_ERROR;
    }

    QJsonObject msgBody = data["msgBody"].toObject();
    if(!msgBody.contains("src") || !msgBody["src"].isString()) {
        KyInfo() << "Not carried src!";
        return KB_REQ_PARA_ERROR;
    }

    QString src = msgBody["src"].toString();

    if(!msgBody.contains("dest") || !msgBody["dest"].isString()) {
        KyInfo() << "Not carried dest!";
        return KB_REQ_PARA_ERROR;
    }

    QString dest = msgBody["dest"].toString();

    KyInfo() << "Rename file " << src << " to " << dest;

    int rowIndex = getIndexByTargetPath(src);
    if (rowIndex == -1) {
        KyInfo() << "Cannot find the current file :" << src;
        return KB_REQ_PARA_ERROR;
    }

    if (!mFileList[rowIndex].isDir) {
        renameSingleFile(rowIndex, dest);
    } else {
        renameSingleDir(rowIndex, dest);
    }

    return KB_REQ_PARA_ERROR;
}

int FileListModel::getItemIdByPath(const QString &path)
{
    if (Globals::uploadDirPath == path) {
        return 0;
    }

    return mDatabase->getItemIdByDocumentargetPath(path);
}

KbDocumentItem *FileListModel::getKbDocumentItemByDocumentTargetPath(const QString &documentTargetPath)
{
    KbDocumentItem *ret = new KbDocumentItem();
    if (ret == nullptr) {
        KyInfo() << "Cannot acquire memory.";
        return nullptr;
    }

    *ret = mDatabase->getItemByDocumentTargetPath(documentTargetPath);

    return ret;
}

int FileListModel::getIndexByTargetPath(const QString &documentTargetPath)
{
    for (int i = 0; i < mFileList.size(); ++i) {
        KbDocumentItem item = mFileList[i];
        if (item.documentTargetPath == documentTargetPath) {
            KyInfo() << "Found : itemId " << item.itemId << " , isDir " << item.isDir;
            return i;
        }
    }

    // 如果没有找到返回 -1
    return -1;
}

void FileListModel::copyAsync(const KbDocumentItem &item)
{
    // 此接口在主线程被调用到的，操作待拷贝队列，不存在数据竞争，因此不需要加锁。
    if (!g_uploadTask || !g_uploadThread) {
        KyInfo() << "Worker not initialized";
        return;
    }

    QString source = item.documentPath;
    QString destination = item.documentTargetPath;

    // 从待拷贝队列拿出首个待拷贝的。

    KyInfo() << "src : " << source << " dest " << destination;

    // 使用QMetaObject::invokeMethod异步调用
    bool result = QMetaObject::invokeMethod(g_uploadTask,
                                            "copyFile",
                                            Qt::QueuedConnection,
                                            Q_ARG(const QString, source),
                                            Q_ARG(const QString, destination));

    if (!result) {
        KyInfo() << "Failed to invoke copyFile method";
    }

    return;
}

bool FileListModel::renameSingleFile(int rowIndex, QString newName)
{
    KbDocumentItem item = mFileList[rowIndex];
    QString newTargetName = getDocumentTargetPathByName(newName);

    // 重命名前检查源文件是否存在
    QFile sourceFile(item.documentTargetPath);
    if (!sourceFile.exists()) {
        KyInfo() << "Source file not exists when rename, path: " << item.documentTargetPath;

        // 设置错误状态和错误码
        mFileList[rowIndex].status = DocumentStatus::UPLOAD_ERROR;
        mFileList[rowIndex].errorCode = KB_ERROR_UPLOAD_DOC_NOT_EXISTS;
        if (mFileList[rowIndex].errorCode == 0) {
            mFileList[rowIndex].statusStr = getStatusStr(mFileList[rowIndex].status);
        } else {
            mFileList[rowIndex].statusStr = Globals::getKnowledgeUploadErrorMsg(mFileList[rowIndex].errorCode);
        }

        // 更新数据库中的状态和错误码
        QJsonObject updateInfo;
        updateInfo["status"] = mFileList[rowIndex].status;
        updateInfo["errorCode"] = mFileList[rowIndex].errorCode;
        bool ret = mDatabase->updateKbTableById(QJsonDocument(updateInfo).toJson(), mFileList[rowIndex].itemId);
        if (!ret) {
            KyInfo() << "Failed to update kb table when source file not exists, itemId: " << mFileList[rowIndex].itemId;
        }

        // 通知前端更新显示
        QModelIndex modelIndex = this->index(rowIndex);
        emit dataChanged(modelIndex, modelIndex, {KbDocumentItemRoles::StatusRole,
                                                  KbDocumentItemRoles::ErrorCodeRole,
                                                  KbDocumentItemRoles::StatusStrRole});

        return false;
    }

    if (!QFile::rename(item.documentTargetPath, newTargetName)) {
        KyInfo() << "Cannot rename " << item.documentTargetPath << " to " << newTargetName;
        return false;
    }

    bool ret = mDatabase->deleteFromKbTable(mFileList[rowIndex].itemId);
    if (!ret) {
        KyInfo() << "Failed to delete " << mFileList[rowIndex].documentTargetPath;
        return false;
    }

    item.documentTargetPath = newTargetName;
    item.documentName = newName;

    beginRemoveRows(QModelIndex(), rowIndex, rowIndex);
    mFileList.removeAt(rowIndex);
    endRemoveRows();

    int itemId = 0;
    // 保存到数据库原位置
    bool insertRet = mDatabase->insertPreviousKnowledgeBaseInfo(mCurrentFolderId, item, &itemId);
    if (!insertRet) {
        KyInfo() << "Failed to insert " << item.documentTargetPath;
        return false;
    }

    item.itemId = itemId;

    int dirCnt = getDirCount();
    // 普通文件插入在原位置
    beginInsertRows(QModelIndex(), rowIndex, rowIndex);
    mFileList.insert(rowIndex, item);
    endInsertRows();

    Q_EMIT fileTotelCountyChanged();
    Q_EMIT countChanged();

    return true;
}

bool FileListModel::renameSingleDir(int rowIndex, QString newName)
{
    KbDocumentItem item = mFileList[rowIndex];

    int oldItemId = item.itemId;
    QString oldTargetName = item.documentTargetPath;
    QString newTargetName = getDocumentTargetPathByName(newName);

    QDir dir(item.documentTargetPath);
    if (!dir.rename(item.documentTargetPath, newTargetName)) {
        KyInfo() << "Cannot rename dir to " << newTargetName;
        return false;
    }

    QList<KbDocumentItem> children = mDatabase->getChildrenByParentId(oldItemId);

    bool ret = mDatabase->deleteFromKbTable(oldItemId);
    if (!ret) {
        KyInfo() << "Failed to delete " << mFileList[rowIndex].documentTargetPath;
        return false;
    }

    beginRemoveRows(QModelIndex(), rowIndex, rowIndex);
    mFileList.removeAt(rowIndex);
    endRemoveRows();

    item.documentName = newName;
    item.documentTargetPath = newTargetName;

    // 保存到数据库
    int newItemId = 0;
    // 插入到数据库的原位置
    bool insertRet = mDatabase->insertPreviousKnowledgeBaseInfo(mCurrentFolderId, item, &newItemId);
    if (!insertRet) {
        KyInfo() << "Failed to insert " << item.documentTargetPath;
        return false;
    }

    item.itemId = newItemId;
    // 文件夹插入在列表原位置
    beginInsertRows(QModelIndex(), rowIndex, rowIndex);  // 这是你插入的行位置
    mFileList.insert(rowIndex, item);  // 在列表中插入数据
    endInsertRows();

    Q_EMIT fileTotelCountyChanged();
    Q_EMIT countChanged();

    // 更新子节点数据库记录的parentId
    for (int i = 0; i < children.size(); ++i) {
        QJsonObject updateInfo;
        updateInfo["parentId"] = newItemId;
        bool ret = mDatabase->updateKbTableById(QJsonDocument(updateInfo).toJson(), children[i].itemId);
        if (!ret) {
            KyInfo() << "updateInfo " << updateInfo << " itemId " << children[i].itemId;
        }
    }

    // 更新所有后代节点的 documentTargetPath
    QList<KbDocumentItem> allChildren = mDatabase->getChildrenByParentIdRecursively(newItemId);
    for (int i = 0; i < allChildren.size(); ++i) {
        /* 这里不能是简单的查找和替换，因为路径的层级可能存在重名的。会匹配到不止一处。
         * 对于所有的节点来说，只是顶层的文件夹路径修改了，因此匹配顶层文件夹路径，则是唯一匹配，进行替换即可。
         */

        KyInfo() << "oldTargetName " << oldTargetName << " newTargetName " << newTargetName;
        QString updateTargetPath = allChildren[i].documentTargetPath.replace(oldTargetName, newTargetName);
        QJsonObject updateInfo;
        updateInfo["documentTargetPath"] = updateTargetPath;
        bool ret = mDatabase->updateKbTableById(QJsonDocument(updateInfo).toJson(), allChildren[i].itemId);
        if (!ret) {
            KyInfo() << "updateInfo " << updateInfo << " itemId " << children[i].itemId;
        }
    }

    return true;
}

int FileListModel::getDirCount()
{
    int cnt = 0;
    for (int i = 0; i < mFileList.size(); ++i) {
        KbDocumentItem item = mFileList[i];
        if (item.isDir) {
            cnt++;
        } else {
            break;
        }
    }

    return cnt;
}

KbDocumentItem *FileListModel::getKbDocumentItemByPath(const QString &documentTargetPath)
{
    KbDocumentItem item = mDatabase->getItemByDocumentTargetPath(documentTargetPath);
    if (item.itemId <= 0 || item.uuid.isEmpty()) {
        KyInfo() << "Cannot find item for path: " << documentTargetPath;
        return nullptr;
    }

    // 根据路径判断是文件还是文件夹
    QFileInfo fileInfo(documentTargetPath);
    item.isDir = fileInfo.isDir();
    item.documentName = fileInfo.fileName();

    KbDocumentItem *ret = new KbDocumentItem();
    if (ret == nullptr) {
        KyInfo() << "Cannot acquire memory.";
        return nullptr;
    }
    *ret = item;
    return ret;
}

int FileListModel::selectMaxHierarchyByPath(const QString &sourcePath)
{
    int maxHierarchyByPath = mDatabase->selectMaxHierarchyByPath(sourcePath);
    return maxHierarchyByPath;
}

bool FileListModel::checkDirIsCanMove(const QString &sourcePath)
{
    int itemId = mDatabase->getItemIdByDocumentPath(sourcePath);
    KyInfo() << "checkDirIsCanMove===" << sourcePath << "mDirectoryPath==" << mDirectoryPath << "  itemId  " << itemId;
    if(itemId > 0){
        QStringList result = mDatabase->selectDocumentPathListByItemId(itemId);
        if(result.contains(mDirectoryPath) || getCommonRootDirectory(result) == mDirectoryPath){
            return false;
        }
    }
    return true;
}

QString FileListModel::getCommonRootDirectory(const QStringList &pathList)
{
    // ========== 步骤1：边界条件处理 ==========
    if (pathList.isEmpty()) {
        KyInfo() << "getCommonRootByRule: pathList is empty";
        return "";
    }

    // ========== 步骤2：提取所有路径的父目录 ==========
    QStringList parentDirList; // 存储所有路径的父目录
    for (const QString& path : pathList) {
        // 标准化路径：去除末尾的/，避免格式不一致
        QString normPath = path.trimmed();
        if (normPath.endsWith('/') && normPath != "/") {
            normPath.chop(1);
        }

        // 提取父目录（找到最后一个/的位置）
        int lastSlashIdx = normPath.lastIndexOf('/');
        if (lastSlashIdx <= 0) { // 根路径（如/）无父目录，跳过
            KyInfo() << "getCommonRootByRule: path [" << path << "] has no parent directory";
            continue;
        }
        QString parentDir = normPath.left(lastSlashIdx);
        parentDirList.append(parentDir);
    }

    // 无有效父目录，返回空
    if (parentDirList.isEmpty()) {
        KyInfo() << "getCommonRootByRule: no valid parent directory found";
        return "";
    }

    // ========== 步骤3：按规则返回结果 ==========
    // 场景1：只有1条有效父目录 → 直接返回
    if (parentDirList.size() == 1) {
        return parentDirList.first();
    }

    // 场景2：≥2条父目录 → 提取最长公共路径
    // 子步骤3.1：拆分所有父目录为"目录段"（如/a/b/c → ["a","b","c"]）
    QList<QStringList> splitParentDirs; // 存储拆分后的目录段
    int minSegmentCount = INT_MAX;     // 最短目录段数（对比的最大长度）
    for (const QString& parentDir : parentDirList) {
        QStringList segments = parentDir.split('/', QString::SkipEmptyParts); // 跳过空段，避免//的情况
        splitParentDirs.append(segments);
        // 更新最短目录段数（只对比到最短的长度）
        if (segments.size() < minSegmentCount) {
            minSegmentCount = segments.size();
        }
    }

    // 子步骤3.2：逐段对比，找到最长公共目录段
    QStringList commonSegments;
    for (int i = 0; i < minSegmentCount; ++i) {
        // 以第一个父目录的第i段为基准
        QString baseSegment = splitParentDirs[0][i];
        bool allMatch = true;
        // 对比所有父目录的第i段
        for (int j = 1; j < splitParentDirs.size(); ++j) {
            if (splitParentDirs[j][i] != baseSegment) {
                allMatch = false;
                break;
            }
        }
        // 一致则加入公共段，不一致则终止对比
        if (allMatch) {
            commonSegments.append(baseSegment);
        } else {
            break;
        }
    }

    // 子步骤3.3：拼接公共目录段为完整路径
    if (commonSegments.isEmpty()) {
        KyInfo() << "getCommonRootByRule: no common path found in parent directories";
        return "";
    }
    KyInfo() << "qwerwqrqerwqerwqrqw" <<"/" + commonSegments.join("/");
    return "/" + commonSegments.join("/");
}

QStringList FileListModel::getAllFileNameByDocumentPath(const QString &sourcePath)
{
    KyInfo() << "getAllFileNameByDocumentPath==" << sourcePath << "---" << (sourcePath == m_uploadRootPath);
    QStringList documentPathList;

    if(sourcePath == m_uploadRootPath){
        documentPathList = mDatabase->selectDocumentTargetPathListByParentId(0);
    }else {
        int itemId = mDatabase->getItemIdByDocumentPath(sourcePath);
        documentPathList = mDatabase->selectDocumentTargetPathListByParentId(itemId);
    }

    return documentPathList;
}

int FileListModel::getAllFileCountByDIr(int itemId)
{
    QList<int> list = mDatabase->selectAllChildDirItemIdByFilepath(itemId);
    return mDatabase->selectFileCountByFilePath(list, false);
}

QString FileListModel::sendToMove(const QJsonObject &data)
{
    if (!data.contains("msgBody")) {
        KyInfo() << "Not carried msgBody!";
        return KB_REQ_PARA_ERROR;
    }

    if (!data["msgBody"].isObject()) {
        KyInfo() << "The type is not an object!";
        return KB_REQ_PARA_ERROR;
    }

    QJsonObject msgBody = data["msgBody"].toObject();
    if(!msgBody.contains("sourcePath") || !msgBody["sourcePath"].isArray()) {
        KyInfo() << "Not carried sourcePath or sourcePath is not an array!";
        return KB_REQ_PARA_ERROR;
    }

    if(!msgBody.contains("targetFilePath") || !msgBody["targetFilePath"].isString()) {
        KyInfo() << "Not carried targetFilePath or targetFilePath is not string!";
        return KB_REQ_PARA_ERROR;
    }

    // 移动单个或者多个文件(文件夹)，都以数组的形式传入
    QJsonArray sourcePathArray = msgBody["sourcePath"].toArray();
    QString targetFilePath = msgBody["targetFilePath"].toString();

    for (const QJsonValue &pathValue : sourcePathArray) {
        if (!pathValue.isString()) {
            KyInfo() << "Invalid source path";
            continue;
        }

        QString sourcePath = pathValue.toString();
        if (sourcePath.isEmpty()) {
            KyInfo() << "sourcePath is empty";
            continue;
        }

        KyInfo() << "Move file or directory from " << sourcePath << " to " << targetFilePath;

        // 从数据库获取源文件/文件夹信息
        KbDocumentItem *item = getKbDocumentItemByPath(sourcePath);
        if (item == nullptr) {
            KyInfo() << "Cannot find the item: " << sourcePath;
            continue;
        }

        bool ret = false;
        if (item->isDir) {
            ret = moveSingleDir(targetFilePath, sourcePath);
        } else {
            ret = moveSingleFile(targetFilePath, sourcePath);
        }

        delete item;

        if (!ret) {
            KyInfo() << "Failed to move: " << sourcePath;
            // 继续处理下一个，不中断整个流程
        }
    }

    return KB_RET_SUCCESS;
}

bool FileListModel::moveSingleFile(const QString targetPath, const QString &sourcePath)
{
    KyInfo() << "moveSingleFile: " << sourcePath;

    // 从数据库获取源文件信息
    KbDocumentItem *itemPtr = getKbDocumentItemByPath(sourcePath);
    if (itemPtr == nullptr) {
        KyInfo() << "Cannot find the file: " << sourcePath;
        return false;
    }

    KbDocumentItem item = *itemPtr;
    delete itemPtr;

    QFileInfo sourceInfo(sourcePath);
    QString fileName = sourceInfo.fileName();
    QString newTargetPath = getDocumentTargetPathByNameAndTargetPath(targetPath, fileName);
    int newTargetItemId = 0;
    if(targetPath == m_uploadRootPath){
        newTargetItemId = 0;
    }else{
        newTargetItemId = mDatabase->getItemIdByDocumentargetPath(targetPath);
    }

    // 检查目标路径是否已存在
    if (QFile::exists(newTargetPath)) {
        KyInfo() << "Target file already exists: " << newTargetPath;
        return false;
    }

    // 重命名前检查源文件是否存在
    if (!sourceInfo.exists()) {
        KyInfo() << "Source file not exists when move, path: " << sourcePath;

        // 设置错误状态和错误码
        item.status = DocumentStatus::UPLOAD_ERROR;
        item.errorCode = KB_ERROR_UPLOAD_DOC_NOT_EXISTS;
        if (item.errorCode == 0) {
            item.statusStr = getStatusStr(item.status);
        } else {
            item.statusStr = Globals::getKnowledgeUploadErrorMsg(item.errorCode);
        }
    }

    // 重命名（移动）磁盘文件
    if (!QFile::rename(sourcePath, newTargetPath)) {
        KyInfo() << "Cannot move file from " << sourcePath << " to " << newTargetPath;
        // 如果这里发生错误，不需要立即返回，报错体现在新的一条数据里。
    }

    // 删除原来的数据库记录
    bool ret = mDatabase->deleteFromKbTable(item.itemId);
    if (!ret) {
        KyInfo() << "Failed to delete original record, itemId: " << item.itemId;
        // 如果删除失败，尝试恢复文件
        QFile::rename(newTargetPath, sourcePath);
        return false;
    }

    // 准备新的记录（继承原有字段，更新路径和创建时间）
    item.documentTargetPath = newTargetPath;
    item.documentName = fileName; // 文件名保持不变
    // 移动文件创建时间为原来的时间
    //item.createTime = getFileCreateTime(QFileInfo(newTargetPath)); // 创建时间为当前时间
    // 其他字段（documentSize, errorCode, status, percent等）都继承原有记录

    // 添加新的数据库记录
    int newItemId = 0;
    bool insertRet = mDatabase->insertKnowledgeBaseInfo(newTargetItemId, item, &newItemId);
    if (!insertRet) {
        KyInfo() << "Failed to insert new record: " << newTargetPath;
        // 如果插入失败，尝试恢复文件
        QFile::rename(newTargetPath, sourcePath);
        return false;
    }

    // 更新列表：如果文件在当前列表中，先移除；然后将新文件添加到列表
    int rowIndex = getIndexByTargetPath(sourcePath);
    if (rowIndex != -1) {
        beginRemoveRows(QModelIndex(), rowIndex, rowIndex);
        mFileList.removeAt(rowIndex);
        endRemoveRows();
    }

    // 新路径在当前文件夹中，添加到列表
    item.itemId = newItemId;
    QFileInfo info(newTargetPath);
    item.fileIcon = getFileIconPath(newTargetPath);
    item.documentSizeStr = getFileSize(info);

    int dirCnt = getDirCount();
    beginInsertRows(QModelIndex(), dirCnt, dirCnt);
    mFileList.insert(dirCnt, item);
    endInsertRows();

    Q_EMIT fileTotelCountyChanged();
    Q_EMIT countChanged();

    return true;
}

bool FileListModel::moveSingleDir(const QString targetPath, const QString &sourcePath)
{
    KyInfo() << "moveSingleDir: " << sourcePath;

    // 从数据库获取源文件夹信息
    KbDocumentItem *itemPtr = getKbDocumentItemByPath(sourcePath);
    if (itemPtr == nullptr) {
        KyInfo() << "Cannot find the directory: " << sourcePath;
        return false;
    }

    KbDocumentItem item = *itemPtr;
    int oldItemId = item.itemId;
    QString oldTargetPath = item.documentTargetPath;
    delete itemPtr;

    QFileInfo sourceInfo(sourcePath);
    QString dirName = sourceInfo.fileName();

    QString newTargetPath = getDocumentTargetPathByNameAndTargetPath(targetPath, dirName);
    int newTargetItemId = 0;
    if(targetPath == m_uploadRootPath){
        newTargetItemId = 0;
    }else{
        newTargetItemId = mDatabase->getItemIdByDocumentargetPath(targetPath);
    }


    // 检查目标路径是否已存在
    if (QDir(newTargetPath).exists()) {
        KyInfo() << "Target directory already exists: " << newTargetPath;
        return false;
    }

    // 获取所有子节点（在删除前获取）
    QList<KbDocumentItem> children = mDatabase->getChildrenByParentId(oldItemId);

    // 重命名（移动）磁盘文件夹
    QDir dir(sourcePath);
    if (!dir.rename(sourcePath, newTargetPath)) {
        KyInfo() << "Cannot move directory from " << sourcePath << " to " << newTargetPath;
        return false;
    }

    // 删除原来的数据库记录
    bool ret = mDatabase->deleteFromKbTable(oldItemId);
    if (!ret) {
        KyInfo() << "Failed to delete original record, itemId: " << oldItemId;
        // 如果删除失败，尝试恢复文件夹
        QDir(newTargetPath).rename(newTargetPath, sourcePath);
        return false;
    }

    // 准备新的记录（继承原有字段，更新路径和创建时间）
    item.documentTargetPath = newTargetPath;
    item.documentName = dirName; // 文件名保持不变
    // 移动文件夹创建时间为原文件夹的原始时间
    //item.createTime = getFileCreateTime(QFileInfo(newTargetPath));
    // 其他字段（documentSize, errorCode, status, percent等）都继承原有记录

    // 添加新的数据库记录
    int newItemId = 0;
    bool insertRet = mDatabase->insertKnowledgeBaseInfo(newTargetItemId, item, &newItemId);
    if (!insertRet) {
        KyInfo() << "Failed to insert new record: " << newTargetPath;
        // 如果插入失败，尝试恢复文件夹
        QDir(newTargetPath).rename(newTargetPath, sourcePath);
        return false;
    }

    // 更新所有子节点的 parentId 为新 itemId
    for (int i = 0; i < children.size(); ++i) {
        QJsonObject updateInfo;
        updateInfo["parentId"] = newItemId;
        bool updateRet = mDatabase->updateKbTableById(QJsonDocument(updateInfo).toJson(), children[i].itemId);
        if (!updateRet) {
            KyInfo() << "Failed to update parentId for itemId: " << children[i].itemId;
        }
    }

    // 更新所有后代节点的 documentTargetPath
    QList<KbDocumentItem> allChildren = mDatabase->getChildrenByParentIdRecursively(newItemId);
    for (int i = 0; i < allChildren.size(); ++i) {
        QString updateTargetPath = allChildren[i].documentTargetPath;
        updateTargetPath.replace(oldTargetPath, newTargetPath);
        QJsonObject updateInfo;
        updateInfo["documentTargetPath"] = updateTargetPath;
        bool updateRet = mDatabase->updateKbTableById(QJsonDocument(updateInfo).toJson(), allChildren[i].itemId);
        if (!updateRet) {
            KyInfo() << "Failed to update documentTargetPath for itemId: " << allChildren[i].itemId;
        }
    }

    // 更新列表：如果文件夹在当前列表中，先移除；然后将新文件夹添加到列表
    int rowIndex = getIndexByTargetPath(sourcePath);
    if (rowIndex != -1) {
        beginRemoveRows(QModelIndex(), rowIndex, rowIndex);
        mFileList.removeAt(rowIndex);
        endRemoveRows();
    }

    // 新路径在当前文件夹中，添加到列表
    item.itemId = newItemId;
    QFileInfo info(newTargetPath);
    item.fileIcon = getFileIconPath(newTargetPath);
    item.documentSizeStr = getFileSize(info);

    beginInsertRows(QModelIndex(), 0, 0);
    mFileList.prepend(item);
    endInsertRows();

    Q_EMIT fileTotelCountyChanged();
    Q_EMIT countChanged();

    return true;
}

QString FileListModel::sendToCopy(const QJsonObject &data)
{
    if (!data.contains("msgBody")) {
        KyInfo() << "Not carried msgBody!";
        return KB_REQ_PARA_ERROR;
    }

    if (!data["msgBody"].isObject()) {
        KyInfo() << "The type is not an object!";
        return KB_REQ_PARA_ERROR;
    }

    QJsonObject msgBody = data["msgBody"].toObject();
    if(!msgBody.contains("sourcePath") || !msgBody["sourcePath"].isArray()) {
        KyInfo() << "Not carried sourcePath or sourcePath is not an array!";
        return KB_REQ_PARA_ERROR;
    }

    if(!msgBody.contains("targetFilePath") || !msgBody["targetFilePath"].isString()) {
        KyInfo() << "Not carried targetFilePath or targetFilePath is not string!";
        return KB_REQ_PARA_ERROR;
    }

    // 复制单个或者多个文件(文件夹)，都以数组的形式传入
    QJsonArray sourcePathArray = msgBody["sourcePath"].toArray();
    QString targetFilePath = msgBody["targetFilePath"].toString();
    for (const QJsonValue &pathValue : sourcePathArray) {
        if (!pathValue.isString()) {
            KyInfo() << "Invalid source path";
            continue;
        }

        QString sourcePath = pathValue.toString();
        if (sourcePath.isEmpty()) {
            KyInfo() << "sourcePath is empty";
            continue;
        }

        KyInfo() << "Copy file or directory from " << sourcePath << " to " << mDirectoryPath;

        // 从数据库获取源文件/文件夹信息
        KbDocumentItem *item = getKbDocumentItemByPath(sourcePath);
        if (item == nullptr) {
            KyInfo() << "Cannot find the item: " << sourcePath;
            continue;
        }

        bool ret = false;
        if (item->isDir) {
            ret = copySingleDir(targetFilePath, sourcePath);
        } else {
            ret = copySingleFile(targetFilePath, sourcePath);
        }

        delete item;

        if (!ret) {
            KyInfo() << "Failed to copy: " << sourcePath;
            // 继续处理下一个，不中断整个流程
        }
    }

    return KB_RET_SUCCESS;
}

QString FileListModel::sendToGetDirHierarchy(const QJsonObject &data)
{
    if (!data.contains("msgBody")) {
        KyInfo() << "Not carried msgBody!";
        return KB_REQ_PARA_ERROR;
    }

    if (!data["msgBody"].isObject()) {
        KyInfo() << "The type is not an object!";
        return KB_REQ_PARA_ERROR;
    }

    QJsonObject msgBody = data["msgBody"].toObject();
    if(!msgBody.contains("sourcePath") || !msgBody["sourcePath"].isArray()) {
        KyInfo() << "Not carried sourcePath or sourcePath is not an array!";
        return KB_REQ_PARA_ERROR;
    }

    int maxHierarchy = 0;

    // 复制单个或者多个文件(文件夹)，都以数组的形式传入
    QJsonArray sourcePathArray = msgBody["sourcePath"].toArray();
    for (const QJsonValue &pathValue : sourcePathArray) {
        if (!pathValue.isString()) {
            KyInfo() << "Invalid source path";
            continue;
        }

        QString sourcePath = pathValue.toString();
        if (sourcePath.isEmpty()) {
            KyInfo() << "sourcePath is empty";
            continue;
        }

        KyInfo() << "Copy / Move file or directory from " << sourcePath << " to " << mDirectoryPath;

        // 从数据库获取源文件/文件夹信息
        KbDocumentItem *item = getKbDocumentItemByPath(sourcePath);
        if (item == nullptr) {
            KyInfo() << "Cannot find the item: " << sourcePath;
            continue;
        }

        if (item->isDir) {
            int currMaxHierarchyByPath = selectMaxHierarchyByPath(sourcePath);
            KyInfo() << "currMaxHierarchyByPath===" << currMaxHierarchyByPath;
            if(currMaxHierarchyByPath >= maxHierarchy){
                maxHierarchy = currMaxHierarchyByPath;
            }
        } else {
            continue;
        }
    }

    QJsonObject recvMsg;
    recvMsg["msgType"] = "sendToGetDirHierarchy";

    QJsonObject recvMsgbody;
    recvMsgbody["maxDirHierarchy"] = maxHierarchy;

    recvMsg["msgBody"] = recvMsgbody;

    return QJsonDocument(recvMsg).toJson();
}

QString FileListModel::sendToIsCanMove(const QJsonObject &data)
{
    if (!data.contains("msgBody")) {
        KyInfo() << "Not carried msgBody!";
        return KB_REQ_PARA_ERROR;
    }

    if (!data["msgBody"].isObject()) {
        KyInfo() << "The type is not an object!";
        return KB_REQ_PARA_ERROR;
    }

    QJsonObject msgBody = data["msgBody"].toObject();
    if(!msgBody.contains("sourcePath") || !msgBody["sourcePath"].isArray()) {
        KyInfo() << "Not carried sourcePath or sourcePath is not an array!";
        return KB_REQ_PARA_ERROR;
    }

    // 移动单个或者多个文件(文件夹)，都以数组的形式传入
    QJsonArray sourcePathArray = msgBody["sourcePath"].toArray();

    // 检验移动的文件夹是否移动到子文件夹和他本身的位置
    for (const QJsonValue &pathValue : sourcePathArray) {
        if (!pathValue.isString()) {
            KyInfo() << "Invalid source path";
            continue;
        }

        QString sourcePath = pathValue.toString();
        if (sourcePath.isEmpty()) {
            KyInfo() << "sourcePath is empty";
            continue;
        }

        KyInfo() << "check Move file or directory from " << sourcePath << " to " << mDirectoryPath;

        // 从数据库获取源文件/文件夹信息
        KbDocumentItem *item = getKbDocumentItemByPath(sourcePath);
        if (item == nullptr) {
            KyInfo() << "Cannot find the item: " << sourcePath;
            continue;
        }

        if (item->isDir) {
            bool isCanMoveFlag = checkDirIsCanMove(sourcePath);
            // 移动的文件夹不能移动到子文件夹和他本身的位置
            if(!isCanMoveFlag){
                QJsonObject recvMsg;
                recvMsg["msgType"] = "getIsCanMoveFlag";

                QJsonObject recvMsgbody;
                recvMsgbody["isCanMoveFlag"] = isCanMoveFlag;

                recvMsg["msgBody"] = recvMsgbody;

                return QJsonDocument(recvMsg).toJson();
            }
        } else {
            continue;
        }
    }
    return KB_RET_SUCCESS;
}

QString FileListModel::sendToGetRepeatFileNames(const QJsonObject &data)
{
    if (!data.contains("msgBody")) {
        KyInfo() << "Not carried msgBody!";
        return KB_REQ_PARA_ERROR;
    }

    if (!data["msgBody"].isObject()) {
        KyInfo() << "The type is not an object!";
        return KB_REQ_PARA_ERROR;
    }

    QJsonObject msgBody = data["msgBody"].toObject();
    if(!msgBody.contains("sourcePath") || !msgBody["sourcePath"].isArray()) {
        KyInfo() << "Not carried sourcePath or sourcePath is not an array!";
        return KB_REQ_PARA_ERROR;
    }

    // 移动单个或者多个文件(文件夹)，都以数组的形式传入
    QJsonArray sourcePathArray = msgBody["sourcePath"].toArray();

    QJsonArray jsonArray;

    QStringList documentTargetPathList = getAllFileNameByDocumentPath(mDirectoryPath);

    KyInfo() << "documentTargetPathList.size=="  <<documentTargetPathList.size() ;

    if(documentTargetPathList.size() > 0){
        foreach (const QString &path, documentTargetPathList) {
            KbDocumentItem *pathItem = getKbDocumentItemByPath(path);
            for (const QJsonValue &pathValue : sourcePathArray) {
                QString sourcePath = pathValue.toString();
                KbDocumentItem *pathValueItem = getKbDocumentItemByPath(sourcePath);
                // 处理每个路径，这里用 qDebug 打印示例
                KyInfo() << "当前文件名：" << pathItem->documentName << "---" << pathValueItem->documentName;
                if(pathItem->documentName == pathValueItem->documentName){
                    jsonArray.append(pathValue);
                }
            }
        }
    }
    if(jsonArray.size() == 0){
        return KB_RET_SUCCESS;
    }

    QJsonObject recvMsg;
    recvMsg["msgType"] = "sendToGetRepeatFileNames";

    QJsonObject recvMsgbody;
    recvMsgbody["repeatFileNames"] = jsonArray;

    recvMsg["msgBody"] = recvMsgbody;

    return QJsonDocument(recvMsg).toJson();
}

QString FileListModel::sendToGetAllFileNames(const QJsonObject &data)
{
    if (!data.contains("msgBody")) {
        KyInfo() << "Not carried msgBody!";
        return KB_REQ_PARA_ERROR;
    }

    if (!data["msgBody"].isObject()) {
        KyInfo() << "The type is not an object!";
        return KB_REQ_PARA_ERROR;
    }

    QJsonArray jsonArray;

    QStringList documentTargetPathList = getAllFileNameByDocumentPath(mDirectoryPath);

    KyInfo() << "sendToGetAllFileNames documentTargetPathList.size=="  <<documentTargetPathList.size() ;

    if(documentTargetPathList.size() > 0){
        foreach (const QString &path, documentTargetPathList) {
            KbDocumentItem *pathItem = getKbDocumentItemByPath(path);
            jsonArray.append(pathItem->documentTargetPath);
        }
    }
    if(jsonArray.size() == 0){
        return KB_RET_SUCCESS;
    }

    QJsonObject recvMsg;
    recvMsg["msgType"] = "sendToGetAllFileNames";

    QJsonObject recvMsgbody;
    recvMsgbody["allFileNames"] = jsonArray;

    recvMsg["msgBody"] = recvMsgbody;

    return QJsonDocument(recvMsg).toJson();
}

QString FileListModel::sendToCheckSpaceCapacity(const QJsonObject &data)
{
    if (!data.contains("msgBody")) {
        KyInfo() << "Not carried msgBody!";
        return KB_REQ_PARA_ERROR;
    }

    if (!data["msgBody"].isObject()) {
        KyInfo() << "The type is not an object!";
        return KB_REQ_PARA_ERROR;
    }

    // 上传文件传sourcePath  ， 复制文件传validFilePaths
    QJsonObject msgBody = data["msgBody"].toObject();

    QJsonObject recvMsg;
    recvMsg["msgType"] = "sendToCheckSpaceCapacity";

    // 上传单个或者多个文件，都以数组的形式传入
    QJsonArray sourcePathArray = msgBody["sourcePath"].toArray();

    // 复制单个或者多个文件，都以数组的形式传入
    QJsonArray validFilePaths = msgBody["validFilePaths"].toArray();

    qint64 totalFileSize = 0;
    // 上传文件容量校验
    for (const QJsonValue &pathValue : sourcePathArray) {
        if (!pathValue.isString()) {
            KyInfo() << "Invalid source path";
            continue;
        }

        QString sourcePath = pathValue.toString();
        if (sourcePath.isEmpty()) {
            KyInfo() << "sourcePath is empty";
            continue;
        }
        QFileInfo fileInfo = QFileInfo(sourcePath);
        totalFileSize = totalFileSize + fileInfo.size();
    }

    // 复制文件容量校验
    for (const QJsonValue &pathValue : validFilePaths) {
        if (!pathValue.isString()) {
            KyInfo() << "Invalid validFilePaths path";
            continue;
        }

        QString sourcePath = pathValue.toString();
        if (sourcePath.isEmpty()) {
            KyInfo() << "validFilePaths is empty";
            continue;
        }
        KbDocumentItem item = mDatabase->getItemByDocumentTargetPath(sourcePath);
        if(!item.isDir){
            totalFileSize = totalFileSize + item.documentSize;
        }
    }


    qint64 totel = KnowledgeBaseSpaceManager::getInstance()->getTotel();
    qint64 used = KnowledgeBaseSpaceManager::getInstance()->getUsed();

    KyInfo() << "totalFileSize==" << totalFileSize << "canUsed" << (totel - used);

    QJsonObject recvMsgbody;

    if(totalFileSize <= (totel - used)){
        recvMsgbody["spaceCapacityAccess"] = true;
    }else{
        recvMsgbody["spaceCapacityAccess"] = false;
    }
    recvMsg["msgBody"] = recvMsgbody;
    return QJsonDocument(recvMsg).toJson();
}

QString FileListModel::sendToGetAllFileCountByPath(const QJsonObject &data)
{
    if (!data.contains("msgBody")) {
        KyInfo() << "Not carried msgBody!";
        return KB_REQ_PARA_ERROR;
    }

    if (!data["msgBody"].isObject()) {
        KyInfo() << "The type is not an object!";
        return KB_REQ_PARA_ERROR;
    }

    QJsonObject msgBody = data["msgBody"].toObject();
    if(!msgBody.contains("sourcePath") || !msgBody["sourcePath"].isArray()) {
        KyInfo() << "Not carried sourcePath or sourcePath is not an array!";
        return KB_REQ_PARA_ERROR;
    }

    // 移动单个或者多个文件(文件夹)，都以数组的形式传入
    QJsonArray sourcePathArray = msgBody["sourcePath"].toArray();

    int totelCount = 0;

    for (const QJsonValue &pathValue : sourcePathArray) {
        QString sourcePath = pathValue.toString();
        KbDocumentItem *pathValueItem = getKbDocumentItemByPath(sourcePath);

        if(!pathValueItem->isDir){
            totelCount++;
        }else{
            totelCount = totelCount + getAllFileCountByDIr(pathValueItem->itemId);
        }
    }

    QJsonObject recvMsg;
    recvMsg["msgType"] = "sendToGetAllFileCountByPath";

    QJsonObject recvMsgbody;

    recvMsgbody["totelCount"] = totelCount;

    recvMsg["msgBody"] = recvMsgbody;
    return QJsonDocument(recvMsg).toJson();
}

QString FileListModel::sendToGetIsAllComplete(const QJsonObject &data)
{
    if (!data.contains("msgBody")) {
        KyInfo() << "Not carried msgBody!";
        return KB_REQ_PARA_ERROR;
    }

    if (!data["msgBody"].isObject()) {
        KyInfo() << "The type is not an object!";
        return KB_REQ_PARA_ERROR;
    }

    QJsonObject msgBody = data["msgBody"].toObject();
    if(!msgBody.contains("sourcePath") || !msgBody["sourcePath"].isArray()) {
        KyInfo() << "Not carried sourcePath or sourcePath is not an array!";
        return KB_REQ_PARA_ERROR;
    }

    bool isAllComplete = true;

    // 移动单个或者多个文件(文件夹)，都以数组的形式传入
    QJsonArray sourcePathArray = msgBody["sourcePath"].toArray();
    for (const QJsonValue &pathValue : sourcePathArray) {
        QString sourcePath = pathValue.toString();
        KbDocumentItem *pathValueItem = getKbDocumentItemByPath(sourcePath);
        if(pathValueItem->isDir){
            QList<int> itemList = mDatabase->selectAllChildDirItemIdByFilepath(pathValueItem->itemId);
            int count = mDatabase->selectFileCountByFilePath(itemList, false);
            if(count > 0){
                isAllComplete = false;
            }
        }else{
            if(pathValueItem->percent != 100){
                isAllComplete = false;
            }
        }
        if(!isAllComplete){
            break;
        }
    }

    QJsonObject recvMsg;
    recvMsg["msgType"] = "sendToGetIsAllComplete";

    QJsonObject recvMsgbody;

    recvMsgbody["isAllComplete"] = isAllComplete;

    recvMsg["msgBody"] = recvMsgbody;
    return QJsonDocument(recvMsg).toJson();
}

bool FileListModel::copySingleFile(const QString targetPath, const QString &sourcePath)
{
    KyInfo() << "copySingleFile: " << sourcePath;

    // 从数据库获取源文件信息
    KbDocumentItem *itemPtr = getKbDocumentItemByPath(sourcePath);
    if (itemPtr == nullptr) {
        KyInfo() << "Cannot find the file: " << sourcePath;
        return false;
    }

    KbDocumentItem sourceItem = *itemPtr;
    delete itemPtr;

    QFileInfo sourceInfo(sourcePath);
    QString fileName = sourceInfo.fileName();
    QString finalTargetPath = getDocumentTargetPathByNameAndTargetPath(targetPath, fileName);

    // 创建新的 item，用于复制
    KbDocumentItem newItem;
    newItem.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    newItem.documentName = fileName;
    newItem.documentPath = sourcePath; // 源路径
    newItem.documentTargetPath = finalTargetPath; // 目标路径
    newItem.documentSize = sourceItem.documentSize; // 继承原有文件大小
    newItem.createTime = getFileCreateTime(QFileInfo(finalTargetPath)); // 创建时间为当前时间
    newItem.documentSizeStr = sourceItem.documentSizeStr; // 继承原有文件大小字符串
    // 无论源文件状态如何，复制的文件都设置为 PENDING 状态，需要重新上传并显示进度
    newItem.status = PENDING; // 初始状态
    newItem.percent = 0;
    newItem.errorCode = 0;
    newItem.isDir = false;
    newItem.documentType = sourceItem.documentType; // 继承原有文件类型
    newItem.statusStr = getStatusStr(newItem.status);

    // 目标文件的ItemId
    int newTargetItemId = 0;
    if(targetPath == m_uploadRootPath){
        newTargetItemId = 0;
    }else{
        newTargetItemId = mDatabase->getItemIdByDocumentargetPath(targetPath);
    }

    // 添加数据库记录
    int newItemId = 0;
    bool insertRet = mDatabase->insertKnowledgeBaseInfo(newTargetItemId, newItem, &newItemId);
    if (!insertRet) {
        KyInfo() << "Failed to insert new record: " << finalTargetPath;
        return false;
    }

    newItem.itemId = newItemId;

    // 添加到列表
    newItem.fileIcon = getFileIconPath(sourcePath);

    int dirCnt = getDirCount();
    beginInsertRows(QModelIndex(), dirCnt, dirCnt);
    mFileList.insert(dirCnt, newItem);
    endInsertRows();

    Q_EMIT fileTotelCountyChanged();
    Q_EMIT countChanged();

    // 更新知识库空间容量
    KnowledgeBaseSpaceManager::getInstance()->addSpace(sourceItem.documentSize);

    // 提交给拷贝线程进行拷贝处理，会实时显示上传进度
    if (newItem.status == PENDING) {
        onCopyDocument(newItem);
    }

    return true;
}

bool FileListModel::copySingleDir(const QString targetPath, const QString &sourcePath)
{
    KyInfo() << "copySingleDir: " << sourcePath;

    // 从数据库获取源文件夹信息
    KbDocumentItem *itemPtr = getKbDocumentItemByPath(sourcePath);
    if (itemPtr == nullptr) {
        KyInfo() << "Cannot find the directory: " << sourcePath;
        return false;
    }

    KbDocumentItem sourceItem = *itemPtr;
    int sourceDirId = sourceItem.itemId;
    // 使用 getChildrenByParentIdRecursively 获取所有后代节点  提前查询子节点，防止创建文件夹后查询的子节点多一个文件夹目录
    QList<KbDocumentItem> allChildren = mDatabase->getChildrenByParentIdRecursively(sourceDirId);
    QString sourceDirPath = sourceItem.documentTargetPath;
    delete itemPtr;

    QFileInfo sourceInfo(sourcePath);
    QString dirName = sourceInfo.fileName();
    QString finalTargetPath = getDocumentTargetPathByNameAndTargetPath(targetPath, dirName);
    KyInfo() << "finalTargetPath====" << finalTargetPath;

    // 创建新的文件夹 item
    KbDocumentItem newItem;
    newItem.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    newItem.documentName = dirName;
    newItem.documentPath = sourcePath; // 源路径
    newItem.documentTargetPath = finalTargetPath; // 目标路径
    newItem.documentSize = 0; // 文件夹大小为0
    newItem.createTime = getFileCreateTime(QFileInfo(finalTargetPath)); // 创建时间为当前时间
    newItem.documentSizeStr = "0"; // 文件夹大小为0
    newItem.status = UPLOAD_COMPLETE; // 文件夹本身不会上传，状态设置为完成
    newItem.percent = 100; // 文件夹本身不会上传，百分比设置为100
    newItem.errorCode = 0;
    newItem.isDir = true;
    newItem.documentType = "inode-directory";
    newItem.statusStr = "";

    // 在磁盘上创建目标文件夹
    QDir targetDir(finalTargetPath);
    if (!targetDir.exists()) {
        bool mkdirRet = targetDir.mkpath(".");
        if (!mkdirRet) {
            KyInfo() << "Failed to create target directory: " << finalTargetPath;
            return false;
        }
    }

    int newTargetItemId = 0;
    if(targetPath == m_uploadRootPath){
        newTargetItemId = 0;
    }else{
        newTargetItemId = mDatabase->getItemIdByDocumentargetPath(targetPath);
    }

    // 添加数据库记录
    int newItemId = 0;
    bool insertRet = mDatabase->insertKnowledgeBaseInfo(newTargetItemId, newItem, &newItemId);
    if (!insertRet) {
        KyInfo() << "Failed to insert new record: " << finalTargetPath;
        return false;
    }

    newItem.itemId = newItemId;

    // 添加到列表
    newItem.fileIcon = getFileIconPath(sourcePath);

    beginInsertRows(QModelIndex(), 0, 0);
    mFileList.prepend(newItem);
    endInsertRows();

    Q_EMIT fileTotelCountyChanged();
    Q_EMIT countChanged();
    
    // 构建 itemId -> 节点信息的映射
    QMap<int, KbDocumentItem> descendantItemMap;
    for (const KbDocumentItem &item : allChildren) {
        descendantItemMap[item.itemId] = item;
    }
    
    // 调用递归处理函数
    copyDirChildrenRecursively(sourceDirId, newItemId, finalTargetPath, allChildren, descendantItemMap);

    return true;
}

void FileListModel::copyDirChildrenRecursively(int sourceDirId, int targetDirId, const QString &targetDirPath,
                                                const QList<KbDocumentItem> &allChildren,
                                                const QMap<int, KbDocumentItem> &descendantItemMap)
{
    // 构建 itemId -> parentId 的映射（通过路径关系确定）
    // 如果 childPath 的父目录路径等于 parentPath，则 parentId 是父节点
    QMap<int, int> parentIdMap; // itemId -> parentId
    for (auto it = descendantItemMap.begin(); it != descendantItemMap.end(); ++it) {
        int childItemId = it.key();
        QString childPath = it.value().documentTargetPath;
        QFileInfo childInfo(childPath);
        QString parentDirPath = childInfo.absolutePath();
        
        // 查找父节点：parentDirPath 对应的 itemId
        for (auto parentIt = descendantItemMap.begin(); parentIt != descendantItemMap.end(); ++parentIt) {
            if (parentIt.value().documentTargetPath == parentDirPath) {
                parentIdMap[childItemId] = parentIt.key();
                break;
            }
        }
        // 如果找不到，说明父节点是根文件夹
        if (!parentIdMap.contains(childItemId)) {
            parentIdMap[childItemId] = sourceDirId;
        }
    }
    
    // 构建 itemId -> 目标itemId 的映射（只包含文件夹）
    QMap<int, int> folderIdMap; // 源文件夹itemId -> 目标文件夹itemId
    folderIdMap[sourceDirId] = targetDirId;
    
    // 构建 itemId -> 目标路径的映射
    QMap<int, QString> targetPathMap; // 源itemId -> 目标路径
    targetPathMap[sourceDirId] = targetDirPath;
    
    // 按层级顺序处理：使用队列逐层处理
    QQueue<int> levelQueue;
    levelQueue.enqueue(sourceDirId);
    
    // 累计所有文件的大小，循环结束后一次性更新已使用空间
    qint64 totalFileSize = 0;
    
    while (!levelQueue.isEmpty()) {
        int currentSourceParentId = levelQueue.dequeue();
        int currentTargetParentId = folderIdMap[currentSourceParentId];
        QString currentTargetParentPath = targetPathMap[currentSourceParentId];
        
        // 从 allChildren 中筛选出当前父节点的直接子项
        QList<KbDocumentItem> directChildren;
        for (const KbDocumentItem &item : allChildren) {
            int itemParentId = parentIdMap.value(item.itemId, sourceDirId);
            if (itemParentId == currentSourceParentId) {
                directChildren.append(item);
            }
        }
        
        for (const KbDocumentItem &child : directChildren) {
            QString childSourcePath = child.documentTargetPath;
            QFileInfo childInfo(childSourcePath);
            QString childName = childInfo.fileName();
            
            // 直接计算目标路径，不依赖 mDirectoryPath
            QString childTargetPath = currentTargetParentPath + "/" + childName;
            
            // 设置 isDir（getChildrenByParentIdRecursively 返回的节点可能没有设置 isDir）
            KbDocumentItem childWithDir = child;
            childWithDir.isDir = childInfo.isDir();
            
            if (childWithDir.isDir) {
                // 复制子文件夹
                // 在磁盘上创建目标文件夹
                QDir targetChildDir(childTargetPath);
                if (!targetChildDir.exists()) {
                    bool mkdirRet = targetChildDir.mkpath(".");
                    if (!mkdirRet) {
                        KyInfo() << "Failed to create target child directory: " << childTargetPath;
                        continue;
                    }
                }
                
                // 创建新的文件夹 item
                KbDocumentItem newChildItem;
                newChildItem.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
                newChildItem.documentName = childName;
                newChildItem.documentPath = childSourcePath;
                newChildItem.documentTargetPath = childTargetPath;
                newChildItem.documentSize = 0;
                newChildItem.createTime = getFileCreateTime(QFileInfo(childTargetPath));
                newChildItem.documentSizeStr = "0";
                newChildItem.status = UPLOAD_COMPLETE;
                newChildItem.percent = 100;
                newChildItem.errorCode = 0;
                newChildItem.isDir = true;
                newChildItem.documentType = "inode-directory";
                newChildItem.statusStr = "";
                
                // 添加数据库记录，使用 currentTargetParentId 作为 parentId
                int newChildItemId = 0;
                bool insertRet = mDatabase->insertKnowledgeBaseInfo(currentTargetParentId, newChildItem, &newChildItemId);
                if (!insertRet) {
                    KyInfo() << "Failed to insert new child folder record: " << childTargetPath;
                    continue;
                }
                
                newChildItem.itemId = newChildItemId;
                
                // 记录映射关系，加入队列继续处理
                folderIdMap[child.itemId] = newChildItemId;
                targetPathMap[child.itemId] = childTargetPath;
                levelQueue.enqueue(child.itemId);
            } else {
                // 批量复制子文件：直接创建文件记录和提交复制任务
                // 确保父文件夹存在
                QFileInfo targetFileInfo(childTargetPath);
                QDir targetParentDir = targetFileInfo.absoluteDir();
                if (!targetParentDir.exists()) {
                    bool mkdirRet = targetParentDir.mkpath(".");
                    if (!mkdirRet) {
                        KyInfo() << "Failed to create parent directory for file: " << childTargetPath;
                        continue;
                    }
                }
                
                // 创建新的文件 item
                KbDocumentItem newFileItem;
                newFileItem.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
                newFileItem.documentName = childName;
                newFileItem.documentPath = childSourcePath;
                newFileItem.documentTargetPath = childTargetPath;
                newFileItem.documentSize = child.documentSize; // 继承原有文件大小
                newFileItem.createTime = getFileCreateTime(QFileInfo(childTargetPath)); // 创建时间为当前时间
                newFileItem.documentSizeStr = child.documentSizeStr; // 继承原有文件大小字符串
                // 无论源文件状态如何，复制的文件都设置为 PENDING 状态，需要重新上传并显示进度
                newFileItem.status = PENDING; // 初始状态
                newFileItem.percent = 0;
                newFileItem.errorCode = 0;
                newFileItem.isDir = false;
                newFileItem.documentType = child.documentType; // 继承原有文件类型
                newFileItem.statusStr = getStatusStr(newFileItem.status);
                
                // 添加数据库记录，使用 currentTargetParentId 作为 parentId
                int newFileItemId = 0;
                bool insertRet = mDatabase->insertKnowledgeBaseInfo(currentTargetParentId, newFileItem, &newFileItemId);
                if (!insertRet) {
                    KyInfo() << "Failed to insert new file record: " << childTargetPath;
                    continue;
                }
                
                newFileItem.itemId = newFileItemId;
                
                // 累计文件大小，循环结束后一次性更新
                totalFileSize += child.documentSize;
                
                // 提交给拷贝线程进行拷贝处理，会实时显示上传进度
                if (newFileItem.status == PENDING) {
                    onCopyDocument(newFileItem);
                }
            }
            Q_EMIT fileTotelCountyChanged();
        }
    }
    
    // 循环结束后，一次性更新知识库空间容量
    if (totalFileSize > 0) {
        KnowledgeBaseSpaceManager::getInstance()->addSpace(totalFileSize);
    }
}
