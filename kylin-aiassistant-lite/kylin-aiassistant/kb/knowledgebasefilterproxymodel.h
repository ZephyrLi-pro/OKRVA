#ifndef KNOWLEDGEBASEFILTERPROXYMODEL_H
#define KNOWLEDGEBASEFILTERPROXYMODEL_H

#include <QSortFilterProxyModel>
#include <QSet>
#include <QDateTime>
#include <QModelIndex>

class KnowledgeBaseFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)
    Q_PROPERTY(int fileTypeFilter READ fileTypeFilter WRITE setFileTypeFilter NOTIFY fileTypeFilterChanged)
    Q_PROPERTY(int statusFilter READ statusFilter WRITE setStatusFilter NOTIFY statusFilterChanged)
    Q_PROPERTY(bool showDirectories READ showDirectories WRITE setShowDirectories NOTIFY showDirectoriesChanged)
    Q_PROPERTY(bool showFiles READ showFiles WRITE setShowFiles NOTIFY showFilesChanged)
    Q_PROPERTY(bool foldersFirst READ foldersFirst WRITE setFoldersFirst NOTIFY foldersFirstChanged)

public:
    explicit KnowledgeBaseFilterProxyModel(QObject *parent = nullptr);

    enum FileTypeFilter {
        AllFiles = 0,
        Documents,     // doc, docx, pdf
        TXTs,          // txt
        WPS,           // wps
        OtherFiles
    };
    Q_ENUM(FileTypeFilter)

    enum StatusFilter {
        AllStatus = 0,
        Uploading,     // COPYING, COPY_COMPLETE, UPLOADING
        Uploaded,      // UPLOAD_COMPLETE
        Error,         // COPY_ERROR, UPLOAD_ERROR
        Pending        // PENDING
    };
    Q_ENUM(StatusFilter)

    // 属性访问器
    QString searchText() const;
    void setSearchText(const QString &text);

    int fileTypeFilter() const;
    void setFileTypeFilter(int filter);

    int statusFilter() const;
    void setStatusFilter(int filter);

    bool showDirectories() const;
    void setShowDirectories(bool show);

    bool showFiles() const;
    void setShowFiles(bool show);

    bool foldersFirst() const;
    void setFoldersFirst(bool first);

    // QML 调用方法
    Q_INVOKABLE void clearFilters();
    Q_INVOKABLE QString getFileExtension(const QString &fileName) const;
    Q_INVOKABLE bool isUploadingStatus(int status) const;
    Q_INVOKABLE bool isErrorStatus(int status) const;
    Q_INVOKABLE QDateTime getCreateTime(const QModelIndex &proxyIndex) const;
    Q_INVOKABLE bool setProperty(int proxyRow, const QString &propertyName, const QVariant &value) const;
    Q_INVOKABLE QVariantMap getPropertyByIndex(int proxyRow) const;

signals:
    void searchTextChanged();
    void fileTypeFilterChanged();
    void statusFilterChanged();
    void showDirectoriesChanged();
    void showFilesChanged();
    void foldersFirstChanged();
    void filterCountChanged(int filteredCount, int totalCount);

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;
    bool lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const override;

private:
    // 过滤条件
    QString m_searchText;
    int m_fileTypeFilter;
    int m_statusFilter;
    bool m_showDirectories;
    bool m_showFiles;
    bool m_foldersFirst;

    // 辅助方法
    bool matchesSearchText(const QString &documentName) const;
    bool matchesFileType(const QString &documentType, const QString &documentName) const;
    bool matchesStatus(int status) const;

    // 文件类型分组
    QSet<QString> m_documentTypes;
    QSet<QString> m_txtTypes;
    QSet<QString> m_wpsTypes;

    void initializeFileTypeSets();

    // 创建时间比较（始终按此排序）
    int compareByCreateTime(const QModelIndex &left, const QModelIndex &right) const;
};

#endif // KNOWLEDGEBASEFILTERPROXYMODEL_H
