#include "filelistmodel.h"
#include "knowledgebasefilterproxymodel.h"
#include <QFileInfo>
#include <QDateTime>
#include <QDebug>
#include <QCollator>

KnowledgeBaseFilterProxyModel::KnowledgeBaseFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
    , m_searchText("")
    , m_fileTypeFilter(AllFiles)
    , m_statusFilter(AllStatus)
    , m_showDirectories(true)
    , m_showFiles(true)
    , m_foldersFirst(true)  // 默认文件夹优先
{
    initializeFileTypeSets();

    // 设置动态排序，始终按创建时间排序
    setDynamicSortFilter(true);
    setFilterCaseSensitivity(Qt::CaseInsensitive);
    // fix BUG: #501493 批量添加文件，文件排序顺序与知识库管理中不一致
    // 代理重新排序导致与源model顺序不一致，禁用此处排序
//    setSortRole(FileListModel::CreateTimeRole); // 设置按创建时间排序
//    sort(0, Qt::DescendingOrder); // 默认按时间倒序（最新的在前）

    // 连接信号，当过滤条件变化时通知外部
    connect(this, &QSortFilterProxyModel::rowsInserted, this, [this]() {
        emit filterCountChanged(rowCount(), sourceModel() ? sourceModel()->rowCount() : 0);
    });
    connect(this, &QSortFilterProxyModel::rowsRemoved, this, [this]() {
        emit filterCountChanged(rowCount(), sourceModel() ? sourceModel()->rowCount() : 0);
    });
    connect(this, &QSortFilterProxyModel::modelReset, this, [this]() {
        emit filterCountChanged(rowCount(), sourceModel() ? sourceModel()->rowCount() : 0);
    });
    connect(this, &QSortFilterProxyModel::layoutChanged, this, [this]() {
        emit filterCountChanged(rowCount(), sourceModel() ? sourceModel()->rowCount() : 0);
    });
}

void KnowledgeBaseFilterProxyModel::initializeFileTypeSets()
{
    // DOC, DOCX, PDF 文档类型
    m_documentTypes = {"application/msword",  // .doc
                       "application/vnd.openxmlformats-officedocument.wordprocessingml.document",  // .docx
                       "application/pdf",  // .pdf
                       "application/wps-office.doc",  // WPS文档
                       "application/encrypted"};  // 加密文档

    // TXT 文本文件类型
    m_txtTypes = {"text/plain",  // .txt
                  "inode/x-empty"};  // 空文件

    // WPS类型
    m_wpsTypes = {"application/vnd.ms-works"};
}

QString KnowledgeBaseFilterProxyModel::searchText() const
{
    return m_searchText;
}

void KnowledgeBaseFilterProxyModel::setSearchText(const QString &text)
{
    if (m_searchText != text) {
        m_searchText = text;
        invalidateFilter();
        emit searchTextChanged();
        emit filterCountChanged(rowCount(), sourceModel() ? sourceModel()->rowCount() : 0);
    }
}

int KnowledgeBaseFilterProxyModel::fileTypeFilter() const
{
    return m_fileTypeFilter;
}

void KnowledgeBaseFilterProxyModel::setFileTypeFilter(int filter)
{
    if (m_fileTypeFilter != filter) {
        m_fileTypeFilter = filter;
        invalidateFilter();
        emit fileTypeFilterChanged();
        emit filterCountChanged(rowCount(), sourceModel() ? sourceModel()->rowCount() : 0);
    }
}

int KnowledgeBaseFilterProxyModel::statusFilter() const
{
    return m_statusFilter;
}

void KnowledgeBaseFilterProxyModel::setStatusFilter(int filter)
{
    if (m_statusFilter != filter) {
        m_statusFilter = filter;
        invalidateFilter();
        emit statusFilterChanged();
        emit filterCountChanged(rowCount(), sourceModel() ? sourceModel()->rowCount() : 0);
    }
}

bool KnowledgeBaseFilterProxyModel::showDirectories() const
{
    return m_showDirectories;
}

void KnowledgeBaseFilterProxyModel::setShowDirectories(bool show)
{
    if (m_showDirectories != show) {
        m_showDirectories = show;
        invalidateFilter();
        emit showDirectoriesChanged();
        emit filterCountChanged(rowCount(), sourceModel() ? sourceModel()->rowCount() : 0);
    }
}

bool KnowledgeBaseFilterProxyModel::showFiles() const
{
    return m_showFiles;
}

