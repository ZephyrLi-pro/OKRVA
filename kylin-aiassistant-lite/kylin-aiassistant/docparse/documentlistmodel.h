#ifndef DOCUMENTLISTMODEL_H
#define DOCUMENTLISTMODEL_H

#include <QAbstractListModel>
#include <QObject>
#include <QList>
#include <QModelIndex>
#include <QVariant>

#include "global/kaglobal.h"

class DocumentListModel : public QAbstractListModel
{
    Q_OBJECT
public:

    enum DocumentRoleType {
        DocumentNameRole = Qt::UserRole + 1,
        DocumentPathRole,
        DocumentSizeRole,
        DocumentTypeRole
    };

    Q_ENUM(DocumentRoleType)

    explicit DocumentListModel(QObject *parent = nullptr);
    ~DocumentListModel();

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QModelIndex index(int row, int column = 0, const QModelIndex &parent = QModelIndex()) const override;

    QList<DocumentItem*> getVisibleDocs(int start, int end) const;
    void addDocument(DocumentItem *document);
    void addDocuments(const QList<DocumentItem *> &documents);
    void deleteDocument(const QString &documentPath);
    QList<DocumentItem*> getDocumentList() const;
    DocumentItem* getDocumentItem(const QString &documentPath) const;
    int getDocumentCount() const;
    bool hasDocument(const QString &documentPath) const;
    void clearData();

private:
    bool isMaximumReached() const;

private:
    QList<DocumentItem *> m_documentList;
    static const int MAX_DOCUMENT_COUNT = 1;
};

#endif // DOCUMENTLISTMODEL_H
