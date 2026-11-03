#ifndef SELECTCOMMANDLISTMODEL_H
#define SELECTCOMMANDLISTMODEL_H

#include <QAbstractListModel>
#include <QObject>

struct SelectCommandItem {
    int m_actorId;
    QString m_name;
    QString m_iconPath;
    QString m_description;
    bool m_isActive;
};

class SelectCommandListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum SelectCommandRoleType {
        NameRole = Qt::UserRole + 1,
        ActorRole,
        IconRole,
        DescRole,
        IsActiveRole
    };

    Q_ENUM(SelectCommandRoleType)

    explicit SelectCommandListModel(QObject *parent = nullptr);
    static SelectCommandListModel *getInstance();
    static SelectCommandListModel *selectCommandListModelInstance;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QVariant getData(const int row, int role) const;
    Q_INVOKABLE int count() const;

private:
    void initDefaultCommands();
    QList<SelectCommandItem> m_commandList;
};

#endif // SELECTCOMMANDLISTMODEL_H

