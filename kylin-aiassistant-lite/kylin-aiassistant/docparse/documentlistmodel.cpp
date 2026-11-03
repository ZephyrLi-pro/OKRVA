#include "documentlistmodel.h"
#include <QDebug>

DocumentListModel::DocumentListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

DocumentListModel::~DocumentListModel()
{
    qDeleteAll(m_documentList);
    m_documentList.clear();
}

bool DocumentListModel::isMaximumReached() const
{
    return m_documentList.size() >= MAX_DOCUMENT_COUNT;
}

int DocumentListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
    {
        return 0;
    }

    return m_documentList.size();
}

QVariant DocumentListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_documentList.size())
        return QVariant();

    DocumentItem *item = m_documentList.at(index.row());
    if (item == nullptr)
    {
        return QVariant();
    }

    switch (role) {
    case DocumentNameRole:
        return item->documentName;
    case DocumentPathRole:
        return item->documentPath;
    case DocumentSizeRole:
        return item->documentSize;
    case DocumentTypeRole:
        return item->documentType;
    default:
        break;
    }

    return QVariant();
}

QModelIndex DocumentListModel::index(int row, int column, const QModelIndex &parent) const {
    if (row < 0 || row >= m_documentList.size() || parent.isValid())
        return QModelIndex();

    return createIndex(row, column, m_documentList.at(row));
}

QList<DocumentItem*> DocumentListModel::getVisibleDocs(int start, int end) const
{
    QList<DocumentItem*> result;
    start = qMax(0, start);
    end = qMin(end, m_documentList.size() - 1);
    for (int i = start; i <= end; ++i) {
        result.append(m_documentList[i]);
    }
    return result;
}

void DocumentListModel::addDocument(DocumentItem *document)
{
    if (document == nullptr || isMaximumReached())
        return;

    beginInsertRows(QModelIndex(), m_documentList.size(), m_documentList.size());
    m_documentList.append(document);
    endInsertRows();
}

void DocumentListModel::addDocuments(const QList<DocumentItem *> &documents)
{
    if (documents.isEmpty() || isMaximumReached()) {
        return;
    }

    // 只添加第一个文件（如果超过限制）
    QList<DocumentItem *> singleDocument;
    singleDocument.append(documents.first());

    beginInsertRows(QModelIndex(), m_documentList.size(), m_documentList.size() + singleDocument.size() - 1);
    m_documentList.append(singleDocument);
    endInsertRows();
}

void DocumentListModel::deleteDocument(const QString &documentPath)
{
    for (int i = 0; i < m_documentList.size(); ++i) {
        DocumentItem *item = m_documentList[i];
        if (item && item->documentPath == documentPath) {
            beginRemoveRows(QModelIndex(), i, i);
            delete item;
            m_documentList.removeAt(i);
            endRemoveRows();
            break;
        }
    }
}

QList<DocumentItem*> DocumentListModel::getDocumentList() const
{
    return m_documentList;
}

DocumentItem* DocumentListModel::getDocumentItem(const QString &documentPath) const
{
    for (DocumentItem *item : m_documentList) {
        if (item && item->documentPath == documentPath) {
            return item;
        }
    }
    return nullptr;
}

int DocumentListModel::getDocumentCount() const
{
    return m_documentList.size();
}

bool DocumentListModel::hasDocument(const QString &documentPath) const
{
    for (DocumentItem *item : m_documentList) {
        if (item && item->documentPath == documentPath) {
            return true;
        }
    }
    return false;
}

void DocumentListModel::clearData()
{
    beginResetModel();
    qDeleteAll(m_documentList);
    m_documentList.clear();
    endResetModel();
}