void KnowledgeBaseFilterProxyModel::setShowFiles(bool show)
{
    if (m_showFiles != show) {
        m_showFiles = show;
        invalidateFilter();
        emit showFilesChanged();
        emit filterCountChanged(rowCount(), sourceModel() ? sourceModel()->rowCount() : 0);
    }
}

bool KnowledgeBaseFilterProxyModel::foldersFirst() const
{
    return m_foldersFirst;
}

void KnowledgeBaseFilterProxyModel::setFoldersFirst(bool first)
{
    if (m_foldersFirst != first) {
        m_foldersFirst = first;
        invalidate(); // 重新排序
        emit foldersFirstChanged();
    }
}

void KnowledgeBaseFilterProxyModel::clearFilters()
{
    setSearchText("");
    setFileTypeFilter(AllFiles);
    setStatusFilter(AllStatus);
    setShowDirectories(true);
    setShowFiles(true);
}

QString KnowledgeBaseFilterProxyModel::getFileExtension(const QString &fileName) const
{
    QFileInfo fileInfo(fileName);
    return fileInfo.suffix().toLower();
}

bool KnowledgeBaseFilterProxyModel::isUploadingStatus(int status) const
{
    // COPYING = 1, COPY_COMPLETE = 2, UPLOADING = 3
    return status >= 1 && status <= 3;
}

bool KnowledgeBaseFilterProxyModel::isErrorStatus(int status) const
{
    // COPY_ERROR = 4, UPLOAD_ERROR = 5
    return status == 4 || status == 5;
}

QDateTime KnowledgeBaseFilterProxyModel::getCreateTime(const QModelIndex &proxyIndex) const
{
    if (!proxyIndex.isValid()) {
        return QDateTime();
    }

    QModelIndex sourceIndex = mapToSource(proxyIndex);
    if (!sourceIndex.isValid() || !sourceModel()) {
        return QDateTime();
    }

    QString dateStr = sourceModel()->data(sourceIndex, FileListModel::CreateTimeRole).toString();
    return QDateTime::fromString(dateStr, "yyyy/MM/dd HH:mm:ss");
}

bool KnowledgeBaseFilterProxyModel::setProperty(int proxyRow, const QString &propertyName, const QVariant &value) const
{
    if (!sourceModel() || proxyRow < 0 || proxyRow >= rowCount()) {
        return false;
    }

    // 映射到源模型索引
    QModelIndex proxyIndex = index(proxyRow, 0);
    QModelIndex sourceIndex = mapToSource(proxyIndex);

    // 根据属性名确定角色
    int role = -1;
    if (propertyName == "checked") {
        role = FileListModel::Checked;
    } else if (propertyName == "documentName") {
        role = FileListModel::DocumentNameRole;
    }
    // 添加其他属性...

    if (role != -1) {
        // 使用源模型的 setData
        return sourceModel()->setData(sourceIndex, value, role);
    }

    return false;
}

QVariantMap KnowledgeBaseFilterProxyModel::getPropertyByIndex(int proxyRow) const
{
    if (!sourceModel() || proxyRow < 0 || proxyRow >= rowCount()) {
        return QVariantMap();
    }
    QVariantMap result;
    QModelIndex proxyIndex = index(proxyRow, 0);
    result["uuid"] = data(proxyIndex, FileListModel::UuidRole);
    result["documentName"] = data(proxyIndex, FileListModel::DocumentNameRole);
    result["documentTargetPath"] = data(proxyIndex, FileListModel::DocumentTargetPathRole);
    result["documentSize"] = data(proxyIndex, FileListModel::DocumentSizeStrRole);
    result["fileIcon"] = data(proxyIndex, FileListModel::FileIconRole);
    result["checked"] = data(proxyIndex, FileListModel::Checked);
    result["isDir"] = data(proxyIndex, FileListModel::IsDirRole);

    return result;
}
bool KnowledgeBaseFilterProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    if (!sourceModel()) {
        return false;
    }

    QModelIndex index = sourceModel()->index(source_row, 0, source_parent);

    // 获取数据
    QString documentName = sourceModel()->data(index, FileListModel::DocumentNameRole).toString();
    QString documentPath = sourceModel()->data(index, FileListModel::DocumentPathRole).toString();
    QString documentType = sourceModel()->data(index, FileListModel::DocumentTypeRole).toString();
    int status = sourceModel()->data(index, FileListModel::StatusRole).toInt();
    bool isDir = sourceModel()->data(index, FileListModel::IsDirRole).toBool();

    // 1. 目录/文件显示控制
    if (isDir && !m_showDirectories) {
        return false;
    }

    if (!isDir && !m_showFiles) {
        return false;
    }

    // 2. 搜索文本过滤
    if (!m_searchText.isEmpty() && !matchesSearchText(documentName)) {
        return false;
    }

    // 3. 文件类型过滤（仅对文件有效）
    if (!isDir && m_fileTypeFilter != AllFiles && !matchesFileType(documentType, documentName)) {
        return false;
    }

    // 4. 状态过滤
    if (m_statusFilter != AllStatus && !matchesStatus(status)) {
        return false;
    }

    return true;
}

