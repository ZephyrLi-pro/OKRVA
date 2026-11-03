#ifndef MODELITEMLISTMODEL_H
#define MODELITEMLISTMODEL_H

#include <QObject>
#include <QAbstractListModel>

struct ModelItem
{
    QString m_modelName;
    QString m_description;
    QString m_type;
    QString m_iconPath;
    QString m_icomPathWhite;
    bool m_isChecked = false;
    bool m_isSupportsThinking = false;
    bool m_isSupportsThinkingToggle = false;
    bool m_isSupportsWebSearch = false;
    bool m_isModelEnabled = true;       // 模型是否可用（是否置灰）
    bool m_isSharedModel = false;       // 是否是共享模型
    int m_deployType;
};

class ModelItemListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum ModelRoles {
        NameRole = Qt::UserRole + 1,
        DescriptionRole,
        TypeRole,
        IconPathRole,
        IconPathWhiteRole,
        IsCheckedRole,
        IsSupportsThinkingRole,
        IsSupportsThinkingToggleRole,
        IsSupportsWebSearchRole,
        IsModelEnabledRole,             // 模型是否可用
        IsSharedModelRole               // 是否是共享模型
    };

    explicit ModelItemListModel(QObject *parent = nullptr);
    ~ModelItemListModel();
    static ModelItemListModel *getInstance();

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    Q_INVOKABLE QVariantMap get(int index) const;

    void setItems(const QList<ModelItem*>& items);
    QHash<int, QByteArray> roleNames() const override;

private:
    QList<ModelItem*> m_items;
    static ModelItemListModel * m_ptr;
};

#endif // MODELITEMLISTMODEL_H