bool KnowledgeBaseFilterProxyModel::matchesSearchText(const QString &documentName) const
{
    if (m_searchText.isEmpty()) {
        return true;
    }

    // 搜索文件名或路径
    return documentName.contains(m_searchText, Qt::CaseInsensitive);
}

bool KnowledgeBaseFilterProxyModel::matchesFileType(const QString &documentType, const QString &documentName) const
{
    switch (m_fileTypeFilter) {
    case Documents:
        return m_documentTypes.contains(documentType) ||
               getFileExtension(documentName) == "doc" ||
               getFileExtension(documentName) == "docx" ||
               getFileExtension(documentName) == "pdf";
    case TXTs:
        return m_txtTypes.contains(documentType) ||
               getFileExtension(documentName) == "txt";
    case WPS:
        return m_wpsTypes.contains(documentType) ||
               getFileExtension(documentName) == "wps";
    case OtherFiles:
        return !m_documentTypes.contains(documentType) &&
               !m_txtTypes.contains(documentType) &&
               !m_wpsTypes.contains(documentType) &&
               getFileExtension(documentName) != "doc" &&
               getFileExtension(documentName) != "docx" &&
               getFileExtension(documentName) != "txt" &&
               getFileExtension(documentName) != "pdf" &&
               getFileExtension(documentName) != "wps";
    default:
        return true;
    }
}

bool KnowledgeBaseFilterProxyModel::matchesStatus(int status) const
{
    switch (m_statusFilter) {
    case Uploading:
        return isUploadingStatus(status); // COPYING, COPY_COMPLETE, UPLOADING
    case Uploaded:
        return status == 6; // UPLOAD_COMPLETE
    case Error:
        return isErrorStatus(status); // COPY_ERROR, UPLOAD_ERROR
    case Pending:
        return status == 0; // PENDING
    default:
        return true;
    }
}

bool KnowledgeBaseFilterProxyModel::lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const
{
    if (!sourceModel()) {
        return false;
    }

    bool leftIsDir = sourceModel()->data(source_left, FileListModel::IsDirRole).toBool();
    bool rightIsDir = sourceModel()->data(source_right, FileListModel::IsDirRole).toBool();

    // 如果启用文件夹优先，则目录排在前面
    if (m_foldersFirst) {
        if (leftIsDir && !rightIsDir) return false;   // 左边是目录，右边是文件 => 目录在前
        if (!leftIsDir && rightIsDir) return true;  // 左边是文件，右边是目录 => 目录在前
    }

    // 按创建时间比较（默认最新的在前）
    return compareByCreateTime(source_left, source_right) < 0;
}

int KnowledgeBaseFilterProxyModel::compareByCreateTime(const QModelIndex &left, const QModelIndex &right) const
{
    QString leftDateStr = sourceModel()->data(left, FileListModel::CreateTimeRole).toString();
    QString rightDateStr = sourceModel()->data(right, FileListModel::CreateTimeRole).toString();

    QDateTime leftDate = QDateTime::fromString(leftDateStr, "yyyy/MM/dd HH:mm:ss");
    QDateTime rightDate = QDateTime::fromString(rightDateStr, "yyyy/MM/dd HH:mm:ss");

    if (leftDate < rightDate) return -1;
    if (leftDate > rightDate) return 1;

    // 如果创建时间相同，则按名称排序以确保稳定排序
    QString leftName = sourceModel()->data(left, FileListModel::DocumentNameRole).toString();
    QString rightName = sourceModel()->data(right, FileListModel::DocumentNameRole).toString();

    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    return collator.compare(leftName, rightName);
}
